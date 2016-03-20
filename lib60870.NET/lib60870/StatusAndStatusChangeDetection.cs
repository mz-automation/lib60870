using System;

namespace lib60870
{

	public class StatusAndStatusChangeDetection {

		public UInt16 STn {
			get {
				return (ushort) (encodedValue[0] + 256 * encodedValue[1]);
			}
		}

		public UInt16 CDn {
			get {
				return (ushort) (encodedValue[2] + 256 * encodedValue[3]);
			}
		}

		public bool ST(int i) {
			if ((i >= 0) && (i < 16))
				return ((int) (STn & (2^i)) != 0);
			else
				return false;
		}

		public bool CD(int i) {
			if ((i >= 0) && (i < 16))
				return ((int) (CDn & (2^i)) != 0);
			else
				return false;
		}

		public StatusAndStatusChangeDetection (byte[] msg, int startIndex)
		{
			if (msg.Length < startIndex + 4)
				throw new ASDUParsingException ("Message too small for parsing StatusAndStatusChangeDetection");

			for (int i = 0; i < 4; i++)
				encodedValue [i] = msg [startIndex + i];
		}

		private byte[] encodedValue = new byte[0];

		public byte[] GetEncodedValue() 
		{
			return encodedValue;
		}
	}
}
