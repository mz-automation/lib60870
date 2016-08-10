using NUnit.Framework;
using System;
using lib60870;

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

			Frame frame = new T104Frame ();

			sc.Encode (frame, new ConnectionParameters ());

			Assert.AreEqual (12, frame.GetMsgSize());

			SetpointCommandNormalized sc2 = new SetpointCommandNormalized (new ConnectionParameters (),
				                                frame.GetBuffer (), 6);

			Assert.AreEqual (-0.5f, sc2.NormalizedValue, 0.001f);
			Assert.AreEqual (102, sc2.ObjectAddress);
			Assert.AreEqual (true, sc2.QOS.Select);
		}
	}
}

