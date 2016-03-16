using System;

namespace lib60870
{
	public class QualityDescriptor
	{
		private byte encodedValue;
	
		public QualityDescriptor()
		{
			this.encodedValue = 0;
		}

		public QualityDescriptor (byte encodedValue)
		{
			this.encodedValue = encodedValue;
		}

		public bool Overflow {
			get {
				if ((encodedValue & 0x01) != 0)
					return true;
				else
					return false;
			}

			set {
				if (value) 
					encodedValue |= 0x01;
				else
					encodedValue &= 0xfe;
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

		public override string ToString ()
		{
			return string.Format ("[QualityDescriptor: Overflow={0}, Blocked={1}, Substituted={2}, NonTopical={3}, Invalid={4}]", Overflow, Blocked, Substituted, NonTopical, Invalid);
		}
	}
		
}

