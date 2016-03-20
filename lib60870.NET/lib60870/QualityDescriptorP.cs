using System;

namespace lib60870
{

	/// <summary>
	/// QDP - Quality descriptor for events of protection equipment
	/// according to IEC 60870-5-101:2003 7.2.6.4
	/// </summary>
	public class QualityDescriptorP 
	{
		private byte encodedValue;

		public QualityDescriptorP () 
		{
			this.encodedValue = 0;
		}

		public QualityDescriptorP (byte encodedValue)
		{
			this.encodedValue = encodedValue;
		}

		public bool Reserved {
			get {
				return ((encodedValue & 0x04) == 0x04);
			}

			set {
				if (value) 
					encodedValue |= 0x04;
				else
					encodedValue &= 0xfb;
			}
		}

		public bool ElapsedTimeInvalid {
			get {
				return ((encodedValue & 0x08) == 0x08);
			}

			set {
				if (value) 
					encodedValue |= 0x08;
				else
					encodedValue &= 0xf7;
			}
		}

		public bool Blocked {
			get {
				if ((encodedValue & 0x10) != 0)
					return true;
				else
					return false;
			}

			set {
				if (value) 
					encodedValue |= 0x10;
				else
					encodedValue &= 0xef;
			}
		}

		public bool Substituted {
			get {
				if ((encodedValue & 0x20) != 0)
					return true;
				else
					return false;
			}

			set {
				if (value) 
					encodedValue |= 0x20;
				else
					encodedValue &= 0xdf;
			}
		}

		public bool NonTopical {
			get {
				if ((encodedValue & 0x40) != 0)
					return true;
				else
					return false;
			}

			set {
				if (value) 
					encodedValue |= 0x40;
				else
					encodedValue &= 0xbf;
			}
		}

		public bool Invalid {
			get {
				if ((encodedValue & 0x80) != 0)
					return true;
				else
					return false;
			}

			set {
				if (value) 
					encodedValue |= 0x80;
				else
					encodedValue &= 0x7f;
			}
		}

		public byte EncodedValue {
			get {
				return this.encodedValue;
			}
			set {
				encodedValue = value;
			}
		}
	}


}
