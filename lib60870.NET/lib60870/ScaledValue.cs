using System;

namespace lib60870
{
	public class ScaledValue {
		private byte[] encodedValue = new byte[2];

		public ScaledValue (byte[] msg, int startIndex)
		{
			if (msg.Length < startIndex + 2)
				throw new ASDUParsingException ("Message too small for parsing ScaledValue");

			for (int i = 0; i < 2; i++)
				encodedValue [i] = msg [startIndex + i];
		}

		public ScaledValue(int value)
		{
			this.Value = value;
		}

		public byte[] GetEncodedValue() 
		{
			return encodedValue;
		}

		public int Value {
			get {
				int value;

				value = encodedValue [0];
				value += (encodedValue [1] * 0x100);

				if (value > 32767)
					value = value - 65536;

				return value;
			}
			set {
				int valueToEncode;

				if (value < 0)
					valueToEncode = value + 65536;
				else
					valueToEncode = value;

				encodedValue[0] = (byte)(valueToEncode % 256);
				encodedValue[1] = (byte)(valueToEncode / 256);
			}
		}

	}
	
}
