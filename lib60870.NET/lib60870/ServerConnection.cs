/*
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

using lib60870;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Collections.Generic;

namespace lib60870
{
	public class ServerConnection 
	{
		private static int connectionsCounter = 0;

		private int connectionID;

		private void DebugLog(string msg)
		{
			if (debugOutput) {
				Console.Write ("CS104 SLAVE CONNECTION ");
				Console.Write (connectionID);
				Console.Write (": ");
				Console.WriteLine (msg);
			}
		}

		static byte[] STARTDT_CON_MSG = new byte[] { 0x68, 0x04, 0x0b, 0x00, 0x00, 0x00 };

		static byte[] STOPDT_CON_MSG = new byte[] { 0x68, 0x04, 0x23, 0x00, 0x00, 0x00 };

		static byte[] TESTFR_CON_MSG = new byte[] { 0x68, 0x04, 0x83, 0x00, 0x00, 0x00 };

		static byte[] TESTFR_ACT_MSG = new byte[] { 0x68, 0x04, 0x43, 0x00, 0x00, 0x00 };

		private int sendCount = 0;
		private int receiveCount = 0;

		private int unconfirmedReceivedIMessages = 0; /* number of unconfirmed messages received */
		private Int64 lastConfirmationTime = System.Int64.MaxValue; /* timestamp when the last confirmation message was sent */

		/* T3 parameter handling */
		private UInt64 nextT3Timeout;
		private int outStandingTestFRConMessages = 0;

		private ConnectionParameters parameters;
		private Server server;

		private Queue<ASDU> receivedASDUs = null;
		private Thread callbackThread = null;
		private bool callbackThreadRunning = false;

		private Queue<BufferFrame> waitingASDUsHighPrio = null;

		private bool firstIMessageReceived = false;

		/* data structure for k-size sent ASDU buffer */
		private struct SentASDU
		{
			// required to identify message in server (low-priority) queue
			public long entryTime;
			public int queueIndex; /* -1 if ASDU is not from low-priority queue */

			// required for T1 timeout
			public long sentTime;
			public int seqNo;
		}

		private int maxSentASDUs;
		private int oldestSentASDU = -1;
		private int newestSentASDU = -1;
		private SentASDU[] sentASDUs = null;

		private void ProcessASDUs()
		{
			callbackThreadRunning = true;

			while (callbackThreadRunning) {

				while ((receivedASDUs.Count > 0) && (callbackThreadRunning) && (running))
					HandleASDU (receivedASDUs.Dequeue ());

				Thread.Sleep (50);
			}

			DebugLog ("ProcessASDUs exit thread");
		}

		public ServerConnection(Socket socket, ConnectionParameters parameters, Server server) 
		{
			connectionsCounter++;
			connectionID = connectionsCounter;

			this.socket = socket;
			this.parameters = parameters;
			this.server = server;

			ResetT3Timeout ();

			maxSentASDUs = parameters.K;
			this.sentASDUs = new SentASDU[maxSentASDUs];

			//TODO only needed when connection is activated
			receivedASDUs = new Queue<ASDU> ();
			waitingASDUsHighPrio = new Queue<BufferFrame> ();

			Thread workerThread = new Thread(HandleConnection);
			callbackThread = new Thread (ProcessASDUs);

			workerThread.Start ();
			callbackThread.Start ();
		}

		/// <summary>
		/// Gets the connection parameters.
		/// </summary>
		/// <returns>The connection parameters used by the server.</returns>
		public ConnectionParameters GetConnectionParameters()
		{
			return parameters;
		}

		private void ResetT3Timeout() {
			nextT3Timeout = (UInt64) SystemUtils.currentTimeMillis () + (UInt64) (parameters.T3 * 1000);
		}

		/// <summary>
		/// Flag indicating that this connection is the active connection.
		/// The active connection is the only connection that is answering
		/// application layer requests and sends cyclic, and spontaneous messages.
		/// </summary>
		private bool isActive = false;

		/// <summary>
		/// Gets or sets a value indicating whether this connection is active.
		/// The active connection is the only connection that is answering
		/// application layer requests and sends cyclic, and spontaneous messages.
		/// </summary>
		/// <value><c>true</c> if this instance is active; otherwise, <c>false</c>.</value>
		public bool IsActive {
			get {
				return this.isActive;
			}
			set {
				isActive = value;

				if (isActive)
					DebugLog("is active");
				else
					DebugLog("is not active");
			}
		}

		private Socket socket;

		private bool running = false;

		private bool debugOutput = true;

		private int receiveMessage(Socket socket, byte[] buffer) 
		{
			if (socket.Poll (50, SelectMode.SelectRead)) {

				if (socket.Available == 0)
					throw new SocketException ();

				// wait for first byte
				if (socket.Receive (buffer, 0, 1, SocketFlags.None) != 1)
					return 0;

				if (buffer [0] != 0x68) {
					DebugLog("Missing SOF indicator!");
					return 0;
				}

				// read length byte
				if (socket.Receive (buffer, 1, 1, SocketFlags.None) != 1)
					return 0;

				int length = buffer [1];

				// read remaining frame
				if (socket.Receive (buffer, 2, length, SocketFlags.None) != length) {
					DebugLog("Failed to read complete frame!");
					return 0;
				}

				return length + 2;
			} else
				return 0;
		}
			
		private void SendSMessage() 
		{
			DebugLog("Send S message");

			byte[] msg = new byte[6];

			msg [0] = 0x68;
			msg [1] = 0x04;
			msg [2] = 0x01;
			msg [3] = 0;

			lock (socket) {
				msg [4] = (byte)((receiveCount % 128) * 2);
				msg [5] = (byte)(receiveCount / 128);

				try {
					socket.Send (msg);
				}
				catch (SocketException) {
					// socket error --> close connection
					running = false;
				}
			}
		}

		private int SendIMessage(BufferFrame asdu)
		{

			byte[] buffer = asdu.GetBuffer ();

			int msgSize = asdu.GetMsgSize () + 6; /* ASDU size + ACPI size */

			buffer [0] = 0x68;

			/* set size field */
			buffer [1] = (byte) (msgSize - 2);

			buffer [2] = (byte) ((sendCount % 128) * 2);
			buffer [3] = (byte) (sendCount / 128);

			buffer [4] = (byte) ((receiveCount % 128) * 2);
			buffer [5] = (byte) (receiveCount / 128);

			try {
				lock (socket) {
					socket.Send (buffer, msgSize, SocketFlags.None);
					sendCount = (sendCount + 1) % 32768;
				}
			}
			catch (SocketException) {
				// socket error --> close connection
				running = false;
			}
				
			unconfirmedReceivedIMessages = 0;

			return sendCount;;
		}

		private bool isSentBufferFull() {

			if (oldestSentASDU == -1)
				return false;

			int newIndex = (newestSentASDU + 1) % maxSentASDUs;

			if (newIndex == oldestSentASDU)
				return true;
			else
				return false;
		}

		private void PrintSendBuffer() {

			if (oldestSentASDU != -1) {

				int currentIndex = oldestSentASDU;

				int nextIndex = 0;

				DebugLog ("------k-buffer------");

				do {
					DebugLog (currentIndex + " : S " + sentASDUs[currentIndex].seqNo + " : time " +
						sentASDUs[currentIndex].sentTime + " : " + sentASDUs[currentIndex].queueIndex);

					if (currentIndex == newestSentASDU)
						nextIndex = -1;

					currentIndex = (currentIndex + 1) % maxSentASDUs;

				} while (nextIndex != -1);

				DebugLog ("--------------------");
					
			}

		}

		private void sendNextAvailableASDU() {

			lock (sentASDUs) {
				if (isSentBufferFull ())
					return;

				long timestamp;
				int index;

				server.LockASDUQueue ();

				BufferFrame asdu = server.GetNextWaitingASDU (out timestamp, out index);

				try {

					if (asdu != null) {

						int currentIndex = 0;

						if (oldestSentASDU == -1) {
							oldestSentASDU = 0;
							newestSentASDU = 0;

						} else {
							currentIndex = (newestSentASDU + 1) % maxSentASDUs;
						}
							
						sentASDUs [currentIndex].entryTime = timestamp;
						sentASDUs [currentIndex].queueIndex = index;
						sentASDUs [currentIndex].seqNo = SendIMessage (asdu);
						sentASDUs [currentIndex].sentTime = SystemUtils.currentTimeMillis ();

						newestSentASDU = currentIndex;

						PrintSendBuffer ();
					}
				}
				finally {

					server.UnlockASDUQueue ();
				}
			}
		}

		private bool sendNextHighPriorityASDU() {
			lock (sentASDUs) {
				if (isSentBufferFull ())
					return false;

				BufferFrame asdu = waitingASDUsHighPrio.Dequeue ();

				if (asdu != null) {

					int currentIndex = 0;

					if (oldestSentASDU == -1) {
						oldestSentASDU = 0;
						newestSentASDU = 0;

					} else {
						currentIndex = (newestSentASDU + 1) % maxSentASDUs;
					}
						
					sentASDUs [currentIndex].queueIndex = -1;
					sentASDUs [currentIndex].seqNo = SendIMessage (asdu);
					sentASDUs [currentIndex].sentTime = SystemUtils.currentTimeMillis ();

					newestSentASDU = currentIndex;

					PrintSendBuffer ();
				} else
					return false;
			}

			return true;
		}



		private void SendWaitingASDUs()
		{

			lock (waitingASDUsHighPrio) {

				while (waitingASDUsHighPrio.Count > 0) {

					if (sendNextHighPriorityASDU () == false)
						return;

					if (running == false)
						return;
				}
			}

			// send messages from low-priority queue
			sendNextAvailableASDU();
		}

		private void SendASDUInternal(ASDU asdu)
		{
			if (isActive) {
				lock (waitingASDUsHighPrio) {

					BufferFrame frame = new BufferFrame (new byte[260], 6);

					asdu.Encode (frame, parameters);

					waitingASDUsHighPrio.Enqueue (frame);
				}

				SendWaitingASDUs ();
			} 
		}

		public void SendASDU(ASDU asdu) 
		{
			if (isActive)
				SendASDUInternal (asdu);
			else
				throw new ConnectionException ("Connection not active");
		}

		public void SendACT_CON(ASDU asdu, bool negative) {
			asdu.Cot = CauseOfTransmission.ACTIVATION_CON;
			asdu.IsNegative = negative;

			SendASDU (asdu);
		}

		public void SendACT_TERM(ASDU asdu) {
			asdu.Cot = CauseOfTransmission.ACTIVATION_TERMINATION;
			asdu.IsNegative = false;

			SendASDU (asdu);
		}
			
		private void HandleASDU(ASDU asdu)
		{		
			DebugLog ("Handle received ASDU");

			bool messageHandled = false;

			switch (asdu.TypeId) {

			case TypeID.C_IC_NA_1: /* 100 - interrogation command */

				DebugLog("Rcvd interrogation command C_IC_NA_1");

				if ((asdu.Cot == CauseOfTransmission.ACTIVATION) || (asdu.Cot == CauseOfTransmission.DEACTIVATION)) {
					if (server.interrogationHandler != null) {

						InterrogationCommand irc = (InterrogationCommand)asdu.GetElement (0);

						if (server.interrogationHandler (server.InterrogationHandlerParameter, this, asdu, irc.QOI))
							messageHandled = true;
					}
				} else {
					asdu.Cot = CauseOfTransmission.UNKNOWN_CAUSE_OF_TRANSMISSION;
					this.SendASDUInternal (asdu);
				}

				break;

			case TypeID.C_CI_NA_1: /* 101 - counter interrogation command */

				DebugLog("Rcvd counter interrogation command C_CI_NA_1");

				if ((asdu.Cot == CauseOfTransmission.ACTIVATION) || (asdu.Cot == CauseOfTransmission.DEACTIVATION)) {
					if (server.counterInterrogationHandler != null) {

						CounterInterrogationCommand cic = (CounterInterrogationCommand)asdu.GetElement (0);

						if (server.counterInterrogationHandler (server.counterInterrogationHandlerParameter, this, asdu, cic.QCC))
							messageHandled = true;
					}
				} else {
					asdu.Cot = CauseOfTransmission.UNKNOWN_CAUSE_OF_TRANSMISSION;
					this.SendASDUInternal (asdu);
				}

				break;

			case TypeID.C_RD_NA_1: /* 102 - read command */

				DebugLog("Rcvd read command C_RD_NA_1");

				if (asdu.Cot == CauseOfTransmission.REQUEST) {

					DebugLog("Read request for object: " + asdu.Ca);

					if (server.readHandler != null) {
						ReadCommand rc = (ReadCommand)asdu.GetElement (0);

						if (server.readHandler (server.readHandlerParameter, this, asdu, rc.ObjectAddress))
							messageHandled = true;

					}

				} else {
					asdu.Cot = CauseOfTransmission.UNKNOWN_CAUSE_OF_TRANSMISSION;
					this.SendASDUInternal (asdu);
				}

				break;

			case TypeID.C_CS_NA_1: /* 103 - Clock synchronization command */

				DebugLog("Rcvd clock sync command C_CS_NA_1");

				if (asdu.Cot == CauseOfTransmission.ACTIVATION) {

					if (server.clockSynchronizationHandler != null) {

						ClockSynchronizationCommand csc = (ClockSynchronizationCommand)asdu.GetElement (0);

						if (server.clockSynchronizationHandler (server.clockSynchronizationHandlerParameter,
							    this, asdu, csc.NewTime))
							messageHandled = true;
					}

				} else {
					asdu.Cot = CauseOfTransmission.UNKNOWN_CAUSE_OF_TRANSMISSION;
					this.SendASDUInternal (asdu);
				}

				break;

			case TypeID.C_TS_NA_1: /* 104 - test command */

				DebugLog("Rcvd test command C_TS_NA_1");

				if (asdu.Cot != CauseOfTransmission.ACTIVATION)
					asdu.Cot = CauseOfTransmission.UNKNOWN_CAUSE_OF_TRANSMISSION;
				else
					asdu.Cot = CauseOfTransmission.ACTIVATION_CON;

				this.SendASDUInternal (asdu);

				messageHandled = true;

				break;

			case TypeID.C_RP_NA_1: /* 105 - Reset process command */

				DebugLog("Rcvd reset process command C_RP_NA_1");

				if (asdu.Cot == CauseOfTransmission.ACTIVATION) {

					if (server.resetProcessHandler != null) {

						ResetProcessCommand rpc = (ResetProcessCommand)asdu.GetElement (0);

						if (server.resetProcessHandler (server.resetProcessHandlerParameter,
							    this, asdu, rpc.QRP))
							messageHandled = true;
					}

				} else {
					asdu.Cot = CauseOfTransmission.UNKNOWN_CAUSE_OF_TRANSMISSION;
					this.SendASDUInternal (asdu);
				}


				break;

			case TypeID.C_CD_NA_1: /* 106 - Delay acquisition command */

				DebugLog("Rcvd delay acquisition command C_CD_NA_1");

				if ((asdu.Cot == CauseOfTransmission.ACTIVATION) || (asdu.Cot == CauseOfTransmission.SPONTANEOUS)) {
					if (server.delayAcquisitionHandler != null) {

						DelayAcquisitionCommand dac = (DelayAcquisitionCommand)asdu.GetElement (0);

						if (server.delayAcquisitionHandler (server.delayAcquisitionHandlerParameter,
							    this, asdu, dac.Delay))
							messageHandled = true;
					}
				} else {
					asdu.Cot = CauseOfTransmission.UNKNOWN_CAUSE_OF_TRANSMISSION;
					this.SendASDUInternal (asdu);
				}

				break;

			}

			if ((messageHandled == false) && (server.asduHandler != null))
				if (server.asduHandler (server.asduHandlerParameter, this, asdu))
					messageHandled = true;

			if (messageHandled == false) {
				asdu.Cot = CauseOfTransmission.UNKNOWN_TYPE_ID;
				this.SendASDUInternal (asdu);
			}

		}

		private bool CheckSequenceNumber(int seqNo) {

			lock (sentASDUs) {

				/* check if received sequence number is valid */

				bool seqNoIsValid = false;
				bool counterOverflowDetected = false;

				if (oldestSentASDU == -1) { /* if k-Buffer is empty */

					if (seqNo == sendCount)
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
							if (seqNo < sentASDUs [oldestSentASDU].seqNo) {
								break;
							}
						}
						else {
							if (seqNo == ((sentASDUs [oldestSentASDU].seqNo - 1) % 32768)) {
								break;
							}
						}

						/* remove from server (low-priority) queue if required */
						if (sentASDUs [oldestSentASDU].queueIndex != -1) {
							server.MarkASDUAsConfirmed (sentASDUs [oldestSentASDU].queueIndex,
								sentASDUs [oldestSentASDU].entryTime);
						}

						oldestSentASDU = (oldestSentASDU + 1) % maxSentASDUs;

						int checkIndex = (newestSentASDU + 1) % maxSentASDUs;

						if (oldestSentASDU == checkIndex) {
							oldestSentASDU = -1;
							break;
						}
							
						if (sentASDUs [oldestSentASDU].seqNo == seqNo) {
							/* we arrived at the seq# that has been confirmed */

							if (oldestSentASDU == newestSentASDU)
								oldestSentASDU = -1;
							else 
								oldestSentASDU = (oldestSentASDU + 1) % maxSentASDUs;

							break;
						}

					} while (true);
				}
			}

			return true;
		}

		private bool HandleMessage(Socket socket, byte[] buffer, int msgSize)
		{
			long currentTime = SystemUtils.currentTimeMillis ();

			if ((buffer [2] & 1) == 0) {

				if (firstIMessageReceived == false) {
					firstIMessageReceived = true;
					lastConfirmationTime = currentTime; /* start timeout T2 */
				}

				if (msgSize < 7) {

					DebugLog ("I msg too small!");

					return false;
				}
		
				int frameSendSequenceNumber = ((buffer [3] * 0x100) + (buffer [2] & 0xfe)) / 2;
				int frameRecvSequenceNumber = ((buffer [5] * 0x100) + (buffer [4] & 0xfe)) / 2;

				DebugLog ("Received I frame: N(S) = " + frameSendSequenceNumber + " N(R) = " + frameRecvSequenceNumber);

				/* check the receive sequence number N(R) - connection will be closed on an unexpected value */
				if (frameSendSequenceNumber != receiveCount) {
					DebugLog ("Sequence error: Close connection!");
					return false;
				}

				if (CheckSequenceNumber (frameRecvSequenceNumber) == false) {
					DebugLog ("Sequence number check failed");
					return false;
				}

				receiveCount = (receiveCount + 1) % 32768;
				unconfirmedReceivedIMessages++;

				if (isActive) {
					ASDU asdu = new ASDU (parameters, buffer, msgSize);
				
					// push to handler thread for processing
					DebugLog ("Enqueue received I-message for processing");
					receivedASDUs.Enqueue (asdu);
				} else {
					// connection not activated --> skip message
					DebugLog ("Connection not activated. Skip I message");
				}
			}

			// Check for TESTFR_ACT message
			else if ((buffer [2] & 0x43) == 0x43) {

				DebugLog ("Send TESTFR_CON");

				socket.Send (TESTFR_CON_MSG);
			} 

			// Check for STARTDT_ACT message
			else if ((buffer [2] & 0x07) == 0x07) {

				DebugLog ("Send STARTDT_CON");

				this.isActive = true;

				socket.Send (STARTDT_CON_MSG);
			}

			// Check for STOPDT_ACT message
			else if ((buffer [2] & 0x13) == 0x13) {
				
				DebugLog ("Send STOPDT_CON");

				this.isActive = false;

				socket.Send (STOPDT_CON_MSG);
			} 

			// Check for TESTFR_CON message
			else if ((buffer [2] & 0x83) == 0x83) {
				DebugLog ("Recv TESTFR_CON");

				outStandingTestFRConMessages = 0;
			}

			// S-message
			else if (buffer [2] == 0x01) {

				int seqNo = (buffer[4] + buffer[5] * 0x100) / 2;

				DebugLog("Recv S(" + seqNo + ") (own sendcounter = " + sendCount + ")");

				if (CheckSequenceNumber (seqNo) == false)
					return false;
					
			}
			else {
				DebugLog("Unknown message");
			}

			ResetT3Timeout ();

			return true;
		}

		private bool handleTimeouts() {
			UInt64 currentTime = (UInt64) SystemUtils.currentTimeMillis();

			if (currentTime > nextT3Timeout) {

				if (outStandingTestFRConMessages > 2) {
					DebugLog("Timeout for TESTFR_CON message");

					// close connection
					return false;
				} else {
					try  {
						socket.Send (TESTFR_ACT_MSG);

						DebugLog("U message T3 timeout");
						outStandingTestFRConMessages++;
						ResetT3Timeout ();
					}
					catch (SocketException) {
						running = false;
					}
				}
			}

			if (unconfirmedReceivedIMessages > 0) {

				if (((long) currentTime - lastConfirmationTime) >= (parameters.T2 * 1000)) {

					lastConfirmationTime = (long) currentTime;
					unconfirmedReceivedIMessages = 0;
					SendSMessage ();
				}
			}
				
			/* check if counterpart confirmed I messages */
			lock (sentASDUs) {
			
				if (oldestSentASDU != -1) {

					if (((long)currentTime - sentASDUs [oldestSentASDU].sentTime) >= (parameters.T1 * 1000)) {

						PrintSendBuffer ();
						DebugLog("I message timeout for " + oldestSentASDU + " seqNo: " + sentASDUs[oldestSentASDU].seqNo);
						return false;
					}
				}
			}

			return true;
		}

		private void HandleConnection() {

			byte[] bytes = new byte[300];

			try {

				try {

					running = true;

					while (running) {

						try {
							// Receive the response from the remote device.
							int bytesRec = receiveMessage(socket, bytes);
							
							if (bytesRec != 0) {
							
								DebugLog("RCVD: " +	BitConverter.ToString(bytes, 0, bytesRec));
							
								if (HandleMessage(socket, bytes, bytesRec) == false) {
									/* close connection on error */
									running = false;
								}

								if (unconfirmedReceivedIMessages >= parameters.W) {
									lastConfirmationTime = SystemUtils.currentTimeMillis();

									unconfirmedReceivedIMessages = 0;
									SendSMessage ();
								}	
							}

						} catch (SocketException) {
							running = false;
						}

						if (handleTimeouts() == false)
							running = false;

						if (running) {
							if (isActive)
								SendWaitingASDUs();

							Thread.Sleep(1);
						}
					}

					isActive = false;

					DebugLog("CLOSE CONNECTION!");

					// Release the socket.
					socket.Shutdown(SocketShutdown.Both);
					socket.Close();

					DebugLog("CONNECTION CLOSED!");

				} catch (ArgumentNullException ane) {
					DebugLog("ArgumentNullException : " + ane.ToString());
				} catch (SocketException se) {
					DebugLog("SocketException : " + se.ToString());
				} catch (Exception e) {
					DebugLog("Unexpected exception : " + e.ToString());
				}

			} catch (Exception e) {
				Console.WriteLine( e.ToString());
			}

			callbackThreadRunning = false;

			// unmark unconfirmed messages in server queue if k-buffer not empty
			if (oldestSentASDU != -1)
				server.UnmarkAllASDUs ();

			server.Remove (this);

			callbackThread.Join ();

			DebugLog("Connection thread finished");
		}
			
		public void Close() 
		{
			running = false;
		}
			
		public void ASDUReadyToSend () 
		{
			if (isActive)
				SendWaitingASDUs ();
		}

	}
	
}
