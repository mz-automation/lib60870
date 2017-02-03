using NUnit.Framework;
using System;
using lib60870;
using System.Threading;
using System.Net.Sockets;

namespace tests
{
	[TestFixture ()]
	public class Test
	{
		[Test ()]
		public void TestBCR ()
		{
			BinaryCounterReading bcr = new BinaryCounterReading ();

			bcr.Value = 1000;

			Assert.AreEqual (1000, bcr.Value);

			bcr.Value = -1000;

			Assert.AreEqual (-1000, bcr.Value);

			bcr.SequenceNumber = 31;

			Assert.AreEqual (31, bcr.SequenceNumber);

			bcr.SequenceNumber = 0;

			Assert.AreEqual (0, bcr.SequenceNumber);

			/* Out of range sequenceNumber */
			bcr.SequenceNumber = 32;

			Assert.AreEqual (0, bcr.SequenceNumber);

			bcr = new BinaryCounterReading ();

			bcr.Invalid = true;

			Assert.AreEqual (true, bcr.Invalid);
			Assert.AreEqual (false, bcr.Carry);
			Assert.AreEqual (false, bcr.Adjusted);
			Assert.AreEqual (0, bcr.SequenceNumber);
			Assert.AreEqual (0, bcr.Value);

			bcr = new BinaryCounterReading ();

			bcr.Carry = true;

			Assert.AreEqual (false, bcr.Invalid);
			Assert.AreEqual (true, bcr.Carry);
			Assert.AreEqual (false, bcr.Adjusted);
			Assert.AreEqual (0, bcr.SequenceNumber);
			Assert.AreEqual (0, bcr.Value);

			bcr = new BinaryCounterReading ();

			bcr.Adjusted = true;

			Assert.AreEqual (false, bcr.Invalid);
			Assert.AreEqual (false, bcr.Carry);
			Assert.AreEqual (true, bcr.Adjusted);
			Assert.AreEqual (0, bcr.SequenceNumber);
			Assert.AreEqual (0, bcr.Value);


		}

		[Test()]
		public void TestSetpointCommandNormalized()
		{
			SetpointCommandNormalized sc = new SetpointCommandNormalized (102, -0.5f,
				new SetpointCommandQualifier (true, 0));

			Assert.AreEqual (102, sc.ObjectAddress);

			Assert.AreEqual (-0.5f, sc.NormalizedValue, 0.001f);
		
			Assert.AreEqual (true, sc.QOS.Select);

		}

		[Test()]
		[Ignore("Ignore to save execution time")]
		public void TestConnectWhileAlreadyConnected()
		{
			ConnectionParameters parameters = new ConnectionParameters ();

			Server server = new Server (parameters);

			server.SetLocalPort (20213);

			server.Start ();

			Connection connection = new Connection ("127.0.0.1", 20213, parameters);

			ConnectionException se = null;

			try {
				connection.Connect ();
			}
			catch (ConnectionException ex) {
				se = ex;
			}

			Assert.IsNull (se);

			Thread.Sleep (100);

			try {
				connection.Connect ();
			}
			catch (ConnectionException ex) {
				se = ex;
			}

			Assert.IsNotNull (se);
			Assert.AreEqual (se.Message, "already connected");
			Assert.AreEqual (10056, ((SocketException)se.InnerException).ErrorCode);

			connection.Close ();

			server.Stop ();
		}


		[Test()]
		[Ignore("Ignore to save execution time")]
		public void TestConnectSameConnectionMultipleTimes()
		{
			ConnectionParameters parameters = new ConnectionParameters ();

			Server server = new Server (parameters);

			server.SetLocalPort (20213);

			server.Start ();

			Connection connection = new Connection ("127.0.0.1", 20213, parameters);

			SocketException se = null;

			try {
				connection.Connect ();

				connection.Close ();
			}
			catch (SocketException ex) {
				se = ex;
			}

			Assert.IsNull (se);

			try {
				connection.Connect ();

				connection.Close ();
			}
			catch (SocketException ex) {
				se = ex;
			}

			Assert.Null (se);

			connection.Close ();

			server.Stop ();
		}

		[Test()]
		public void TestASDUAddInformationObjects() {
			ASDU asdu = new ASDU (CauseOfTransmission.PERIODIC, false, false, 0, 1, false);

			asdu.AddInformationObject (new SinglePointInformation (100, false, new QualityDescriptor()));
			asdu.AddInformationObject (new SinglePointInformation (101, false, new QualityDescriptor()));

			// wrong InformationObject type expect exception
			ArgumentException ae = null;

			try {
				asdu.AddInformationObject (new DoublePointInformation (102, DoublePointValue.ON, new QualityDescriptor()));
			}
			catch(ArgumentException e) {
				ae = e;
			}

			Assert.NotNull (ae);
		}

		[Test()]
		public void TestASDUAddTooMuchInformationObjects() {
			ASDU asdu = new ASDU (CauseOfTransmission.PERIODIC, false, false, 0, 1, false);

			asdu.AddInformationObject (new SinglePointInformation (100, false, new QualityDescriptor()));
			asdu.AddInformationObject (new SinglePointInformation (101, false, new QualityDescriptor()));

			//TODO implement test case
		}
			
