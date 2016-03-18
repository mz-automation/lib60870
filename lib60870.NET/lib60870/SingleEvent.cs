using System;

namespace lib60870
{

	public enum EventState {
		INDETERMINATE_0 = 0,
		OFF = 1,
		ON = 2,
		INDETERMINATE_3 = 3
	}

	public class SingleEvent
	{
		private byte encodedValue;

		public SingleEvent()
		{
			this.encodedValue = 0;
		}

		public SingleEvent (byte encodedValue)
		{
			this.encodedValue = encodedValue;
		}

		public EventState State {
			get {
				return (EventState)(encodedValue & 0x03);
			}
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

