using System;

namespace lib60870
{
	public class SinglePointWithCP56Time2a : InformationObject
	{
		private bool value;

		public bool Value {
			get {
				return this.value;
			}
		}

		private QualityDescriptor quality;

		public QualityDescriptor Quality {
			get {
				return this.quality;
			}
		}

		private CP56Time2a timestamp;

		public CP56Time2a Timestamp {
			get {
				return this.timestamp;
			}
		}

		public SinglePointWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse SIQ (single point information with qualitiy) */
			byte siq = msg [startIndex++];

			value = ((siq & 0x01) == 0x01);
			
			quality = new QualityDescriptor ((byte) (siq & 0xf0));

			/* parse CP56Time2a (time stamp) */
			timestamp = new CP56Time2a (msg, startIndex);
		}
	}
}

