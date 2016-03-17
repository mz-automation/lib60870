using System;

namespace lib60870
{
	//TODO refactor: use SinglePointInformation as base class

	public class SinglePointWithCP24Time2a : InformationObject
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

		private CP24Time2a timestamp;

		public CP24Time2a Timestamp {
			get {
				return this.timestamp;
			}
		}
		
		public SinglePointWithCP24Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse SIQ (single point information with qualitiy) */
			byte siq = msg [startIndex++];

			value = ((siq & 0x01) == 0x01);

			quality = new QualityDescriptor ((byte) (siq & 0xf0));

			/* parse CP24Time2a (time stamp) */
			timestamp = new CP24Time2a (msg, startIndex);
		}
	}

}
