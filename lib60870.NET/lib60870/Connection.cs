/*
 *  Connection.cs
 *
 *  Copyright 2016, 2017 MZ Automation GmbH
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
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography.X509Certificates;
using System.Net.Security;

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
		STOPDT_CON_RECEIVED = 3,
        CONNECT_FAILED = 4
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

		/**********************************************/
		/* data structure for k-size sent ASDU buffer */
		private struct SentASDU
		{
			public long sentTime; // required for T1 timeout
			public int seqNo;
		}
			
		private int maxSentASDUs; /* maximum number of ASDU to be sent without confirmation - parameter k */
		private int oldestSentASDU = -1; /* index of oldest entry in k-buffer */
		private int newestSentASDU = -1; /* index of newest entry in k-buffer */
		private SentASDU[] sentASDUs = null; /* the k-buffer */

		/**********************************************/

		
		private Queue<ASDU> waitingToBeSent = null;
		private bool useSendMessageQueue = true;

		private UInt64 nextT3Timeout;
		private int outStandingTestFRConMessages = 0;

		Thread workerThread = null;

		private int unconfirmedReceivedIMessages; /* number of unconfirmed messages received */
		private long lastConfirmationTime; /* timestamp when the last confirmation message was sent */

		private Socket socket;
		//private NetworkStream netStream = null;
		private Stream netStream = null;
		private TlsSecurityInformation tlsSecInfo = null;

		private bool autostart = true;


		private string hostname;
		private int tcpPort;

		private bool running = false;
		private bool connecting = false;
		private bool socketError;
		private bool firstIMessageReceived = false;
		private SocketException lastException;

		private bool debugOutput = false;
		private static int connectionCounter = 0;
		private int connectionID;

		/// <summary>
		/// Gets or sets a value indicating whether this <see cref="lib60870.Connection"/> use send message queue.
		/// </summary>
		/// <description>
		/// If <c>true</c> the Connection stores the ASDUs to be sent in a Queue when the connection cannot send
		/// ASDUs. This is the case when the counterpart (slave/server) is (temporarily) not able to handle new message,
		/// or the slave did not confirm the reception of the ASDUs for other reasons. If <c>false</c> the ASDU will be 
		/// ignored and a <see cref="lib60870.ConnectionException"/> will be thrown in this case.
		/// </description>
		/// <value><c>true</c> if use send message queue; otherwise, <c>false</c>.</value>
		public bool UseSendMessageQueue {
			get {
				return this.useSendMessageQueue;
			}
			set {
				useSendMessageQueue = value;
			}
		}

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

		/// <summary>
		/// Gets or sets a value indicating whether this <see cref="lib60870.Connection"/> is automatically sends
		/// a STARTDT_ACT message on startup.
		/// </summary>
		/// <value><c>true</c> to send STARTDT_ACT message on startup; otherwise, <c>false</c>.</value>
		public bool Autostart {
			get {
				return this.autostart;
			}
			set {
				this.autostart = value;
			}
		}



		private void DebugLog(string message)
		{
			if (debugOutput)
				Console.WriteLine ("CS104 MASTER CONNECTION " + connectionID + ": " + message);
		}

		private ConnectionStatistics statistics = new ConnectionStatistics();

		private void ResetConnection() {
			sendSequenceNumber = 0;
			receiveSequenceNumber = 0;
			unconfirmedReceivedIMessages = 0;
			lastConfirmationTime = System.Int64.MaxValue;
			firstIMessageReceived = false;
			outStandingTestFRConMessages = 0;

			uMessageTimeout = 0;

			socketError = false;
			lastException = null;

			maxSentASDUs = parameters.K;
			oldestSentASDU = -1;
			newestSentASDU = -1;
			sentASDUs = new SentASDU[maxSentASDUs];

			if (useSendMessageQueue)
				waitingToBeSent = new Queue<ASDU> ();

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

		public ConnectionParameters Parameters {
			get {
				return this.parameters;
			}
		}

		ASDUReceivedHandler asduReceivedHandler = null;
		object asduReceivedHandlerParameter = null;

		ConnectionHandler connectionHandler = null;
		object connectionHandlerParameter = null;

		RawMessageHandler recvRawMessageHandler = null;
		object recvRawMessageHandlerParameter = null;

        RawMessageHandler sentMessageHandler = null;
        object sentMessageHandlerParameter = null;

		private void SendSMessage() {
			byte[] msg = new byte[6];

			msg [0] = 0x68;
			msg [1] = 0x04;
			msg [2] = 0x01;
			msg [3] = 0;
			msg [4] = (byte) ((receiveSequenceNumber % 128) * 2);
			msg [5] = (byte) (receiveSequenceNumber / 128);

			netStream.Write (msg, 0, msg.Length);

			statistics.SentMsgCounter++;

            if (sentMessageHandler != null)
            {
                sentMessageHandler(sentMessageHandlerParameter, msg, 6);
            }
        }

		private bool CheckSequenceNumber(int seqNo) {

			lock (sentASDUs) {

				/* check if received sequence number is valid */

				bool seqNoIsValid = false;
				bool counterOverflowDetected = false;

				if (oldestSentASDU == -1) { /* if k-Buffer is empty */
					if (seqNo == sendSequenceNumber)
						seqNoIsValid = true;
				}
				else {
					// Two cases are required to reflect sequence number overflow
					if (sentASDUs[oldestSentASDU].seqNo <= sentASDUs[newestSentASDU].seqNo) {
						if ((seqNo >= sentASDUs [oldestSentASDU].seqNo) &&
							(seqNo <= sentASDUs [newestSentASDU].seqNo))
							seqNoIsValid = true;

					} else {
						if ((seqNo >= sentASDUs [oldestSentASDU].seqNo) ||
							(seqNo <= sentASDUs [newestSentASDU].seqNo))
							seqNoIsValid = true;

						counterOverflowDetected = true;
					}

					int latestValidSeqNo = (sentASDUs [oldestSentASDU].seqNo - 1) % 32768;

					if (latestValidSeqNo == seqNo)
						seqNoIsValid = true;
				}

				if (seqNoIsValid == false) {
					DebugLog ("Received sequence number out of range");
					return false;
				}

				if (oldestSentASDU != -1) {
					do {
						if (counterOverflowDetected == false) {
							if (seqNo < sentASDUs [oldestSentASDU].seqNo)
								break;
						}
						else {
							if (seqNo == ((sentASDUs [oldestSentASDU].seqNo - 1) % 32768))
								break;
						}
							
						oldestSentASDU = (oldestSentASDU + 1) % maxSentASDUs;

						int checkIndex = (newestSentASDU + 1) % maxSentASDUs;

						if (oldestSentASDU == checkIndex) {
							oldestSentASDU = -1;
							break;
						}

						if (sentASDUs [oldestSentASDU].seqNo == seqNo)
							break;

					} while (true);
				}
			}

			return true;
		}

		private bool IsSentBufferFull() {

			if (oldestSentASDU == -1)
				return false;

			int newIndex = (newestSentASDU + 1) % maxSentASDUs;

			if (newIndex == oldestSentASDU)
				return true;
			else
				return false;
		}

		private int SendIMessage(ASDU asdu) {
			BufferFrame frame = new BufferFrame(new byte[260], 6); /* reserve space for ACPI */
			asdu.Encode (frame, parameters);

			byte[] buffer = frame.GetBuffer ();

			int msgSize = frame.GetMsgSize (); /* ACPI + ASDU */

			buffer [0] = 0x68;

			/* set size field */
			buffer [1] = (byte) (msgSize - 2);

			buffer [2] = (byte) ((sendSequenceNumber % 128) * 2);
			buffer [3] = (byte) (sendSequenceNumber / 128);

			buffer [4] = (byte) ((receiveSequenceNumber % 128) * 2);
			buffer [5] = (byte) (receiveSequenceNumber / 128);

			if (running) {
				//socket.Send (buffer, msgSize, SocketFlags.None);

				netStream.Write (buffer, 0, msgSize);

				sendSequenceNumber = (sendSequenceNumber + 1) % 32768;
				statistics.SentMsgCounter++;
				unconfirmedReceivedIMessages = 0;

                if (sentMessageHandler != null)
                {
                    sentMessageHandler(sentMessageHandlerParameter, buffer, msgSize);
                }

                return sendSequenceNumber;
			} else {
				if (lastException != null)
					throw new ConnectionException (lastException.Message, lastException);
				else
					throw new ConnectionException ("not connected", new SocketException (10057));
			}


		}

		private void PrintSendBuffer() {

			if (oldestSentASDU != -1) {

				int currentIndex = oldestSentASDU;

				int nextIndex = 0;

				DebugLog ("------k-buffer------");

				do {
					DebugLog (currentIndex + " : S " + sentASDUs[currentIndex].seqNo + " : time " +
						sentASDUs[currentIndex].sentTime);

					if (currentIndex == newestSentASDU)
						nextIndex = -1;

					currentIndex = (currentIndex + 1) % maxSentASDUs;

				} while (nextIndex != -1);

				DebugLog ("--------------------");

			}
		}

		private void SendIMessageAndUpdateSentASDUs(ASDU asdu)
		{
			lock (sentASDUs) {

				int currentIndex = 0;

				if (oldestSentASDU == -1) {
					oldestSentASDU = 0;
					newestSentASDU = 0;

				} else {
					currentIndex = (newestSentASDU + 1) % maxSentASDUs;
				}
					
				sentASDUs [currentIndex].seqNo = SendIMessage (asdu);
				sentASDUs [currentIndex].sentTime = SystemUtils.currentTimeMillis ();

				newestSentASDU = currentIndex;

				PrintSendBuffer ();
			}
		}

		private bool SendNextWaitingASDU() 
		{
			bool sentAsdu = false;

			if (running == false)
				throw new ConnectionException ("connection lost");

			try {

				lock (waitingToBeSent) {

					while (waitingToBeSent.Count > 0) {

						if (IsSentBufferFull () == true)
							break;

						ASDU asdu = waitingToBeSent.Dequeue ();

						if (asdu != null) {
							SendIMessageAndUpdateSentASDUs(asdu);
							sentAsdu = true;
						}
						else
							break;
						
					}
				}
			} catch (Exception) {
				running = false;
				throw new ConnectionException ("connection lost");
			}

			return sentAsdu;
		}

		private void SendASDUInternal(ASDU asdu) 
		{
			lock (socket) {

				if (running == false)
					throw new ConnectionException ("not connected", new SocketException (10057));

				if (useSendMessageQueue) {
					lock (waitingToBeSent) {
						waitingToBeSent.Enqueue (asdu);
					}
				
					SendNextWaitingASDU ();
				} else {

					if (IsSentBufferFull())
						throw new ConnectionException ("Flow control congestion. Try again later.");

					SendIMessageAndUpdateSentASDUs (asdu);
				}
			}
		}

		private void Setup(string hostname, ConnectionParameters parameters, int tcpPort)
		{
			this.hostname = hostname;
			this.parameters = parameters;
			this.tcpPort = tcpPort;
			this.connectTimeoutInMs = parameters.T0 * 1000;

			connectionCounter++;
			connectionID = connectionCounter;
		}

		public Connection (string hostname)
		{
			Setup (hostname, new ConnectionParameters(), 2404);
		}


		public Connection (string hostname, int tcpPort)
		{
			Setup (hostname, new ConnectionParameters(), tcpPort);
		}

		public Connection (string hostname, ConnectionParameters parameters)
		{
			Setup (hostname, parameters.clone(), 2404);
		}

		public Connection (string hostname, int tcpPort, ConnectionParameters parameters)
		{
			Setup (hostname, parameters.clone(), tcpPort);
		}

		public void SetTlsSecurity(TlsSecurityInformation securityInfo)
		{
			tlsSecInfo = securityInfo;

			if (securityInfo != null)
				this.tcpPort = 19998;
		}

		public ConnectionStatistics GetStatistics() {
			return this.statistics;
		}

		public void SetConnectTimeout(int millies)
		{
			this.connectTimeoutInMs = millies;
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
			ASDU asdu = new ASDU (parameters, cot, false, false, (byte) parameters.OriginatorAddress, ca, false);

			asdu.AddInformationObject (new InterrogationCommand (0, qoi));

			SendASDUInternal (asdu);
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
			ASDU asdu = new ASDU (parameters, cot, false, false, (byte) parameters.OriginatorAddress, ca, false);

			asdu.AddInformationObject (new CounterInterrogationCommand(0, qcc));

			SendASDUInternal (asdu);
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
			ASDU asdu = new ASDU (parameters, CauseOfTransmission.REQUEST, false, false, (byte) parameters.OriginatorAddress, ca, false);

			asdu.AddInformationObject(new ReadCommand(ioa));

			SendASDUInternal (asdu);
		}

		/// <summary>
		/// Sends a clock synchronization command (C_CS_NA_1 typeID: 103).
		/// </summary>
		/// <param name="ca">Common address</param>
		/// <param name="time">the new time to set</param>
		/// <exception cref="ConnectionException">description</exception>
		public void SendClockSyncCommand(int ca, CP56Time2a time)
		{
			ASDU asdu = new ASDU (parameters, CauseOfTransmission.ACTIVATION, false, false, (byte) parameters.OriginatorAddress, ca, false);

			asdu.AddInformationObject (new ClockSynchronizationCommand (0, time));

			SendASDUInternal (asdu);
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
			ASDU asdu = new ASDU (parameters, CauseOfTransmission.ACTIVATION, false, false, (byte) parameters.OriginatorAddress, ca, false);

			asdu.AddInformationObject (new TestCommand ());

			SendASDUInternal (asdu);
		}

        /// <summary>
        /// Sends a test command with CP56Time2a time (C_TS_TA_1 typeID: 107).
        /// </summary>
        /// <param name="ca">Common address</param>
        /// <param name="tsc">test sequence number</param>
        /// <param name="time">test timestamp</param>
        /// <exception cref="ConnectionException">description</exception>
        public void SendTestCommandWithCP56Time2a(int ca, ushort tsc, CP56Time2a time)
        {
            ASDU asdu = new ASDU(parameters, CauseOfTransmission.ACTIVATION, false, false, (byte)parameters.OriginatorAddress, ca, false);

            asdu.AddInformationObject(new TestCommandWithCP56Time2a(tsc, time));

            SendASDUInternal(asdu);
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
			ASDU asdu = new ASDU (parameters, CauseOfTransmission.ACTIVATION, false, false, (byte) parameters.OriginatorAddress, ca, false);

			asdu.AddInformationObject (new ResetProcessCommand(0, qrp));

			SendASDUInternal (asdu);
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
			ASDU asdu = new ASDU (parameters, CauseOfTransmission.ACTIVATION, false, false, (byte) parameters.OriginatorAddress, ca, false);

			asdu.AddInformationObject (new DelayAcquisitionCommand (0, delay));

			SendASDUInternal (asdu);
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
		/// <param name="typeId">Type ID of the control command</param>
		/// <param name="cot">Cause of transmission (use ACTIVATION to start a control sequence)</param>
		/// <param name="ca">Common address</param>
		/// <param name="sc">Information object of the command</param>
		/// <exception cref="ConnectionException">description</exception>
		[Obsolete("Use function without TypeID instead. TypeID will be derived from sc parameter")]
		public void SendControlCommand(TypeID typeId, CauseOfTransmission cot, int ca, InformationObject sc) {

			ASDU controlCommand = new ASDU (parameters, cot, false, false, (byte) parameters.OriginatorAddress, ca, false);

			controlCommand.AddInformationObject (sc);

			SendASDUInternal (controlCommand);
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
		/// <param name="cot">Cause of transmission (use ACTIVATION to start a control sequence)</param>
		/// <param name="ca">Common address</param>
		/// <param name="sc">Information object of the command</param>
		/// <exception cref="ConnectionException">description</exception>
		public void SendControlCommand(CauseOfTransmission cot, int ca, InformationObject sc) {

			ASDU controlCommand = new ASDU (parameters, cot, false, false, (byte) parameters.OriginatorAddress, ca, false);

			controlCommand.AddInformationObject (sc);

			SendASDUInternal (controlCommand);
		}

		/// <summary>
		/// Sends an arbitrary ASDU to the connected slave
		/// </summary>
		/// <param name="asdu">The ASDU to send</param>
		public void SendASDU(ASDU asdu) 
		{
			SendASDUInternal (asdu);
		}

		/// <summary>
		/// Start data transmission on this connection
		/// </summary>
		public void SendStartDT() {
			if (running) {
				netStream.Write (STARTDT_ACT_MSG, 0, STARTDT_ACT_MSG.Length);

				statistics.SentMsgCounter++;

                if (sentMessageHandler != null)
                {
                    sentMessageHandler(sentMessageHandlerParameter, STARTDT_ACT_MSG, 6);
                }
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
				//socket.Send(STOPDT_ACT_MSG);

				netStream.Write (STOPDT_ACT_MSG, 0, STOPDT_ACT_MSG.Length);

				statistics.SentMsgCounter++;
                if (sentMessageHandler != null)
                {
                    sentMessageHandler(sentMessageHandlerParameter, STOPDT_ACT_MSG, 6);
                }
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

                workerThread = new Thread(HandleConnection);

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

		private int receiveMessage(byte[] buffer) 
		{
			int readLength = 0;

			//if (netStream.DataAvailable) {

			if (socket.Poll (50, SelectMode.SelectRead)) {
				// wait for first byte
				if (netStream.Read (buffer, 0, 1) != 1)
					return -1;

				if (buffer [0] != 0x68) {
					DebugLog("Missing SOF indicator!");

					return -1;
				}

				// read length byte
				if (netStream.Read (buffer, 1, 1) != 1)
					return -1;

				int length = buffer [1];

				// read remaining frame
				if (netStream.Read (buffer, 2, length) != length) {
					DebugLog("Failed to read complete frame!");

					return -1;
				}

				readLength = length + 2;
			} 

			return readLength;
		}

		private bool checkConfirmTimeout(long currentTime) 
        {
            if ((currentTime - lastConfirmationTime) >= (parameters.T2 * 1000))
                return true;
            else
                return false;
        }

		private bool checkMessage(byte[] buffer, int msgSize)
		{
			long currentTime = SystemUtils.currentTimeMillis ();

			if ((buffer [2] & 1) == 0) { /* I format frame */

				if (firstIMessageReceived == false) {
					firstIMessageReceived = true;
					lastConfirmationTime = currentTime; /* start timeout T2 */
				}

				if (msgSize < 7) {
					DebugLog("I msg too small!");
					return false;
				}

				int frameSendSequenceNumber = ((buffer [3] * 0x100) + (buffer [2] & 0xfe)) / 2;
				int frameRecvSequenceNumber = ((buffer [5] * 0x100) + (buffer [4] & 0xfe)) / 2;

				DebugLog("Received I frame: N(S) = " + frameSendSequenceNumber + " N(R) = " + frameRecvSequenceNumber);

				/* check the receive sequence number N(R) - connection will be closed on an unexpected value */
				if (frameSendSequenceNumber != receiveSequenceNumber) {
					DebugLog("Sequence error: Close connection!");
					return false;
				}

				if (CheckSequenceNumber (frameRecvSequenceNumber) == false)
					return false;

				receiveSequenceNumber = (receiveSequenceNumber + 1) % 32768;
				unconfirmedReceivedIMessages++;

				try {
					ASDU asdu = new ASDU (parameters, buffer, 6, msgSize);

					if (asduReceivedHandler != null)
						asduReceivedHandler (asduReceivedHandlerParameter, asdu);
				}
				catch (ASDUParsingException e) {
					DebugLog ("ASDU parsing failed: " + e.Message);
					return false;
				}
					
			} else if ((buffer [2] & 0x03) == 0x01) { /* S format frame */
				int seqNo = (buffer[4] + buffer[5] * 0x100) / 2;

				DebugLog("Recv S(" + seqNo + ") (own sendcounter = " + sendSequenceNumber + ")");

				if (CheckSequenceNumber (seqNo) == false)
					return false;
			} else if ((buffer [2] & 0x03) == 0x03) { /* U format frame */

				uMessageTimeout = 0;

				if (buffer [2] == 0x43) { // Check for TESTFR_ACT message
					statistics.RcvdTestFrActCounter++;
					DebugLog ("RCVD TESTFR_ACT");
					DebugLog ("SEND TESTFR_CON");

					netStream.Write (TESTFR_CON_MSG, 0, TESTFR_CON_MSG.Length);

					statistics.SentMsgCounter++;
                    if (sentMessageHandler != null)
                    {
                        sentMessageHandler(sentMessageHandlerParameter, TESTFR_CON_MSG, 6);
                    }

                } else if (buffer [2] == 0x83) { /* TESTFR_CON */
					DebugLog ("RCVD TESTFR_CON");
					statistics.RcvdTestFrConCounter++;
					outStandingTestFRConMessages = 0;
				} else if (buffer [2] == 0x07) { /* STARTDT ACT */
					DebugLog ("RCVD STARTDT_ACT");

					netStream.Write (STARTDT_CON_MSG, 0, STARTDT_CON_MSG.Length);

					statistics.SentMsgCounter++;
                    if (sentMessageHandler != null)
                    {
                        sentMessageHandler(sentMessageHandlerParameter, STARTDT_CON_MSG, 6);
                    }
                } else if (buffer [2] == 0x0b) { /* STARTDT_CON */
					DebugLog ("RCVD STARTDT_CON");

					if (connectionHandler != null)
						connectionHandler (connectionHandlerParameter, ConnectionEvent.STARTDT_CON_RECEIVED);

				} else if (buffer [2] == 0x23) { /* STOPDT_CON */
					DebugLog ("RCVD STOPDT_CON");

					if (connectionHandler != null)
						connectionHandler (connectionHandlerParameter, ConnectionEvent.STOPDT_CON_RECEIVED);
				}

			} else {
				DebugLog("Unknown message type");
				return false;
			}

			ResetT3Timeout ();

			return true;
		}


		private bool isConnected() 
		{
			try {
				byte[] tmp = new byte[1];


				socket.Send (tmp, 0, 0);

				return true;
			}
			catch (SocketException e) {
				if (e.NativeErrorCode.Equals (10035)) {
					DebugLog("Still Connected, but the Send would block");
					return true;
				}
				else
				{
					DebugLog("Disconnected: error code " + e.NativeErrorCode);
					return false;
				}
			}
		}

		private void ConnectSocketWithTimeout()
		{
			IPAddress ipAddress;
			IPEndPoint remoteEP;

			try {
				ipAddress = IPAddress.Parse(hostname);
				remoteEP = new IPEndPoint(ipAddress, tcpPort);
			}
			catch(Exception) {
				throw new SocketException (87); // wrong argument
			}

			// Create a TCP/IP  socket.
			socket = new Socket(AddressFamily.InterNetwork, 
				SocketType.Stream, ProtocolType.Tcp );

			var result = socket.BeginConnect(remoteEP, null, null);

			bool success = result.AsyncWaitHandle.WaitOne(connectTimeoutInMs, true);
			if (success)
			{
				socket.EndConnect(result);

				socket.NoDelay = true;

				netStream = new NetworkStream (socket);
			}
			else
			{
				socket.Close();
				throw new SocketException(10060); // Connection timed out.
			}
		}

		private bool handleTimeouts() {
			UInt64 currentTime = (UInt64) SystemUtils.currentTimeMillis();

			if (currentTime > nextT3Timeout) {

				if (outStandingTestFRConMessages > 2) {
					DebugLog("Timeout for TESTFR_CON message");

					// close connection
					return false;
				} 
				else 
				{
					netStream.Write (TESTFR_ACT_MSG, 0, TESTFR_ACT_MSG.Length);

					statistics.SentMsgCounter++;
					DebugLog("U message T3 timeout");
					uMessageTimeout = (UInt64)currentTime + (UInt64)(parameters.T1 * 1000);
					outStandingTestFRConMessages++;
					ResetT3Timeout ();
                    if (sentMessageHandler != null)
                    {
                        sentMessageHandler(sentMessageHandlerParameter, TESTFR_ACT_MSG, 6);
                    }
                }
			}

			if (unconfirmedReceivedIMessages > 0) {
				if (checkConfirmTimeout ((long)currentTime)) {

					lastConfirmationTime = (long)currentTime;
					unconfirmedReceivedIMessages = 0;

					SendSMessage (); /* send confirmation message */
				}
			}

			if (uMessageTimeout != 0) {
				if (currentTime > uMessageTimeout) {
					DebugLog("U message T1 timeout");
					throw new SocketException (10060);
				}
			}

			/* check if counterpart confirmed I messages */
			lock (sentASDUs) {
				if (oldestSentASDU != -1) {

					if (((long)currentTime - sentASDUs [oldestSentASDU].sentTime) >= (parameters.T1 * 1000)) {
						return false;
					}
				}
			}

			return true;
		}

		private bool AreByteArraysEqual(byte[] array1, byte[] array2)
		{
			if (array1.Length == array2.Length) {

				for (int i = 0; i < array1.Length; i++) {
					if (array1 [i] != array2 [i])
						return false;
				}

				return true;
			} else
				return false;
		}

		private bool CertificateValidation(Object sender, X509Certificate certificate, X509Chain chain, SslPolicyErrors sslPolicyErrors) 
		{
			if (certificate != null) {

				if (tlsSecInfo.ChainValidation) {

					X509Chain newChain = new X509Chain ();

					newChain.ChainPolicy.RevocationMode = X509RevocationMode.NoCheck;
					newChain.ChainPolicy.RevocationFlag = X509RevocationFlag.ExcludeRoot;
					newChain.ChainPolicy.VerificationFlags = X509VerificationFlags.AllowUnknownCertificateAuthority;
					newChain.ChainPolicy.VerificationTime = DateTime.Now;
					newChain.ChainPolicy.UrlRetrievalTimeout = new TimeSpan(0, 0, 0);

					foreach (X509Certificate2 caCert in tlsSecInfo.CaCertificates)
						newChain.ChainPolicy.ExtraStore.Add(caCert);

					bool certificateStatus =  newChain.Build(new X509Certificate2(certificate.GetRawCertData()));

					if (certificateStatus == false)
						return false;
				}

				if (tlsSecInfo.AllowOnlySpecificCertificates) {

					foreach (X509Certificate2 allowedCert in tlsSecInfo.AllowedCertificates) {
						if (AreByteArraysEqual (allowedCert.GetCertHash (), certificate.GetCertHash ())) {
							return true;
						}
					}

					return false;
				}

				return true;
			}
			else 
				return false;
		}

		public X509Certificate LocalCertificateSelectionCallback (object sender, string targetHost, X509CertificateCollection localCertificates, X509Certificate remoteCertificate, string[] acceptableIssuers)
		{
			return localCertificates [0];
		}
			

		private void HandleConnection() {

			byte[] bytes = new byte[300];

			try {

				try {

					connecting = true;

					try {
						// Connect to a remote device.
						ConnectSocketWithTimeout();

						DebugLog("Socket connected to " + socket.RemoteEndPoint.ToString());

						if (tlsSecInfo != null) {

							DebugLog("Setup TLS");

							SslStream sslStream = new SslStream(netStream, true, CertificateValidation, LocalCertificateSelectionCallback);

							var clientCertificateCollection = new X509Certificate2Collection(tlsSecInfo.OwnCertificate);

							try {
								string targetHostName = tlsSecInfo.TargetHostName;

								if (targetHostName == null)
									targetHostName = "*";

								sslStream.AuthenticateAsClient(targetHostName, clientCertificateCollection, System.Security.Authentication.SslProtocols.Tls, false);
							}
							catch (IOException e) {

								string message;

								if (e.GetBaseException() != null) {
									message = e.GetBaseException().Message;
								}
								else {
									message = e.Message;
								}

								DebugLog("TLS authentication error: " + message);

								throw new SocketException();
							}

							if (sslStream.IsAuthenticated) {
								netStream = sslStream;
							}
							else {
								throw new SocketException();
							}

						}

						netStream.ReadTimeout = 50;

						if (autostart) {
							netStream.Write(STARTDT_ACT_MSG, 0, STARTDT_ACT_MSG.Length);

							statistics.SentMsgCounter++;
						}
							
						running = true;
						socketError = false;
                        connecting = false;

						if (connectionHandler != null)
							connectionHandler(connectionHandlerParameter, ConnectionEvent.OPENED);
						
					} catch (SocketException se) {
						DebugLog("SocketException: " + se.ToString());

						running = false;
						socketError = true;
						lastException = se;

                        if (connectionHandler != null)
                            connectionHandler(connectionHandlerParameter, ConnectionEvent.CONNECT_FAILED);
					}

					if (running) {

						bool loopRunning = running;

						while (loopRunning) {

							bool suspendThread = true;

							try {
								// Receive a message from from the remote device.
								int bytesRec = receiveMessage(bytes);

								if (bytesRec > 0) {

									DebugLog("RCVD: " + BitConverter.ToString(bytes, 0, bytesRec));

									statistics.RcvdMsgCounter++;
								
									bool handleMessage = true;

									if (recvRawMessageHandler != null) 
										handleMessage = recvRawMessageHandler(recvRawMessageHandlerParameter, bytes, bytesRec);
												
									if (handleMessage) {
										if (checkMessage(bytes, bytesRec) == false) {
											/* close connection on error */
											loopRunning = false;
										}
									}

									if (unconfirmedReceivedIMessages >= parameters.W) {
										lastConfirmationTime = SystemUtils.currentTimeMillis();

										unconfirmedReceivedIMessages = 0;
										SendSMessage ();
									}

									suspendThread = false;
								}
								else if (bytesRec == -1)
									loopRunning = false;
								

								if (handleTimeouts() == false)
									loopRunning = false;

								if (isConnected() == false)
									loopRunning = false;

								if (useSendMessageQueue) {
									if (SendNextWaitingASDU() == true)
										suspendThread = false;
								}

								if (suspendThread)
									Thread.Sleep(10);

							}
							catch (SocketException) {	
								loopRunning = false;
							}
							catch (System.IO.IOException e) {
								DebugLog("IOException: " + e.ToString());
								loopRunning = false;
							}
							catch (ConnectionException) {
								loopRunning = false;
							}
						}

						DebugLog("CLOSE CONNECTION!");

						// Release the socket.
						try {
							socket.Shutdown(SocketShutdown.Both);
						}
						catch (SocketException) {
						}

						socket.Close();

						netStream.Dispose();

						if (connectionHandler != null)
							connectionHandler(connectionHandlerParameter, ConnectionEvent.CLOSED);
					}

				} catch (ArgumentNullException ane) {
                    connecting = false;
					DebugLog("ArgumentNullException: " + ane.ToString());
				} catch (SocketException se) {
					DebugLog("SocketException: " + se.ToString());
				} catch (Exception e) {
					DebugLog("Unexpected exception: " + e.ToString());
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
				running = false;
				workerThread.Join ();
			}
		}

        /// <summary>
        /// Set the ASDUReceivedHandler. This handler is invoked whenever a new ASDU is received
        /// </summary>
        /// <param name="handler">the handler to be called</param>
        /// <param name="parameter">user provided parameter that is passed to the handler</param>
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
        /// <param name="handler">Handler/delegate that will be invoked when a message is received</param>
        /// <param name="parameter">will be passed to the delegate</param>
        public void SetRawMessageHandler(RawMessageHandler handler, object parameter)
		{
			recvRawMessageHandler = handler;
			recvRawMessageHandlerParameter = parameter;
		}

        /// <summary>
        /// Sets the sent message handler. This is a callback to intercept the sent raw messages
        /// </summary>
        /// <param name="handler">Handler/delegate that will be invoked when a message is sent<</param>
        /// <param name="parameter">will be passed to the delegate</param>
        public void SetSentMessageHandler(RawMessageHandler handler, object parameter)
        {
            sentMessageHandler = handler;
            sentMessageHandlerParameter = parameter;
        }

		/// <summary>
		/// Determines whether the transmit (send) buffer is full. If true the next send command will throw a ConnectionException
		/// </summary>
		/// <returns><c>true</c> if this instance is send buffer full; otherwise, <c>false</c>.</returns>
		public bool IsTransmitBufferFull()
		{
			if (useSendMessageQueue)
				return false;
			else
				return IsSentBufferFull ();
		}
	}
}

