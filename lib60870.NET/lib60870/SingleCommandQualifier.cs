using System;

namespace lib60870
{
	public class SingleCommandQualifier
	{
		private byte encodedValue;

		public SingleCommandQualifier (byte encodedValue)
		{
			this.encodedValue = encodedValue;
		}

		public SingleCommandQualifier(bool state, bool selectCommand, int qu) {
			encodedValue = (byte) ((qu & 0x1f) * 4);

			if (state) encodedValue |= 0x01;

			if (selectCommand)
				encodedValue |= 0x80;
		}

		public int QU {
			get {
				return ((encodedValue & 0x7c) / 4);
			}
		}

		public bool State {
			get {
				return ((encodedValue & 0x01) == 0x01);
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

