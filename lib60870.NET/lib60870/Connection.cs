/*
 *  Connection.cs
 *
 *  Copyright 2016 MZ Automation GmbH
 *
 *  This file is part of lib60870.NET
 *
 *  lib60870.NET is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  lib60870.NET is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with lib60870.NET.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  See COPYING file for the complete license text.
 */

using System;

using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;

namespace lib60870
{
	public class ConnectionException : Exception
	{
		public ConnectionException(string message)
			:base(message)
		{
		}

		public ConnectionException(string message, Exception e)
			:base(message, e)
		{
		}
	}

	public enum ConnectionEvent {
		OPENED = 0,
		CLOSED = 1,
		STARTDT_CON_RECEIVED = 2,
		STOPDT_CON_RECEIVED = 3
	}

	/// <summary>
	/// ASDU received handler.
	/// </summary>
	public delegate bool ASDUReceivedHandler (object parameter, ASDU asdu);

	public delegate void ConnectionHandler (object parameter, ConnectionEvent connectionEvent);

	/// <summary>
	/// Raw message handler. Can be used to access the raw message.
	/// Returns true when message should be handled by the protocol stack, false, otherwise.
	/// </summary>
	public delegate bool RawMessageHandler (object parameter, byte[] message, int messageSize);

	public class Connection
	{
		static byte[] STARTDT_ACT_MSG = new byte[] { 0x68, 0x04, 0x07, 0x00, 0x00, 0x00 };

		static byte[] STARTDT_CON_MSG = new byte[] { 0x68, 0x04, 0x0b, 0x00, 0x00, 0x00 };

		static byte[] STOPDT_ACT_MSG = new byte[] { 0x68, 0x04, 0x13, 0x00, 0x00, 0x00 };

		static byte[] TESTFR_ACT_MSG = new byte[] { 0x68, 0x04, 0x43, 0x00, 0x00, 0x00 };

		static byte[] TESTFR_CON_MSG = new byte[] { 0x68, 0x04, 0x83, 0x00, 0x00, 0x00 };

		private int sendSequenceNumber;
		private int receiveSequenceNumber;

		private UInt64 uMessageTimeout = 0;
		private UInt64 iMessageTimeout = 0;

		private int lastUnconfirmedSendMessageNumber = 0;
		private UInt64 nextT3Timeout;
		private int outStandingTestFRConMessages = 0;

		/// <summary>
		/// Gets or sets the send sequence number N(S). WARNING: For test purposes only! Do net set
		/// in real application!
		/// </summary>
		/// <value>The send sequence number N(S)</value>
		public int SendSequenceNumber {
			get {
				return this.sendSequenceNumber;
			}
			set {
				this.sendSequenceNumber = value;
			}
		}

		/// <summary>
		/// Gets or sets the receive sequence number N(R). WARNING: For test purposes only! Do net set
		/// in real application!
		/// </summary>
		/// <value>The receive sequence number N(R)</value>
		public int ReceiveSequenceNumber {
			get {
				return this.receiveSequenceNumber;
			}
			set {
				this.receiveSequenceNumber = value;
			}
		}

		private int unconfirmedSlaveMessages; /* number of unconfirmed messages received */
		private long lastConfirmationTime; /* timestamp when the last confirmation message was sent */

        private Socket socket;

		private bool autostart = true;

		public bool Autostart {
			get {
				return this.autostart;
			}
			set {
				this.autostart = value;
			}
		}

		private string hostname;
		private int tcpPort;

		private bool running = false;
        private bool connecting = false;
		private bool socketError;
		private bool firstIMessageReceived = false;
		private SocketException lastException;

		private bool debugOutput = false;

		private ConnectionStatistics statistics = new ConnectionStatistics();

		private void ResetConnection() {
			sendSequenceNumber = 0;
			receiveSequenceNumber = 0;
			unconfirmedSlaveMessages = 0;
			lastConfirmationTime = System.Int64.MaxValue;
			firstIMessageReceived = false;
			outStandingTestFRConMessages = 0;

			uMessageTimeout = 0;
			iMessageTimeout = 0;

			lastUnconfirmedSendMessageNumber = 0;

			socketError = false;
			lastException = null;

			statistics.Reset ();
		}

