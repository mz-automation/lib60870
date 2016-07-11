using System;

namespace lib60870
{

	public class SinglePointWithCP56Time2a : SinglePointInformation
	{
		private CP56Time2a timestamp;

		public CP56Time2a Timestamp {
			get {
				return this.timestamp;
			}
		}

		public SinglePointWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA + 1; /* skip IOA + SIQ */

			/* parse CP56Time2a (time stamp) */
			timestamp = new CP56Time2a (msg, startIndex);
		}

		public SinglePointWithCP56Time2a(int objectAddress, bool value, QualityDescriptor quality, CP56Time2a timestamp):
			base(objectAddress, value, quality)
		{
			this.timestamp = timestamp;
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.AppendBytes (timestamp.GetEncodedValue ());
		}
	}


}

