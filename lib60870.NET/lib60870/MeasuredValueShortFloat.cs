using System;

namespace lib60870
{
	public class MeasuredValueShortFloat : InformationObject
	{
		private float value;

		public float Value {
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

		public MeasuredValueShortFloat (int objectAddress, float value, QualityDescriptor quality)
			: base(objectAddress)
		{
			this.value = value;
			this.quality = quality;
		}


		public MeasuredValueShortFloat (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse float value */
			value = System.BitConverter.ToSingle (msg, startIndex);
			startIndex += 4;

			/* parse QDS (quality) */
			quality = new QualityDescriptor (msg [startIndex++]);
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			byte[] floatEncoded = BitConverter.GetBytes (value);

			if (BitConverter.IsLittleEndian == false)
				Array.Reverse (floatEncoded);

			frame.AppendBytes (floatEncoded);

			frame.SetNextByte (quality.EncodedValue);
		}
	}

	public class MeasuredValueShortFloatWithCP24Time2a : MeasuredValueShortFloat
	{


		private CP24Time2a timestamp;

		public CP24Time2a Timestamp {
			get {
				return this.timestamp;
			}
		}

		public MeasuredValueShortFloatWithCP24Time2a (int objectAddress, float value, QualityDescriptor quality, CP24Time2a timestamp)
			: base(objectAddress, value, quality)
		{
			this.timestamp = timestamp;
		}

		public MeasuredValueShortFloatWithCP24Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA + 5; /* skip IOA */

			/* parse CP56Time2a (time stamp) */
			timestamp = new CP24Time2a (msg, startIndex);
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.AppendBytes (timestamp.GetEncodedValue ());
		}

	}

	public class MeasuredValueShortFloatWithCP56Time2a : MeasuredValueShortFloat
	{

		private CP56Time2a timestamp;

		public CP56Time2a Timestamp {
			get {
				return this.timestamp;
			}
		}

		public MeasuredValueShortFloatWithCP56Time2a (int objectAddress, float value, QualityDescriptor quality, CP56Time2a timestamp)
			: base(objectAddress, value, quality)
		{
			this.timestamp = timestamp;
		}

		public MeasuredValueShortFloatWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA + 5; /* skip IOA */

			/* parse CP56Time2a (time stamp) */
			timestamp = new CP56Time2a (msg, startIndex);
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.AppendBytes (timestamp.GetEncodedValue ());
		}
	}
}

