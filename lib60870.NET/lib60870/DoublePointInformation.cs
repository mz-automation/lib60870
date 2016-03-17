using System;

namespace lib60870
{
	public enum DoublePointValue {
		INTERMEDIATE = 0,
		OFF = 1,
		ON = 2,
		INDETERMINATE = 3
	}

	public class DoublePointInformation : InformationObject
	{

		private DoublePointValue value;

		public DoublePointValue Value {
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

		public DoublePointInformation (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse DIQ (double point information with qualitiy) */
			byte siq = msg [startIndex++];

			value = (DoublePointValue)(siq & 0x03);

			quality = new QualityDescriptor ((byte) (siq & 0xf0));
		}
	}

	//TODO refactor: use DoublePointInformation as common base class

	public class DoublePointWithCP24Time2a : InformationObject
	{
		private DoublePointValue value;

		public DoublePointValue Value {
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

		public DoublePointWithCP24Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse DIQ (double point information with qualitiy) */
			byte siq = msg [startIndex++];

			value = (DoublePointValue)(siq & 0x03);

			quality = new QualityDescriptor ((byte) (siq & 0xf0));

			/* parse CP24Time2a (time stamp) */
			timestamp = new CP24Time2a (msg, startIndex);
		}
	}

	public class DoublePointWithCP56Time2a : InformationObject
	{
		private DoublePointValue value;

		public DoublePointValue Value {
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

		public DoublePointWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse DIQ (double point information with qualitiy) */
			byte siq = msg [startIndex++];

			value = (DoublePointValue)(siq & 0x03);

			quality = new QualityDescriptor ((byte) (siq & 0xf0));

			/* parse CP24Time2a (time stamp) */
			timestamp = new CP56Time2a (msg, startIndex);
		}
	}

}