		public bool DebugOutput {
			get {
				return this.debugOutput;
			}
			set {
				debugOutput = value;
			}
		}

		private int connectTimeoutInMs = 1000;

		private ConnectionParameters parameters;

		ASDUReceivedHandler asduReceivedHandler = null;
		object asduReceivedHandlerParameter = null;

		ConnectionHandler connectionHandler = null;
		object connectionHandlerParameter = null;

		RawMessageHandler rawMessageHandler = null;
		object rawMessageHandlerParameter = null;

		private void sendSMessage() {
			byte[] msg = new byte[6];

			msg [0] = 0x68;
			msg [1] = 0x04;
			msg [2] = 0x01;
			msg [3] = 0;
			msg [4] = (byte) ((receiveSequenceNumber % 128) * 2);
			msg [5] = (byte) (receiveSequenceNumber / 128);

			socket.Send (msg);

			statistics.SentMsgCounter++;
		}


		private void sendIMessage(Frame frame) 
		{
			if (running == false)
				throw new ConnectionException("not connected", new SocketException (10057));

			frame.PrepareToSend (sendSequenceNumber, receiveSequenceNumber);

			if (running) {
				socket.Send (frame.GetBuffer (), frame.GetMsgSize (), SocketFlags.None);
				sendSequenceNumber++;
				statistics.SentMsgCounter++;
			}
			else {
				if (lastException != null)
					throw new ConnectionException(lastException.Message, lastException);
				else
					throw new ConnectionException("not connected", new SocketException (10057));
			}

		}

		private void setup(string hostname, ConnectionParameters parameters, int tcpPort)
		{
			this.hostname = hostname;
			this.parameters = parameters;
			this.tcpPort = tcpPort;
			this.connectTimeoutInMs = parameters.T0 * 1000;
		}

		public Connection (string hostname)
		{
			setup (hostname, new ConnectionParameters(), 2404);
		}


		public Connection (string hostname, int tcpPort)
		{
			setup (hostname, new ConnectionParameters(), tcpPort);
		}

		public Connection (string hostname, ConnectionParameters parameters)
		{
			setup (hostname, parameters.clone(), 2404);
		}

		public Connection (string hostname, int tcpPort, ConnectionParameters parameters)
		{
			setup (hostname, parameters.clone(), tcpPort);
		}

		public ConnectionStatistics GetStatistics() {
			return this.statistics;
		}

