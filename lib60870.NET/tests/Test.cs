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
		public void TestConnectWhileAlreadyConnected()
		{
			ConnectionParameters parameters = new ConnectionParameters ();

			Server server = new Server (parameters);

			server.SetLocalPort (20213);

			server.Start ();

			Connection connection = new Connection ("127.0.0.1", parameters);

			SocketException se = null;

			try {
				connection.Connect ();
			}
			catch (SocketException ex) {
				se = ex;
			}

			Assert.IsNull (se);

			Thread.Sleep (100);

			try {
				connection.Connect ();
			}
			catch (SocketException ex) {
				se = ex;
			}

			Assert.IsNotNull (se);
			Assert.AreEqual (10056, se.ErrorCode);

			connection.Close ();

			server.Stop ();
		}


		[Test()]
		public void TestConnectSameConnectionMultipleTimes()
		{
			ConnectionParameters parameters = new ConnectionParameters ();

			Server server = new Server (parameters);

			server.SetLocalPort (20213);

			server.Start ();

			Connection connection = new Connection ("127.0.0.1", parameters);

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
			
	}
}

