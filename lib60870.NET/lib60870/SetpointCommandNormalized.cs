using System;

namespace lib60870
{

	public class SetpointCommandNormalized : InformationObject
	{
		private int scaledValue;

		public float NormalizedValue {
			get {
				float nv = (float) scaledValue / 32767f;

				return nv;
			}
		}

		private SetpointCommandQualifier qos;

		public SetpointCommandQualifier QOS {
			get {
				return qos;
			}
		}

		public SetpointCommandNormalized (int objectAddress, float value, SetpointCommandQualifier qos)
			: base(objectAddress)
		{
			// TODO check if value is in range

			this.scaledValue = (int) (value * 32767f);
			this.qos = qos;
		}

		public SetpointCommandNormalized (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			scaledValue = msg [startIndex++];
			scaledValue += (msg [startIndex++] * 0x100);

			if (scaledValue > 32767)
				scaledValue = scaledValue - 65536;

			this.qos = new SetpointCommandQualifier (msg [startIndex++]);
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

			frame.SetNextByte (this.qos.GetEncodedValue ());
		}
	}

}
