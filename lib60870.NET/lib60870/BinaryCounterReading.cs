using System;

namespace lib60870
{
	public class BinaryCounterReading
	{

		private byte[] encodedValue = new byte[5];

		public byte[] GetEncodedValue() 
		{
			return encodedValue;
		}

		public Int32 Value {
			get {
				Int32 value = encodedValue [0];
				value += (encodedValue [1] * 0x100);
				value += (encodedValue [2] * 0x10000);
				value += (encodedValue [3] * 0x1000000);

				return value;
			}
		}

		public int SequenceNumber {
			get {
				return (encodedValue [4] & 0x1f);
			}
		}

		public bool Carry {
			get {
				return ((encodedValue[4] & 0x20) == 0x20);
			}
		}

		public bool Adjusted {
			get {
				return ((encodedValue[4] & 0x40) == 0x40);
			}
		}

		public bool Invalid {
			get {
				return ((encodedValue[4] & 0x80) == 0x80);
			}
		}

		public BinaryCounterReading (byte[] msg, int startIndex)
		{
			if (msg.Length < startIndex + 5)
				throw new ASDUParsingException ("Message too small for parsing BinaryCounterReading");

			for (int i = 0; i < 5; i++)
				encodedValue [i] = msg [startIndex + i];
		}

		public BinaryCounterReading ()
		{
		}
	}
}

