using System;

namespace lib60870
{

	public class ParameterNormalizedValue : InformationObject
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

		private byte qpm;

		public float QPM {
			get {
				return qpm;
			}
		}

		public ParameterNormalizedValue (int objectAddress, float normalizedValue, byte qpm) :
			base (objectAddress)
		{
			scaledValue = new ScaledValue ((int)(normalizedValue * 32767f));

			this.NormalizedValue = normalizedValue;

			this.qpm = qpm;
		}

		public ParameterNormalizedValue (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			scaledValue = new ScaledValue (msg, startIndex);
			startIndex += 2;

			/* parse QDS (quality) */
			qpm = msg [startIndex++];
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.AppendBytes (scaledValue.GetEncodedValue ());

			frame.SetNextByte (qpm);
		}
	}

	public class ParameterScaledValue : InformationObject
	{
		private ScaledValue scaledValue;

		public ScaledValue ScaledValue {
			get {
				return scaledValue;
			}
			set {
				scaledValue = value;
			}
		}

		private byte qpm;

		public float QPM {
			get {
				return qpm;
			}
		}

		public ParameterScaledValue (int objectAddress, ScaledValue value, byte qpm) :
			base (objectAddress)
		{
			scaledValue = value;

			this.qpm = qpm;
		}

		public ParameterScaledValue (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			scaledValue = new ScaledValue (msg, startIndex);
			startIndex += 2;

			/* parse QDS (quality) */
			qpm = msg [startIndex++];
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.AppendBytes (scaledValue.GetEncodedValue ());

			frame.SetNextByte (qpm);
		}
	}
}