		public void SetConnectTimeout(int millies)
		{
			this.connectTimeoutInMs = millies;
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

		private void EncodeIOA(Frame frame, int ioa) {
			frame.SetNextByte((byte)(ioa & 0xff));

			if (parameters.SizeOfIOA > 1)
				frame.SetNextByte((byte)((ioa / 0x100) & 0xff));

			if (parameters.SizeOfIOA > 2)
				frame.SetNextByte((byte)((ioa / 0x10000) & 0xff));
		}

		/// <summary>
		/// Sends the interrogation command.
		/// </summary>
		/// <param name="cot">Cause of transmission</param>
		/// <param name="ca">Common address</param>
		/// <param name="qoi">Qualifier of interrogation (20 = station interrogation)</param>
		/// <exception cref="ConnectionException">description</exception>
		public void SendInterrogationCommand(CauseOfTransmission cot, int ca, byte qoi) 
		{
			Frame frame = new T104Frame ();

			EncodeIdentificationField (frame, TypeID.C_IC_NA_1, 1, cot, ca);

			EncodeIOA (frame, 0);

			/* encode COI (7.2.6.21) */
			frame.SetNextByte (qoi); /* 20 = station interrogation */

			if (debugOutput)
				Console.WriteLine("MASTER: Encoded C_IC_NA_1 with " + frame.GetMsgSize() + " bytes.");

			sendIMessage (frame);
		}

		/// <summary>
		/// Sends the counter interrogation command (C_CI_NA_1 typeID: 101)
		/// </summary>
		/// <param name="cot">Cause of transmission</param>
		/// <param name="ca">Common address</param>
		/// <param name="qcc">Qualifier of counter interrogation command</param>
		/// <exception cref="ConnectionException">description</exception>
		public void SendCounterInterrogationCommand(CauseOfTransmission cot, int ca, byte qcc)
		{
			Frame frame = new T104Frame ();

			EncodeIdentificationField (frame, TypeID.C_CI_NA_1, 1, cot, ca);

			EncodeIOA (frame, 0);

			/* encode QCC */
			frame.SetNextByte (qcc);

			if (debugOutput)
				Console.WriteLine("MASTER: Encoded C_CI_NA_1 with " + frame.GetMsgSize() + " bytes.");

			sendIMessage (frame);
		}
	
		/// <summary>
		/// Sends a read command (C_RD_NA_1 typeID: 102).
		/// </summary>
		/// 
		/// This will send a read command C_RC_NA_1 (102) to the slave/outstation. The COT is always REQUEST (5).
		/// It is used to implement the cyclical polling of data application function.
		/// 
		/// <param name="ca">Common address</param>
		/// <param name="ioa">Information object address</param>
		/// <exception cref="ConnectionException">description</exception>
		public void SendReadCommand(int ca, int ioa)
		{
			Frame frame = new T104Frame ();

			EncodeIdentificationField (frame, TypeID.C_RD_NA_1, 1, CauseOfTransmission.REQUEST, ca);

			EncodeIOA (frame, ioa);

			if (debugOutput)
				Console.WriteLine("MASTER: Encoded C_RD_NA_1 with " + frame.GetMsgSize() + " bytes.");

			sendIMessage (frame);
		}

		/// <summary>
		/// Sends a clock synchronization command (C_CS_NA_1 typeID: 103).
		/// </summary>
		/// <param name="ca">Common address</param>
		/// <param name="time">the new time to set</param>
		/// <exception cref="ConnectionException">description</exception>
		public void SendClockSyncCommand(int ca, CP56Time2a time)
		{
			Frame frame = new T104Frame ();

			EncodeIdentificationField (frame, TypeID.C_CS_NA_1, 1, CauseOfTransmission.ACTIVATION, ca);

			EncodeIOA (frame, 0);

			frame.AppendBytes (time.GetEncodedValue ());

			if (debugOutput)
				Console.WriteLine("MASTER: Encoded C_CS_NA_1 with " + frame.GetMsgSize() + " bytes.");

			sendIMessage (frame);
		}

		/// <summary>
		/// Sends a test command (C_TS_NA_1 typeID: 104).
		/// </summary>
		/// 
		/// Not required and supported by IEC 60870-5-104. 
		/// 
		/// <param name="ca">Common address</param>
		/// <exception cref="ConnectionException">description</exception>
		public void SendTestCommand(int ca)
		{
			Frame frame = new T104Frame ();

			EncodeIdentificationField (frame, TypeID.C_TS_NA_1, 1, CauseOfTransmission.ACTIVATION, ca);

			EncodeIOA (frame, 0);

			frame.SetNextByte (0xcc);
			frame.SetNextByte (0x55);

			if (debugOutput)
				Console.WriteLine("MASTER: Encoded C_TS_NA_1 with " + frame.GetMsgSize() + " bytes.");

			sendIMessage (frame);
		}

		/// <summary>
		/// Sends a reset process command (C_RP_NA_1 typeID: 105).
		/// </summary>
		/// <param name="cot">Cause of transmission</param>
		/// <param name="ca">Common address</param>
		/// <param name="qrp">Qualifier of reset process command</param>
		/// <exception cref="ConnectionException">description</exception>
		public void SendResetProcessCommand(CauseOfTransmission cot, int ca, byte qrp)
		{
			Frame frame = new T104Frame ();

			EncodeIdentificationField (frame, TypeID.C_RP_NA_1, 1, cot, ca);

			EncodeIOA (frame, 0);

			frame.SetNextByte (qrp);

			if (debugOutput)
				Console.WriteLine("MASTER: Encoded C_RP_NA_1 with " + frame.GetMsgSize() + " bytes.");

			sendIMessage (frame);
		}


		/// <summary>
		/// Sends a delay acquisition command (C_CD_NA_1 typeID: 106).
		/// </summary>
		/// <param name="cot">Cause of transmission</param>
		/// <param name="ca">Common address</param>
		/// <param name="delay">delay for acquisition</param>
		/// <exception cref="ConnectionException">description</exception>
		public void SendDelayAcquisitionCommand(CauseOfTransmission cot, int ca, CP16Time2a delay)
		{
			Frame frame = new T104Frame ();

			EncodeIdentificationField (frame, TypeID.C_CD_NA_1, 1, cot, ca);

			EncodeIOA (frame, 0);

			frame.AppendBytes (delay.GetEncodedValue ());

			if (debugOutput)
				Console.WriteLine("MASTER: Encoded C_CD_NA_1 with " + frame.GetMsgSize() + " bytes.");

			sendIMessage (frame);
		}

		/// <summary>
		/// Sends the control command.
		/// </summary>
		/// 
		/// The type ID has to match the type of the InformationObject!
		/// 
		/// C_SC_NA_1 -> SingleCommand
		/// C_DC_NA_1 -> DoubleCommand
		/// C_RC_NA_1 -> StepCommand
		/// C_SC_TA_1 -> SingleCommandWithCP56Time2a
		/// C_SE_NA_1 -> SetpointCommandNormalized
		/// C_SE_NB_1 -> SetpointCommandScaled
		/// C_SE_NC_1 -> SetpointCommandShort
		/// C_BO_NA_1 -> Bitstring32Command
		/// 
		/// 
		/// <param name="typeId">Type ID of the control command</param>
		/// <param name="cot">Cause of transmission (use ACTIVATION to start a control sequence)</param>
		/// <param name="ca">Common address</param>
		/// <param name="sc">Information object of the command</param>
		/// <exception cref="ConnectionException">description</exception>
		public void SendControlCommand(TypeID typeId, CauseOfTransmission cot, int ca, InformationObject sc) {
			Frame frame = new T104Frame ();

			EncodeIdentificationField (frame, typeId, 1 /* SQ:false; NumIX:1 */, cot, ca);

			sc.Encode (frame, parameters, false);

			if (debugOutput)
				Console.WriteLine("MASTER: Encoded " +  typeId.ToString() + " with " + frame.GetMsgSize() + " bytes.");

			sendIMessage (frame);
		}

		/// <summary>
		/// Sends an arbitrary ASDU to the connected slave
		/// </summary>
		/// <param name="asdu">The ASDU to send</param>
		public void SendASDU(ASDU asdu) 
		{
			Frame frame = new T104Frame ();
			asdu.Encode (frame, parameters);

			sendIMessage (frame);
		}

		/// <summary>
		/// Start data transmission on this connection
		/// </summary>
		public void SendStartDT() {
			if (running) {
				socket.Send(STARTDT_ACT_MSG);
				statistics.SentMsgCounter++;
			}
			else {
				if (lastException != null)
					throw new ConnectionException(lastException.Message, lastException);
				else
					throw new ConnectionException("not connected", new SocketException (10057));
			}
		}

		/// <summary>
		/// Stop data transmission on this connection
		/// </summary>
		public void SendStopDT() {
			if (running) {
				socket.Send(STOPDT_ACT_MSG);
				statistics.SentMsgCounter++;
			}
			else {
				if (lastException != null)
					throw new ConnectionException(lastException.Message, lastException);
				else
					throw new ConnectionException("not connected", new SocketException (10057));
			}
		}

		/// <summary>
		/// Connect this instance.
		/// </summary>
		/// 
		/// The function will throw a SocketException if the connection attempt is rejected or timed out.
		/// <exception cref="ConnectionException">description</exception>
		public void Connect() {

			ConnectAsync ();


			while ((running == false) && (socketError == false)) {
				Thread.Sleep (1);
			}

			if (socketError)
				throw new ConnectionException(lastException.Message, lastException);
		}

		private void ResetT3Timeout() {
			nextT3Timeout = (UInt64) SystemUtils.currentTimeMillis () + (UInt64) (parameters.T3 * 1000);
		}

		/// <summary>
		/// Connects to the server (outstation). This is a non-blocking call. Before using the connection
		/// you have to check if the connection is already connected and running.
		/// </summary>
		/// <exception cref="ConnectionException">description</exception>
        public void ConnectAsync()
        {
            if ((running == false) && (connecting == false))
            {
				ResetConnection ();

				ResetT3Timeout ();

                Thread workerThread = new Thread(HandleConnection);

                workerThread.Start();
            }
            else
            {
                if (running)
					throw new ConnectionException("already connected", new SocketException(10056)); /* WSAEISCONN - Socket is already connected */
                else
					throw new ConnectionException("already connecting",new SocketException(10037)); /* WSAEALREADY - Operation already in progress */

            }
        }

		private int receiveMessage(Socket socket, byte[] buffer) 
		{
			// wait for first byte
			if (socket.Poll (50, SelectMode.SelectRead)) {

				if (socket.Available == 0)
					throw new SocketException ();

				try {
					if (socket.Receive (buffer, 0, 1, SocketFlags.None) != 1)
						return -1;
				} catch (SocketException se) {
					if ((se.SocketErrorCode == SocketError.TimedOut) || (se.SocketErrorCode == SocketError.WouldBlock))
						return 0;
					else
						throw se;
				}

				if (buffer [0] != 0x68) {
					if (debugOutput)
						Console.WriteLine ("MASTER: Missing SOF indicator!");

					return -1;
				}

				// read length byte
				if (socket.Receive (buffer, 1, 1, SocketFlags.None) != 1)
					return -1;

				int length = buffer [1];

				// read remaining frame
				if (socket.Receive (buffer, 2, length, SocketFlags.None) != length) {
					if (debugOutput)
						Console.WriteLine ("MASTER: Failed to read complete frame!");

					return -1;
				}

				return length + 2;
			} else
				return 0;
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
			long currentTime = SystemUtils.currentTimeMillis ();

			if ((buffer [2] & 1) == 0) { /* I format frame */

				if (firstIMessageReceived == false) {
					firstIMessageReceived = true;
					lastConfirmationTime = currentTime; /* start timeout T2 */
				}

				if (msgSize < 7) {

					if (debugOutput)
						Console.WriteLine ("MASTER: I msg too small!");

					return false;
				}
					
				int frameSendSequenceNumber = ((buffer [3] * 0x100) + (buffer [2] & 0xfe)) / 2;
				int frameRecvSequenceNumber = ((buffer [5] * 0x100) + (buffer [4] & 0xfe)) / 2;
					
				if (debugOutput)
					Console.WriteLine ("MASTER: Received I frame: N(S) = " + frameSendSequenceNumber + " N(R) = " +
				frameRecvSequenceNumber);

				int expectedSequenceNumber = receiveSequenceNumber;

				/* check the receive sequence number N(R) - connection will be closed on an unexpected value */
				if (frameSendSequenceNumber != expectedSequenceNumber) {
					if (debugOutput)
						Console.WriteLine ("MASTER: Sequence error: Close connection!");
					return false;
				}
					
				receiveSequenceNumber = (receiveSequenceNumber + 1) % 32768;
				unconfirmedSlaveMessages++;

				if ((unconfirmedSlaveMessages > parameters.W) || checkConfirmTimeout (currentTime)) {

					lastConfirmationTime = currentTime;

					unconfirmedSlaveMessages = 0;
					sendSMessage (); /* send confirmation message */
				}

				ASDU asdu = new ASDU (parameters, buffer, msgSize);

				if (asduReceivedHandler != null)
					asduReceivedHandler (asduReceivedHandlerParameter, asdu);
			} else if ((buffer [2] & 0x03) == 0x01) { /* S format frame */
				//TODO check if message is confirmed
			} else if ((buffer [2] & 0x03) == 0x03) { /* U format frame */

				uMessageTimeout = 0;

				if (buffer [2] == 0x43) { // Check for TESTFR_ACT message
					statistics.RcvdTestFrActCounter++;
					socket.Send (TESTFR_CON_MSG);
					statistics.SentMsgCounter++;
				} else if (buffer [2] == 0x83) { /* TESTFR_CON */
					statistics.RcvdTestFrConCounter++;
					outStandingTestFRConMessages = 0;
				} else if (buffer [2] == 0x07) { /* STARTDT ACT */

					socket.Send (STARTDT_CON_MSG);
					statistics.SentMsgCounter++;
				} else if (buffer [2] == 0x0b) { /* STARTDT_CON */

					if (connectionHandler != null)
						connectionHandler (connectionHandlerParameter, ConnectionEvent.STARTDT_CON_RECEIVED);

				} else if (buffer [2] == 0x23) { /* STOPDT_CON */

					if (connectionHandler != null)
						connectionHandler (connectionHandlerParameter, ConnectionEvent.STOPDT_CON_RECEIVED);
				}

			} else {
				if (debugOutput)
					Console.WriteLine ("MASTER: Unknown message type");
				return false;
			}

			ResetT3Timeout ();

			return true;
		}


		private bool isConnected() 
		{
			try {
				byte[] tmp = new byte[1];

				socket.Blocking = false;
				socket.Send (tmp, 0, 0);

				return true;
			}
			catch (SocketException e) {
				if (e.NativeErrorCode.Equals (10035)) {
					if (debugOutput)
						Console.WriteLine ("MASTER: Still Connected, but the Send would block");
					return true;
				}
				else
				{
					if (debugOutput)
						Console.WriteLine("MASTER: Disconnected: error code {0}!", e.NativeErrorCode);
					return false;
				}
			}
		}

		private void ConnectSocketWithTimeout()
		{
			IPAddress ipAddress = IPAddress.Parse(hostname);
			IPEndPoint remoteEP = new IPEndPoint(ipAddress, tcpPort);

			// Create a TCP/IP  socket.
			socket = new Socket(AddressFamily.InterNetwork, 
				SocketType.Stream, ProtocolType.Tcp );

			var result = socket.BeginConnect(remoteEP, null, null);

			bool success = result.AsyncWaitHandle.WaitOne(connectTimeoutInMs, true);
			if (success)
			{
				socket.EndConnect(result);
			}
			else
			{
				socket.Close();
				throw new SocketException(10060); // Connection timed out.
			}
		}

		private bool handleTimeouts() {
			UInt64 currentTime = (UInt64) SystemUtils.currentTimeMillis();

			if (checkConfirmTimeout ((long) currentTime)) {

				lastConfirmationTime = (long) currentTime;
				unconfirmedSlaveMessages = 0;

				sendSMessage (); /* send confirmation message */
			}
				
			if (currentTime > nextT3Timeout) {

				if (outStandingTestFRConMessages > 2) {
					if (debugOutput)
						Console.WriteLine ("MASTER: Timeout for TESTFR_CON message");

					// close connection
					return false;
				} else {
					socket.Send (TESTFR_ACT_MSG);
					statistics.SentMsgCounter++;
					if (debugOutput)
						Console.WriteLine ("MASTER: U message T3 timeout");
					uMessageTimeout = (UInt64)currentTime + (UInt64)(parameters.T1 * 1000);
					outStandingTestFRConMessages++;
					ResetT3Timeout ();
				}
			}

			if (uMessageTimeout != 0) {
				if (currentTime > uMessageTimeout) {
					if (debugOutput) 
						Console.WriteLine ("MASTER: U message T1 timeout");
					throw new SocketException (10060);
				}
			}

			return true;
		}

		private void HandleConnection() {

			byte[] bytes = new byte[300];


			try {

				try {

					connecting = true;

					try {
						// Connect to a remote device.
						ConnectSocketWithTimeout();

						if (debugOutput)
							Console.WriteLine("MASTER: Socket connected to {0}",
								socket.RemoteEndPoint.ToString());

						if (autostart) {
							socket.Send(STARTDT_ACT_MSG);
							statistics.SentMsgCounter++;
						}

						running = true;
						socketError = false;
                        connecting = false;

						if (connectionHandler != null)
							connectionHandler(connectionHandlerParameter, ConnectionEvent.OPENED);
						
					} catch (SocketException se) {
						if (debugOutput)
							Console.WriteLine("MASTER: SocketException : {0}",se.ToString());

						running = false;
						socketError = true;
						lastException = se;
					}

					bool loopRunning = running;

					while (loopRunning) {

						try {
							// Receive a message from from the remote device.
							int bytesRec = receiveMessage(socket, bytes);

							if (bytesRec > 0) {

								if (debugOutput)
									Console.WriteLine(
										BitConverter.ToString(bytes, 0, bytesRec));

								statistics.RcvdMsgCounter++;
							
								bool handleMessage = true;

								if (rawMessageHandler != null) 
									handleMessage = rawMessageHandler(rawMessageHandlerParameter, bytes, bytesRec);
											
								if (handleMessage) {
									if (checkMessage(socket, bytes, bytesRec) == false) {
										/* close connection on error */
										loopRunning = false;
									}
								}
							}
							else if (bytesRec == -1)
								loopRunning = false;

							if (handleTimeouts() == false)
								loopRunning = false;

							if (isConnected() == false)
								loopRunning = false;
						}
						catch (SocketException) {	
							loopRunning = false;
						}
					}

					if (debugOutput)
						Console.WriteLine("MASTER: CLOSE CONNECTION!");

					// Release the socket.
					try {
						socket.Shutdown(SocketShutdown.Both);
					}
					catch (SocketException) {
					}

					socket.Close();

					if (connectionHandler != null)
						connectionHandler(connectionHandlerParameter, ConnectionEvent.CLOSED);

				} catch (ArgumentNullException ane) {
                    connecting = false;
					if (debugOutput)
						Console.WriteLine("MASTER: ArgumentNullException : {0}",ane.ToString());
				} catch (SocketException se) {
					if (debugOutput)
						Console.WriteLine("MASTER: SocketException : {0}",se.ToString());
				} catch (Exception e) {
					if (debugOutput)
						Console.WriteLine("MASTER: Unexpected exception : {0}", e.ToString());
				}
					
			} catch (Exception e) {
				Console.WriteLine( e.ToString());
			}

			running = false;
			connecting = false;
		}

		public bool IsRunning {
			get {
				return this.running;
			}
		}

		public void Close()
		{
			if (running) {
				socket.Shutdown (SocketShutdown.Both);

				while (running)
					Thread.Sleep (1);
			}
		}

		public void SetASDUReceivedHandler(ASDUReceivedHandler handler, object parameter)
		{
			asduReceivedHandler = handler;
			asduReceivedHandlerParameter = parameter;
		}

		/// <summary>
		/// Sets the connection handler. The connection handler is called when
		/// the connection is established or closed
		/// </summary>
		/// <param name="handler">the handler to be called</param>
		/// <param name="parameter">user provided parameter that is passed to the handler</param>
		public void SetConnectionHandler(ConnectionHandler handler, object parameter)
		{
			connectionHandler = handler;
			connectionHandlerParameter = parameter;
		}

		/// <summary>
		/// Sets the raw message handler. This is a callback to intercept raw messages received.
		/// </summary>
		/// <param name="handler">Handler.</param>
		/// <param name="parameter">Parameter.</param>
		public void SetRawMessageHandler(RawMessageHandler handler, object parameter)
		{
			rawMessageHandler = handler;
			rawMessageHandlerParameter = parameter;
		}
	}
}

