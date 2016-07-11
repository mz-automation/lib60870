using System;

namespace lib60870
{

	public class SinglePointWithCP24Time2a : SinglePointInformation
	{
		private CP24Time2a timestamp;

		public CP24Time2a Timestamp {
			get {
				return this.timestamp;
			}
		}
		
		public SinglePointWithCP24Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA + 1; /* skip IOA  + SIQ */

			/* parse CP24Time2a (time stamp) */
			timestamp = new CP24Time2a (msg, startIndex);
		}

		public SinglePointWithCP24Time2a(int objectAddress, bool value, QualityDescriptor quality, CP24Time2a timestamp):
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
