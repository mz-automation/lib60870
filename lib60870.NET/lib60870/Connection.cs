using System;

using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;

namespace lib60870
{




	/// <summary>
	/// ASDU received handler.
	/// </summary>
	public delegate bool ASDUReceivedHandler (object parameter, ASDU asdu);

	public class Connection
	{
		static byte[] STARTDT_ACT_MSG = new byte[] { 0x68, 0x04, 0x07, 0x00, 0x00, 0x00 };

		static byte[] TESTFR_CON_MSG = new byte[] { 0x68, 0x04, 0x83, 0x00, 0x00, 0x00 };

		private int sendCount = 0;
		private int receiveCount = 0;

		private int unconfirmedMessages = 0; /* number of unconfirmed messages received */
		private long lastConfirmationTime = System.Int64.MaxValue; /* timestamp when the last confirmation message was sent */

        private Socket socket;

		private bool autostart = true;

		private string hostname;
		private int tcpPort;

		private bool running = false;

		private bool debugOutput = false;

		public bool DebugOutput {
			get {
				return this.debugOutput;
			}
			set {
				debugOutput = value;
			}
		}

		private ConnectionParameters parameters;

		ASDUReceivedHandler asduReceivedHandler = null;
		object asduReceivedHandlerParameter = null;

		private void sendSMessage() {
			byte[] msg = new byte[6];

			msg [0] = 0x68;
			msg [1] = 0x04;
			msg [2] = 0x01;
			msg [3] = 0;
			msg [4] = (byte) ((receiveCount % 128) * 2);
			msg [5] = (byte) (receiveCount / 128);

			socket.Send (msg);
		}


		private void sendIMessage(Frame frame) 
		{
			frame.PrepareToSend (sendCount, receiveCount);
			socket.Send (frame.GetBuffer (), frame.GetMsgSize (), SocketFlags.None);
			sendCount++;
		}

		private void setup(string hostname, ConnectionParameters parameters)
		{
			this.hostname = hostname;
			this.parameters = parameters;
			this.tcpPort = parameters.TcpPort;
		}

		public Connection (string hostname)
		{
			setup (hostname, new ConnectionParameters());
		}

		public Connection (string hostname, ConnectionParameters parameters)
		{
			setup (hostname, parameters.clone());
		}


		private void EncodeIdentificationField(Frame frame, TypeID typeId, 
		                                       int vsq, CauseOfTransmission cot, int ca) 
		{
			frame.SetNextByte ((byte) typeId);
			frame.SetNextByte ((byte) vsq); /* SQ:false; NumIX:1 */

			/* encode COT */
			frame.SetNextByte ((byte) cot);  
			if (parameters.SizeOfCOT == 2)
				frame.SetNextByte ((byte) parameters.OriginatorAddress);

			/* encode CA */
			frame.SetNextByte ((byte)(ca & 0xff));
			if (parameters.SizeOfCA == 2) 
				frame.SetNextByte ((byte) ((ca & 0xff00) >> 8));
		}

		public void sendInterrogationCommand(CauseOfTransmission cot, int ca, byte qoi) 
		{
			Frame frame = new T104Frame ();

			EncodeIdentificationField (frame, TypeID.C_IC_NA_1, 1, cot, ca);

			/* encode IOA */
			frame.SetNextByte ((byte)0);

			if (parameters.SizeOfIOA > 1)
				frame.SetNextByte ((byte)0);

			if (parameters.SizeOfIOA > 2)
				frame.SetNextByte ((byte)0);

			/* encode COI (7.2.6.21) */
			frame.SetNextByte (qoi); /* 20 = station interrogation */

			Console.WriteLine("Encoded C_IC_NA_1 with " + frame.GetMsgSize() + " bytes.");

			sendIMessage (frame);
		}

	
		public void sendControlCommand(TypeID typeId, CauseOfTransmission cot, int ca, InformationObject sc) {
			Frame frame = new T104Frame ();

			EncodeIdentificationField (frame, typeId, 1 /* SQ:false; NumIX:1 */, cot, ca);

			sc.Encode (frame, parameters);

			Console.WriteLine("Encoded " +  typeId.ToString() + " with " + frame.GetMsgSize() + " bytes.");

			sendIMessage (frame);
		}

