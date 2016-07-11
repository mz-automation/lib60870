using System;

namespace lib60870
{
	public class SinglePointInformation : InformationObject
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

		public SinglePointInformation (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse SIQ (single point information with qualitiy) */
			byte siq = msg [startIndex++];

			value = ((siq & 0x01) == 0x01);

			quality = new QualityDescriptor ((byte) (siq & 0xf0));
		}


		public SinglePointInformation(int objectAddress, bool value, QualityDescriptor quality):
			base(objectAddress)
		{
			this.value = value;
			this.quality = quality;
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			byte val = quality.EncodedValue;

			if (value)
				val++;

			frame.SetNextByte (val);
		}

	}

	public class PackedSinglePointWithSCD : InformationObject
	{
		private StatusAndStatusChangeDetection scd;

		private QualityDescriptor qds;

		public StatusAndStatusChangeDetection SCD {
			get {
				return this.scd;
			}
			set {
				scd = value;
			}
		}

		public QualityDescriptor QDS {
			get {
				return this.qds;
			}
			set {
				qds = value;
			}
		}

		public PackedSinglePointWithSCD (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			scd = new StatusAndStatusChangeDetection (msg, startIndex);
			startIndex += 4;

			qds = new QualityDescriptor (msg [startIndex++]);
		}

		//TODO add server side functionality
	}

}

