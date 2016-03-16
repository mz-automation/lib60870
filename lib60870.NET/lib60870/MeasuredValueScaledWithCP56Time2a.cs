using System;

namespace lib60870
{
	public class MeasuredValueScaledWithCP56Time2a : InformationObject
	{

		private int scaledValue;

		public int ScaledValue {
			get {
				return this.scaledValue;
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

		public MeasuredValueScaledWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			scaledValue = msg [startIndex++];
			scaledValue += (msg [startIndex++] * 0x100);

			if (scaledValue > 32767)
				scaledValue = scaledValue - 65536;

			/* parse QDS (quality) */
			quality = new QualityDescriptor (msg [startIndex++]);

			/* parse CP56Time2a (time stamp) */
			timestamp = new CP56Time2a (msg, startIndex);
		}

	}
}

