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
		public void TestStepPositionInformation() 
		{
			StepPositionInformation spi = new StepPositionInformation (103, 27, false, new QualityDescriptor ());

			Assert.IsFalse (spi.Transient);
			Assert.NotNull (spi.Quality);

			spi = null;

			try {
				spi = new StepPositionInformation (103, 64, false, new QualityDescriptor ());
			}
			catch (ArgumentOutOfRangeException) {
			}

			Assert.IsNull (spi);

			try {
				spi = new StepPositionInformation (103, -65, false, new QualityDescriptor ());
			}
			catch (ArgumentOutOfRangeException) {
			}
		}

		[Test()]
		//[Ignore("Ignore to save execution time")]
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
		//[Ignore("Ignore to save execution time")]
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
			ConnectionParameters cp = new ConnectionParameters ();

			ASDU asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);

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
			ConnectionParameters cp = new ConnectionParameters ();

			ASDU asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);

			int addedCounter = 0;
			int ioa = 100;

			while (asdu.AddInformationObject (new SinglePointInformation (ioa, false, new QualityDescriptor()))) {
				ioa++;
				addedCounter++;
			}
		
			Assert.AreEqual (60, addedCounter); 

			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, true);

			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new SinglePointInformation (ioa, false, new QualityDescriptor()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (240, addedCounter); 

			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);

			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new SinglePointWithCP24Time2a (ioa, false, new QualityDescriptor(), new CP24Time2a()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (34, addedCounter); 

			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, true);

			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new SinglePointWithCP56Time2a (ioa, false, new QualityDescriptor(), new CP56Time2a()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (30, addedCounter); 

			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);

			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new MeasuredValueShortWithCP56Time2a (ioa, 0.0f, QualityDescriptor.VALID(), new CP56Time2a ()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (16, addedCounter); 
		}

		[Test()]
		public void TestEncodeASDUsWithManyInformationObjects() {
			ConnectionParameters cp = new ConnectionParameters ();

			ASDU asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);
			int addedCounter = 0;
			int ioa = 100;

			while (asdu.AddInformationObject (new SinglePointInformation (ioa, false, new QualityDescriptor()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (60, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new SinglePointWithCP24Time2a (ioa, true, new QualityDescriptor(), new CP24Time2a()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (34, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new SinglePointWithCP56Time2a (ioa, true, new QualityDescriptor(), new CP56Time2a()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (22, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new DoublePointInformation (ioa, DoublePointValue.ON, new QualityDescriptor()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (60, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new DoublePointWithCP24Time2a (ioa, DoublePointValue.ON, new QualityDescriptor(), new CP24Time2a()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (34, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new DoublePointWithCP56Time2a (ioa, DoublePointValue.ON, new QualityDescriptor(), new CP56Time2a()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (22, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());
	

			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new MeasuredValueNormalized (ioa, 1f, new QualityDescriptor()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (40, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new MeasuredValueNormalizedWithCP24Time2a (ioa, 1f, new QualityDescriptor(), new CP24Time2a()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (27, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new MeasuredValueNormalizedWithCP56Time2a (ioa, 1f, new QualityDescriptor(), new CP56Time2a()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (18, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new MeasuredValueScaled (ioa, 0, new QualityDescriptor()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (40, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new MeasuredValueScaledWithCP24Time2a (ioa, 0, new QualityDescriptor(), new CP24Time2a()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (27, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new MeasuredValueScaledWithCP56Time2a (ioa, 0, new QualityDescriptor(), new CP56Time2a()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (18, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new MeasuredValueShort (ioa, 0f, new QualityDescriptor()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (30, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new MeasuredValueShortWithCP24Time2a (ioa, 0f, new QualityDescriptor(), new CP24Time2a()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (22, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new MeasuredValueShortWithCP56Time2a (ioa, 0f, new QualityDescriptor(), new CP56Time2a()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (16, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new StepPositionInformation (ioa, 0, false, new QualityDescriptor()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (48, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new StepPositionWithCP24Time2a (ioa, 0, false, new QualityDescriptor(), new CP24Time2a()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (30, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new StepPositionWithCP56Time2a (ioa, 0, false, new QualityDescriptor(), new CP56Time2a()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (20, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new IntegratedTotals (ioa, new BinaryCounterReading()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (30, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new IntegratedTotalsWithCP24Time2a (ioa, new BinaryCounterReading(), new CP24Time2a()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (22, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new IntegratedTotalsWithCP56Time2a (ioa, new BinaryCounterReading(), new CP56Time2a()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (16, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());

			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new EventOfProtectionEquipment (ioa, new SingleEvent(), new CP16Time2a(10), new CP24Time2a()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (27, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, false);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new EventOfProtectionEquipmentWithCP56Time2a (ioa, new SingleEvent(), new CP16Time2a(10), new CP56Time2a()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (18, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());

			//TODO add missing tests
		}

		[Test()]
		public void TestEncodeASDUsWithManyInformationObjectsSequenceOfIO() {

			ConnectionParameters cp = new ConnectionParameters ();

			ASDU asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, true);
			int addedCounter = 0;
			int ioa = 100;

			while (asdu.AddInformationObject (new SinglePointInformation (ioa, false, new QualityDescriptor()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (240, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, true);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new DoublePointInformation (ioa, DoublePointValue.OFF, new QualityDescriptor()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (240, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, true);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new MeasuredValueNormalized (ioa, 1f, new QualityDescriptor()))) {
				ioa++;
				addedCounter++;
			}


			Assert.AreEqual (80, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());

			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, true);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new MeasuredValueScaled (ioa, 0, new QualityDescriptor()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (80, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, true);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new MeasuredValueShort (ioa, 0f, new QualityDescriptor()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (48, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, true);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new StepPositionInformation (ioa, 0, false, new QualityDescriptor()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (120, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());


			asdu = new ASDU (cp, CauseOfTransmission.PERIODIC, false, false, 0, 1, true);
			addedCounter = 0;
			ioa = 100;

			while (asdu.AddInformationObject (new IntegratedTotals (ioa, new BinaryCounterReading()))) {
				ioa++;
				addedCounter++;
			}

			Assert.AreEqual (48, addedCounter);
			Assert.NotNull (asdu.AsByteArray ());
		}
			
		[Test()]
		//[Ignore("Ignore to save execution time")]
		public void TestSendTestFR() {
			ConnectionParameters clientParameters = new ConnectionParameters ();
			ConnectionParameters serverParameters = new ConnectionParameters ();

			clientParameters.T3 = 1;

			Server server = new Server (serverParameters);

			server.SetLocalPort (20213);

			server.Start ();

			Connection connection = new Connection ("127.0.0.1", 20213, clientParameters);

			connection.Connect ();

			ASDU asdu = new ASDU (clientParameters, CauseOfTransmission.SPONTANEOUS, false, false, 0, 1, false);
			asdu.AddInformationObject (new SinglePointInformation (100, false, new QualityDescriptor()));

			connection.SendASDU(asdu);

			Assert.AreEqual (2, connection.GetStatistics ().SentMsgCounter); /* STARTDT + ASDU */

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
		//[Ignore("Ignore to save execution time")]
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

			ASDU asdu = new ASDU (clientParameters, CauseOfTransmission.SPONTANEOUS, false, false, 0, 1, false);
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
		//[Ignore("Ignore to save execution time")]
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


			// Connection is closed. SendASDU should fail
			try {
				ASDU asdu = new ASDU (clientParameters, CauseOfTransmission.SPONTANEOUS, false, false, 0, 1, false);
				asdu.AddInformationObject (new SinglePointInformation (100, false, new QualityDescriptor()));

				connection.SendASDU(asdu);
			}
			catch (ConnectionException) {
			}
				

			while (connection.IsRunning == true)
				Thread.Sleep (10);

			connection.Close ();
			server.Stop ();

		//	Assert.AreEqual (5, connection.GetStatistics ().RcvdMsgCounter); /* STARTDT_CON + ASDU + TESTFR_CON */

		//	Assert.AreEqual (0, connection.GetStatistics ().RcvdTestFrConCounter);
		}
	}
}

