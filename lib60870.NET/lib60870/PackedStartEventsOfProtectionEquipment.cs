using System;
using System.Collections.Generic;

namespace lib60870
{

	public class PackedStartEventsOfProtectionEquipment : InformationObject
	{
		private StartEvent spe;

		public StartEvent SPE {
			get {
				return spe;
			}
		}

		private QualityDescriptorP qdp;

		public QualityDescriptorP QDP {
			get {
				return qdp;
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

		public PackedStartEventsOfProtectionEquipment (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			spe = new StartEvent (msg [startIndex++]);
			qdp = new QualityDescriptorP (msg [startIndex++]);

			elapsedTime = new CP16Time2a (msg, startIndex);
			startIndex += 2;

			/* parse CP56Time2a (time stamp) */
			timestamp = new CP24Time2a (msg, startIndex);
		}

	}

	public class PackedStartEventsOfProtectionEquipmentWithCP56Time2a : InformationObject
	{
		private StartEvent spe;

		public StartEvent SPE {
			get {
				return spe;
			}
		}

		private QualityDescriptorP qdp;

		public QualityDescriptorP QDP {
			get {
				return qdp;
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

		public PackedStartEventsOfProtectionEquipmentWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			spe = new StartEvent (msg [startIndex++]);
			qdp = new QualityDescriptorP (msg [startIndex++]);

			elapsedTime = new CP16Time2a (msg, startIndex);
			startIndex += 2;

			/* parse CP56Time2a (time stamp) */
			timestamp = new CP56Time2a (msg, startIndex);
		}

	}
}