		[Test()]
		[Ignore("Ignore to save execution time")]
		public void TestSendTestFR() {
			ConnectionParameters clientParameters = new ConnectionParameters ();
			ConnectionParameters serverParameters = new ConnectionParameters ();

			clientParameters.T3 = 1;

			Server server = new Server (serverParameters);

			server.SetLocalPort (20213);

			server.Start ();

			Connection connection = new Connection ("127.0.0.1", 20213, clientParameters);

			connection.Connect ();

			ASDU asdu = new ASDU (CauseOfTransmission.SPONTANEOUS, false, false, 0, 1, false);
			asdu.AddInformationObject (new SinglePointInformation (100, false, new QualityDescriptor()));

			connection.SendASDU(asdu);

			Assert.AreEqual (2, connection.GetStatistics ().SentMsgCounter); /* STARTDT + ASDU */
			Assert.AreEqual (1, connection.GetStatistics ().RcvdMsgCounter); /* STARTDT_CON */

			while (connection.GetStatistics ().RcvdMsgCounter < 2)
				Thread.Sleep (1);

			Assert.AreEqual (2, connection.GetStatistics ().RcvdMsgCounter); /* STARTDT_CON + ASDU */

			Thread.Sleep (2500);

			connection.Close ();
			server.Stop ();

			Assert.AreEqual (4, connection.GetStatistics ().RcvdMsgCounter); /* STARTDT_CON + ASDU + TESTFR_CON */

			Assert.AreEqual (2, connection.GetStatistics ().RcvdTestFrConCounter);
		}


		private static bool testSendTestFRTimeoutMasterRawMessageHandler(object param, byte[] msg, int msgSize)
		{
			// intercept TESTFR_CON message
			if ((msgSize == 6) && (msg [2] == 0x83))
				return false;
			else
				return true;
		}

		/// <summary>
		/// This test checks that the connection will be closed when the master
		/// doesn't receive the TESTFR_CON messages
		/// </summary>
		[Test()]
		[Ignore("Ignore to save execution time")]
		public void TestSendTestFRTimeoutMaster() {
			ConnectionParameters clientParameters = new ConnectionParameters ();
			ConnectionParameters serverParameters = new ConnectionParameters ();

			clientParameters.T3 = 1;

			Server server = new Server (serverParameters);

			server.SetLocalPort (20213);

			server.Start ();

			Connection connection = new Connection ("127.0.0.1", 20213, clientParameters);

			connection.Connect ();

			connection.SetRawMessageHandler (testSendTestFRTimeoutMasterRawMessageHandler, null);

			ASDU asdu = new ASDU (CauseOfTransmission.SPONTANEOUS, false, false, 0, 1, false);
			asdu.AddInformationObject (new SinglePointInformation (100, false, new QualityDescriptor()));

			connection.SendASDU(asdu);

			Assert.AreEqual (2, connection.GetStatistics ().SentMsgCounter); /* STARTDT + ASDU */

			while (connection.GetStatistics ().RcvdMsgCounter < 2)
				Thread.Sleep (1);

			Assert.AreEqual (2, connection.GetStatistics ().RcvdMsgCounter); /* STARTDT_CON + ASDU */

			Thread.Sleep (6000);

			// Expect connection to be closed due to three missing TESTFR_CON responses
			Assert.IsFalse(connection.IsRunning);

			ConnectionException ce = null;
		
			// Connection is closed. SendASDU should fail
			try {
				connection.SendASDU(asdu);
			}
			catch (ConnectionException e) {
				ce = e;
			}

			Assert.IsNotNull (ce);
			Assert.AreEqual ("not connected", ce.Message);

			connection.Close ();
			server.Stop ();

			Assert.AreEqual (5, connection.GetStatistics ().RcvdMsgCounter); /* STARTDT_CON + ASDU + TESTFR_CON */

			Assert.AreEqual (0, connection.GetStatistics ().RcvdTestFrConCounter);
		}

		private static bool testSendTestFRTimeoutSlaveRawMessageHandler(object param, byte[] msg, int msgSize)
		{
			// intercept TESTFR_ACT messages for so that the master doesn't response
			if ((msgSize == 6) && (msg [2] == 0x43))
				return false;
			else
				return true;
		}

		/// <summary>
		/// This test checks that the connection will be closed when the master
		/// doesn't send the TESTFR_CON messages
		/// </summary>
		[Test()]
		public void TestSendTestFRTimeoutSlave() {
			ConnectionParameters clientParameters = new ConnectionParameters ();
			ConnectionParameters serverParameters = new ConnectionParameters ();

			serverParameters.T3 = 1;

			Server server = new Server (serverParameters);

			server.SetLocalPort (20213);

			server.Start ();

			Connection connection = new Connection ("127.0.0.1", 20213, clientParameters);

			connection.DebugOutput = true;
			connection.SetRawMessageHandler (testSendTestFRTimeoutSlaveRawMessageHandler, null);

			connection.Connect ();

			Assert.AreEqual (1, connection.GetStatistics ().SentMsgCounter); /* STARTDT */

			while (connection.GetStatistics ().RcvdMsgCounter < 1)
				Thread.Sleep (1);

			Assert.AreEqual (1, connection.GetStatistics ().RcvdMsgCounter); /* STARTDT_CON */

			Thread.Sleep (6000);

			// Expect connection to be closed due to three missing TESTFR_CON responses
			//Assert.IsFalse(connection.IsRunning);

			ConnectionException ce = null;

			// Connection is closed. SendASDU should fail
			try {
				ASDU asdu = new ASDU (CauseOfTransmission.SPONTANEOUS, false, false, 0, 1, false);
				asdu.AddInformationObject (new SinglePointInformation (100, false, new QualityDescriptor()));

				connection.SendASDU(asdu);
			}
			catch (ConnectionException e) {
				ce = e;
			}

		//	Assert.IsNotNull (ce);
	//		Assert.AreEqual ("not connected", ce.Message);

			while (connection.IsRunning == true)
				Thread.Sleep (10);

			connection.Close ();
			server.Stop ();

		//	Assert.AreEqual (5, connection.GetStatistics ().RcvdMsgCounter); /* STARTDT_CON + ASDU + TESTFR_CON */

		//	Assert.AreEqual (0, connection.GetStatistics ().RcvdTestFrConCounter);
		}
	}
}

