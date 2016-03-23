using System;

namespace lib60870
{

	public class MeasuredValueNormalizedWithoutQuality : InformationObject
	{
		private ScaledValue scaledValue;

		public float NormalizedValue {
			get {
				float nv = (float) (scaledValue.Value) / 32767f;

				return nv;
			}
			set {
				//TODO check value range
				scaledValue.Value = (int)(value * 32767f); 
			}
		}

		public MeasuredValueNormalizedWithoutQuality (int objectAddress, float value)
			: base(objectAddress)
		{
			this.scaledValue = new ScaledValue ((int)(value * 32767f));

			this.NormalizedValue = value;
		}

		public MeasuredValueNormalizedWithoutQuality (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			scaledValue = new ScaledValue (msg, startIndex);
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.AppendBytes (scaledValue.GetEncodedValue ());
		}
	}


	public class MeasuredValueNormalized : MeasuredValueNormalizedWithoutQuality
	{
		private QualityDescriptor quality;

		public QualityDescriptor Quality {
			get {
				return this.quality;
			}
		}

		public MeasuredValueNormalized (int objectAddress, float value, QualityDescriptor quality)
			: base(objectAddress, value)
		{
			this.quality = quality;
		}

		public MeasuredValueNormalized (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA + 2; /* skip IOA + scaled value */

			/* parse QDS (quality) */
			quality = new QualityDescriptor (msg [startIndex++]);
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.SetNextByte (quality.EncodedValue);
		}
	}
	

	public class MeasuredValueNormalizedWithCP24Time2a : MeasuredValueNormalized
	{
		private CP24Time2a timestamp;

		public CP24Time2a Timestamp {
			get {
				return this.timestamp;
			}
		}


		public MeasuredValueNormalizedWithCP24Time2a (int objectAddress, float value, QualityDescriptor quality, CP24Time2a timestamp)
			: base(objectAddress, value, quality)
		{
			this.timestamp = timestamp;
		}

		public MeasuredValueNormalizedWithCP24Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA + 3; /* skip IOA + scaledValue + quality */

			/* parse CP24Time2a (time stamp) */
			timestamp = new CP24Time2a (msg, startIndex);
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.AppendBytes (timestamp.GetEncodedValue ());
		}
	}

	public class MeasuredValueNormalizedWithCP56Time2a : MeasuredValueNormalized
	{

		private CP56Time2a timestamp;

		public CP56Time2a Timestamp {
			get {
				return this.timestamp;
			}
		}

		public MeasuredValueNormalizedWithCP56Time2a (int objectAddress, float value, QualityDescriptor quality, CP56Time2a timestamp)
			: base(objectAddress, value, quality)
		{
			this.timestamp = timestamp;
		}

		public MeasuredValueNormalizedWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA + 3; /* skip IOA + scaledValue + quality */

			/* parse CP56Time2a (time stamp) */
			timestamp = new CP56Time2a (msg, startIndex);
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.AppendBytes (timestamp.GetEncodedValue ());
		}
	}



}

