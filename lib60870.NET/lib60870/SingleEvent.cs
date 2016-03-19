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
		private QualityDescriptorP qdp;

		private EventState eventState;

		public SingleEvent()
		{
			this.eventState = EventState.INDETERMINATE_0;
			this.qdp = new QualityDescriptorP ();
		}

		public SingleEvent (byte encodedValue)
		{
			this.eventState = (EventState)(encodedValue & 0x03);

			this.qdp = new QualityDescriptorP (encodedValue);
		}

		public EventState State {
			get {
				return eventState;
			}
		}

		public QualityDescriptorP QDP {
			get {
				return qdp;
			}
		}


		public byte EncodedValue {
			get {
				byte encodedValue = (byte)((qdp.EncodedValue & 0xfc) + (int) eventState);

				return encodedValue;
			}
		}

	}
}

