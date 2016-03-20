using System;

namespace lib60870
{
	public class EventOfProtectionEquipment : InformationObject
	{

		private SingleEvent singleEvent;

		public SingleEvent Event {
			get {
				return singleEvent;
			}
		}

		private CP16Time2a elapsedTime;

		public CP16Time2a ElapsedTime {
			get {
				return this.elapsedTime;
			}
		}

		private CP24Time2a timestamp;

		public CP24Time2a Timestamp {
			get {
				return this.timestamp;
			}
		}

		public EventOfProtectionEquipment (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			singleEvent = new SingleEvent (msg [startIndex++]);

			elapsedTime = new CP16Time2a (msg, startIndex);
			startIndex += 2;

			/* parse CP56Time2a (time stamp) */
			timestamp = new CP24Time2a (msg, startIndex);
		}
	}

	public class EventOfProtectionEquipmentWithCP56Time2a : InformationObject
	{

		private SingleEvent singleEvent;

		public SingleEvent Event {
			get {
				return singleEvent;
			}
		}

		private CP16Time2a elapsedTime;

		public CP16Time2a ElapsedTime {
			get {
				return this.elapsedTime;
			}
		}

		private CP56Time2a timestamp;

		public CP56Time2a Timestamp {
			get {
				return this.timestamp;
			}
		}

		public EventOfProtectionEquipmentWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			singleEvent = new SingleEvent (msg [startIndex++]);

			elapsedTime = new CP16Time2a (msg, startIndex);
			startIndex += 2;

			/* parse CP56Time2a (time stamp) */
			timestamp = new CP56Time2a (msg, startIndex);
		}
	}
}

