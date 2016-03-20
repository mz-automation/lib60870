using System;

namespace lib60870
{
	
	public class MeasuredValueNormalized : InformationObject
	{
		private int scaledValue;

		public float NormalizedValue {
			get {
				float nv = (float) scaledValue / 32767f;

				return nv;
			}
		}

		private QualityDescriptor quality;

		public QualityDescriptor Quality {
			get {
				return this.quality;
			}
		}

		public MeasuredValueNormalized (int objectAddress, float value, QualityDescriptor quality)
			: base(objectAddress)
		{
			// TODO check if value is in range

			this.scaledValue = (int) (value * 32767f);
			this.quality = quality;
		}

		public MeasuredValueNormalized (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			scaledValue = msg [startIndex++];
			scaledValue += (msg [startIndex++] * 0x100);

			if (scaledValue > 32767)
				scaledValue = scaledValue - 65536;

			/* parse QDS (quality) */
			quality = new QualityDescriptor (msg [startIndex++]);
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			int valueToEncode;

			if (scaledValue < 0)
				valueToEncode = scaledValue + 65536;
			else
				valueToEncode = scaledValue;

			frame.SetNextByte ((byte)(valueToEncode % 256));
			frame.SetNextByte ((byte)(valueToEncode / 256));

			frame.SetNextByte (quality.EncodedValue);
		}
	}

	public class MeasuredValueNormalizedWithoutQuality : InformationObject
	{
		private int scaledValue;

		public float NormalizedValue {
			get {
				float nv = (float) scaledValue / 32767f;

				return nv;
			}
		}
	

		public MeasuredValueNormalizedWithoutQuality (int objectAddress, float value)
			: base(objectAddress)
		{
			// TODO check if value is in range

			this.scaledValue = (int) (value * 32767f);
		}

		public MeasuredValueNormalizedWithoutQuality (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			scaledValue = msg [startIndex++];
			scaledValue += (msg [startIndex++] * 0x100);

			if (scaledValue > 32767)
				scaledValue = scaledValue - 65536;
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			int valueToEncode;

			if (scaledValue < 0)
				valueToEncode = scaledValue + 65536;
			else
				valueToEncode = scaledValue;

			frame.SetNextByte ((byte)(valueToEncode % 256));
			frame.SetNextByte ((byte)(valueToEncode / 256));
		}
	}

	public class MeasuredValueNormalizedWithCP24Time2a : InformationObject
	{
		private int scaledValue;

		public float NormalizedValue {
			get {
				float nv = (float) scaledValue / 32767f;

				return nv;
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


		public MeasuredValueNormalizedWithCP24Time2a (int objectAddress, float value, QualityDescriptor quality)
			: base(objectAddress)
		{
			// TODO check if value is in range

			this.scaledValue = (int) (value * 32767f);
			this.quality = quality;
		}

		public MeasuredValueNormalizedWithCP24Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			scaledValue = msg [startIndex++];
			scaledValue += (msg [startIndex++] * 0x100);

			if (scaledValue > 32767)
				scaledValue = scaledValue - 65536;

			/* parse QDS (quality) */
			quality = new QualityDescriptor (msg [startIndex++]);

			/* parse CP24Time2a (time stamp) */
			timestamp = new CP24Time2a (msg, startIndex);
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			int valueToEncode;

			if (scaledValue < 0)
				valueToEncode = scaledValue + 65536;
			else
				valueToEncode = scaledValue;

			frame.SetNextByte ((byte)(valueToEncode % 256));
			frame.SetNextByte ((byte)(valueToEncode / 256));

			frame.SetNextByte (quality.EncodedValue);

			frame.AppendBytes (timestamp.GetEncodedValue ());
		}
	}

	public class MeasuredValueNormalizedWithCP56Time2a : InformationObject
	{
		private int scaledValue;

		public float NormalizedValue {
			get {
				float nv = (float) scaledValue / 32767f;

				return nv;
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

		public MeasuredValueNormalizedWithCP56Time2a (int objectAddress, float value, QualityDescriptor quality)
			: base(objectAddress)
		{
			// TODO check if value is in range

			this.scaledValue = (int) (value * 32767f);
			this.quality = quality;
		}

		public MeasuredValueNormalizedWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
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

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			int valueToEncode;

			if (scaledValue < 0)
				valueToEncode = scaledValue + 65536;
			else
				valueToEncode = scaledValue;

			frame.SetNextByte ((byte)(valueToEncode % 256));
			frame.SetNextByte ((byte)(valueToEncode / 256));

			frame.SetNextByte (quality.EncodedValue);

			frame.AppendBytes (timestamp.GetEncodedValue ());
		}
	}



}