		public void connect() {
			Thread workerThread = new Thread(HandleConnection);

			workerThread.Start ();
		}

		private int receiveMessage(Socket socket, byte[] buffer) 
		{
			//TODO add timeout handling

			// wait for first byte
			if (socket.Receive (buffer, 0, 1, SocketFlags.None) != 1)
				return 0;

			if (buffer [0] != 0x68) {
				Console.WriteLine ("Missing SOF indicator!");
				return 0;
			}

			// read length byte
			if (socket.Receive (buffer, 1, 1, SocketFlags.None) != 1)
				return 0;

			int length = buffer [1];

			// read remaining frame
			if (socket.Receive (buffer, 2, length, SocketFlags.None) != length) {
				Console.WriteLine ("Failed to read complete frame!");
				return 0;
			}

			return length + 2;
		}

        private bool checkConfirmTimeout(long currentTime) 
        {
            if ((currentTime - lastConfirmationTime) >= (parameters.T2 * 1000))
                return true;
            else
                return false;
        }

		private bool checkMessage(Socket socket, byte[] buffer, int msgSize)
		{

			if ((buffer [2] & 1) == 0) {
			
				if (debugOutput)
					Console.WriteLine ("Received I frame");

				if (msgSize < 7) {

					if (debugOutput)
						Console.WriteLine ("I msg too small!");

					return false;
				}

				receiveCount++;
				unconfirmedMessages++;

                long currentTime = SystemUtils.currentTimeMillis();

				if ((unconfirmedMessages > parameters.W) || checkConfirmTimeout(currentTime)) {
					//TODO check timeout condition /* t2 */
                    lastConfirmationTime = currentTime;

					unconfirmedMessages = 0;
					sendSMessage ();
				}

				ASDU asdu = new ASDU (parameters, buffer, msgSize);

				if (asduReceivedHandler != null)
					asduReceivedHandler(asduReceivedHandlerParameter, asdu);

				return true;
			}

			// Check for TESTFR_ACT message
			if (buffer [2] == 0x43) {

				if (debugOutput)
					Console.WriteLine ("Send TESTFR_CON");

				socket.Send (TESTFR_CON_MSG);

				return true;
			}

			return true;
		}

		private void HandleConnection() {

			byte[] bytes = new byte[300];

			// Connect to a remote device.
			try {

				IPAddress ipAddress = IPAddress.Parse(hostname);
				IPEndPoint remoteEP = new IPEndPoint(ipAddress, tcpPort);

				// Create a TCP/IP  socket.
				socket = new Socket(AddressFamily.InterNetwork, 
				                           SocketType.Stream, ProtocolType.Tcp );

				try {
					socket.Connect(remoteEP);

					Console.WriteLine("Socket connected to {0}",
					                  socket.RemoteEndPoint.ToString());

					if (autostart)
						socket.Send(STARTDT_ACT_MSG);

					running = true;

					while (running) {

						// Receive the response from the remote device.
						int bytesRec = receiveMessage(socket, bytes);

						if (bytesRec != 0) {

							if (debugOutput)
								Console.WriteLine(
									BitConverter.ToString(bytes, 0, bytesRec));
						
						
							if (checkMessage(socket, bytes, bytesRec) == false) {
								/* close connection on error */
								running = false;
							}
						}
						// TODO else ?

						Thread.Sleep(100);
					}

					Console.WriteLine("CLOSE CONNECTION!");

					// Release the socket.
					socket.Shutdown(SocketShutdown.Both);
					socket.Close();

				} catch (ArgumentNullException ane) {
					Console.WriteLine("ArgumentNullException : {0}",ane.ToString());
				} catch (SocketException se) {
					Console.WriteLine("SocketException : {0}",se.ToString());
				} catch (Exception e) {
					Console.WriteLine("Unexpected exception : {0}", e.ToString());
				}

			} catch (Exception e) {
				Console.WriteLine( e.ToString());
			}
		}

		public bool IsRunning {
			get {
				return this.running;
			}
		}

		public void SetASDUReceivedHandler(ASDUReceivedHandler handler, object parameter)
		{
			asduReceivedHandler = handler;
			asduReceivedHandlerParameter = parameter;
		}

	}
}

