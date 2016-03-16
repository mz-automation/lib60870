using System;

namespace lib60870
{
	public class SetpointCommandQualifier
	{
		private byte encodedValue;

		public SetpointCommandQualifier (byte encodedValue)
		{
			this.encodedValue = encodedValue;
		}

		public SetpointCommandQualifier (bool select, int ql) 
		{
			encodedValue = (byte)(ql & 0x7f);

			if (select)
				encodedValue |= 0x80;
		}

		public int QL {
			get {
				return (encodedValue & 0x7f);
			}
		}

		public bool Select {
			get {
				return ((encodedValue & 0x80) == 0x80);
			}
		}

		public byte GetEncodedValue () {
			return encodedValue;
		}
	}
}

