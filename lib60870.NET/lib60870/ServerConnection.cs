using System;

using lib60870;
using System.Net;
using System.Net.Sockets;
using System.Threading;

namespace lib60870
{
	public class ServerConnection 
	{

		//static byte[] STARTDT_ACT_MSG = new byte[] { 0x68, 0x04, 0x07, 0x00, 0x00, 0x00 };

		static byte[] STARTDT_ACT_CON_MSG = new byte[] { 0x68, 0x04, 0x0b, 0x00, 0x00, 0x00 };

		static byte[] TESTFR_CON_MSG = new byte[] { 0x68, 0x04, 0x83, 0x00, 0x00, 0x00 };

		private int sendCount = 0;
		private int receiveCount = 0;

		private int unconfirmedMessages = 0; /* number of unconfirmed messages received */
		private long lastConfirmationTime = System.Int64.MaxValue; /* timestamp when the last confirmation message was sent */

		private ConnectionParameters parameters;
		private Server server;

		public ServerConnection(Socket socket, ConnectionParameters parameters, Server server) 
		{
			this.socket = socket;
			this.parameters = parameters;
			this.server = server;

			Thread workerThread = new Thread(HandleConnection);

			workerThread.Start ();
		}

		private Socket socket;

		private bool running = false;

		private bool debugOutput = true;

		private int receiveMessage(Socket socket, byte[] buffer) 
		{
			if (socket.Poll (100, SelectMode.SelectRead)) {

				if (socket.Available == 0)
					throw new SocketException ();

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
			} else
				return 0;
		}

		private void sendIMessage(Frame frame) 
		{
			frame.PrepareToSend (sendCount, receiveCount);
			socket.Send (frame.GetBuffer (), frame.GetMsgSize (), SocketFlags.None);
			sendCount++;
		}

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

		public void SendACT_CON(ASDU asdu, bool negative) {
			asdu.Cot = CauseOfTransmission.ACTIVATION_CON;
			asdu.IsNegative = negative;

			Frame frame = new T104Frame ();
			asdu.Encode (frame, parameters);
			frame.PrepareToSend (sendCount, receiveCount);

			sendIMessage (frame);
		}

		public void SendACT_TERM(ASDU asdu) {
			asdu.Cot = CauseOfTransmission.ACTIVATION_TERMINATION;
			asdu.IsNegative = false;

			Frame frame = new T104Frame ();
			asdu.Encode (frame, parameters);
			frame.PrepareToSend (sendCount, receiveCount);

			sendIMessage (frame);
		}

		public void SendASDU(ASDU asdu) {
			Frame frame = new T104Frame ();
			asdu.Encode (frame, parameters);
			frame.PrepareToSend (sendCount, receiveCount);

			sendIMessage (frame);
		}


		private bool checkConfirmTimeout(long currentTime) 
		{
			if ((currentTime - lastConfirmationTime) >= (parameters.T2 * 1000))
				return true;
			else
				return false;
		}

		private void SendSMessageIfRequired() {
			long currentTime = SystemUtils.currentTimeMillis();

			if (unconfirmedMessages > 0) {

				if ((unconfirmedMessages > parameters.W) || checkConfirmTimeout (currentTime)) {

					Console.WriteLine ("Send S message");

					//TODO check timeout condition /* t2 */
					lastConfirmationTime = currentTime;

					unconfirmedMessages = 0;
					sendSMessage ();
				}
			}
		}

		private void IncreaseReceivedMessageCounters ()
		{
			receiveCount++;
			unconfirmedMessages++;

			if (unconfirmedMessages == 1) {
				lastConfirmationTime = SystemUtils.currentTimeMillis();
			}
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

				IncreaseReceivedMessageCounters ();

				bool messageHandled = false;

				ASDU asdu = new ASDU (parameters, buffer, msgSize);

				switch (asdu.TypeId) {
				case TypeID.C_IC_NA_1: /* 100 - interrogation command */

					Console.WriteLine ("Rcvd interrogation command C_IC_NA_1");

					if (server.interrogationHandler != null) {

						if (server.interrogationHandler (server.InterrogationHandlerParameter, this, asdu))
							messageHandled = true;
					}

					break;

				case TypeID.C_TS_NA_1: /* 104 - test command */

					Console.WriteLine ("Rcvd test command C_TS_NA_1");

					if (asdu.Cot != CauseOfTransmission.ACTIVATION)
						asdu.Cot = CauseOfTransmission.UNKNOWN_CAUSE_OF_TRANSMISSION;
					else
						asdu.Cot = CauseOfTransmission.ACTIVATION_CON;

					this.SendASDU (asdu);

					messageHandled = true;

					break;

				}

				if ((messageHandled == false) && (server.asduHandler != null))
					if (server.asduHandler (server.asduHandlerParameter, this, asdu))
						messageHandled = true;



				return true;
			}

			// Check for TESTFR_ACT message
			if (buffer [2] == 0x43) {

				if (debugOutput)
					Console.WriteLine ("Send TESTFR_CON");

				socket.Send (TESTFR_CON_MSG);

				return true;

			} else if (buffer [2] == 0x07) {

				if (debugOutput)
					Console.WriteLine ("Send STARTDT ACT CON");

				socket.Send (STARTDT_ACT_CON_MSG);

				return true;
			}

			return true;
		}

		private void HandleConnection() {

			byte[] bytes = new byte[300];

			// Connect to a remote device.
			try {

				try {

					Console.WriteLine("Socket connected");

					running = true;

					while (running) {

						try {
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
							
							SendSMessageIfRequired();
							
							Thread.Sleep(100);
						} catch (SocketException) {
							running = false;
						}
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
	}
	
}
