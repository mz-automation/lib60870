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

		private void EncodeIOA(Frame frame, int ioa) {
			frame.SetNextByte((byte)(ioa & 0xff));

			if (parameters.SizeOfIOA > 1)
				frame.SetNextByte((byte)((ioa / 0x100) & 0xff));

			if (parameters.SizeOfIOA > 1)
				frame.SetNextByte((byte)((ioa / 0x10000) & 0xff));
		}

		/// <summary>
		/// Sends the interrogation command.
		/// </summary>
		/// <param name="cot">Cause of transmission</param>
		/// <param name="ca">Common address</param>
		/// <param name="qoi">Qualifier of interrogation</param>
		public void SendInterrogationCommand(CauseOfTransmission cot, int ca, byte qoi) 
		{
			Frame frame = new T104Frame ();

			EncodeIdentificationField (frame, TypeID.C_IC_NA_1, 1, cot, ca);

			EncodeIOA (frame, 0);

			/* encode COI (7.2.6.21) */
			frame.SetNextByte (qoi); /* 20 = station interrogation */

			Console.WriteLine("Encoded C_IC_NA_1 with " + frame.GetMsgSize() + " bytes.");

			sendIMessage (frame);
		}

		/// <summary>
		/// Sends the counter interrogation command (C_CI_NA_1 typeID: 101)
		/// </summary>
		/// <param name="cot">Cause of transmission</param>
		/// <param name="ca">Common address</param>
		/// <param name="qcc">Qualifier of counter interrogation command</param>
		public void SendCounterInterrogationCommand(CauseOfTransmission cot, int ca, byte qcc)
		{
			Frame frame = new T104Frame ();

			EncodeIdentificationField (frame, TypeID.C_CI_NA_1, 1, cot, ca);

			EncodeIOA (frame, 0);

			/* encode QCC */
			frame.SetNextByte (qcc);

			Console.WriteLine("Encoded C_CI_NA_1 with " + frame.GetMsgSize() + " bytes.");

			sendIMessage (frame);
		}
	
		/// <summary>
		/// Sends a read command (C_RD_NA_1 typeID: 102).
		/// </summary>
		/// <param name="cot">Cause of transmission</param>
		/// <param name="ca">Common address</param>
		/// <param name="ioa">Information object address</param>
		public void SendReadCommand(CauseOfTransmission cot, int ca, int ioa)
		{
			Frame frame = new T104Frame ();

			EncodeIdentificationField (frame, TypeID.C_RD_NA_1, 1, cot, ca);

			EncodeIOA (frame, ioa);

			Console.WriteLine("Encoded C_RD_NA_1 with " + frame.GetMsgSize() + " bytes.");

			sendIMessage (frame);
		}

		/// <summary>
		/// Sends a clock synchronization command (C_CS_NA_1 typeID: 103).
		/// </summary>
		/// <param name="cot">Cause of transmission</param>
		/// <param name="ca">Common address</param>
		/// <param name="time">the new time to set</param>
		public void SendClockSyncCommand(CauseOfTransmission cot, int ca, CP56Time2a time)
		{
			Frame frame = new T104Frame ();

			EncodeIdentificationField (frame, TypeID.C_CS_NA_1, 1, cot, ca);

			EncodeIOA (frame, 0);

			frame.AppendBytes (time.GetEncodedValue ());

			Console.WriteLine("Encoded C_CS_NA_1 with " + frame.GetMsgSize() + " bytes.");

			sendIMessage (frame);
		}

		/// <summary>
		/// Sends a test command (C_TS_NA_1 typeID: 104).
		/// </summary>
		/// <param name="cot">Cause of transmission</param>
		/// <param name="ca">Common address</param>
		public void SendTestCommand(CauseOfTransmission cot, int ca)
		{
			Frame frame = new T104Frame ();

			EncodeIdentificationField (frame, TypeID.C_TS_NA_1, 1, cot, ca);

			EncodeIOA (frame, 0);

			frame.SetNextByte (0xcc);
			frame.SetNextByte (0x55);

			Console.WriteLine("Encoded C_TS_NA_1 with " + frame.GetMsgSize() + " bytes.");

			sendIMessage (frame);
		}

		/// <summary>
		/// Sends a reset process command (C_RP_NA_1 typeID: 105).
		/// </summary>
		/// <param name="cot">Cause of transmission</param>
		/// <param name="ca">Common address</param>
		/// <param name="qrp">Qualifier of reset process command</param>
		public void SendResetProcessCommand(CauseOfTransmission cot, int ca, byte qrp)
		{
			Frame frame = new T104Frame ();

			EncodeIdentificationField (frame, TypeID.C_RP_NA_1, 1, cot, ca);

			EncodeIOA (frame, 0);

			frame.SetNextByte (qrp);

			Console.WriteLine("Encoded C_RP_NA_1 with " + frame.GetMsgSize() + " bytes.");

			sendIMessage (frame);
		}


		/// <summary>
		/// Sends a delay acquisition command (C_CD_NA_1 typeID: 106).
		/// </summary>
		/// <param name="cot">Cause of transmission</param>
		/// <param name="ca">Common address</param>
		/// <param name="delay">delay for acquisition</param>
		public void SendDelayAcquisitionCommand(CauseOfTransmission cot, int ca, CP16Time2a delay)
		{
			Frame frame = new T104Frame ();

			EncodeIdentificationField (frame, TypeID.C_CD_NA_1, 1, cot, ca);

			EncodeIOA (frame, 0);

			frame.AppendBytes (delay.GetEncodedValue ());

			Console.WriteLine("Encoded C_CD_NA_1 with " + frame.GetMsgSize() + " bytes.");

			sendIMessage (frame);
		}

		/// <summary>
		/// Sends the control command.
		/// </summary>
		/// <param name="typeId">Type ID of the control command</param>
		/// <param name="cot">Cause of transmission</param>
		/// <param name="ca">Common address</param>
		/// <param name="sc">Information object of the command</param>
		public void SendControlCommand(TypeID typeId, CauseOfTransmission cot, int ca, InformationObject sc) {
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

						if (bytesRec > 0) {

							if (debugOutput)
								Console.WriteLine(
									BitConverter.ToString(bytes, 0, bytesRec));
						
						
							if (checkMessage(socket, bytes, bytesRec) == false) {
								/* close connection on error */
								running = false;
							}
						}
						else
							running = false;

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

