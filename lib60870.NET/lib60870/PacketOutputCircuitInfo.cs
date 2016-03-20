using System;

namespace lib60870
{

	public class PacketOutputCircuitInfo : InformationObject
	{
		OutputCircuitInfo oci;

		public OutputCircuitInfo OCI {
			get {
				return this.oci;
			}
		}

		QualityDescriptorP qdp;

		public QualityDescriptorP QDP {
			get {
				return this.qdp;
			}
		}

		private CP16Time2a operatingTime;

		public CP16Time2a OperatingTime {
			get {
				return this.operatingTime;
			}
		}

		private CP24Time2a timestamp;

		public CP24Time2a Timestamp {
			get {
				return this.timestamp;
			}
		}

		public PacketOutputCircuitInfo (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			oci = new OutputCircuitInfo (msg [startIndex++]);

			qdp = new QualityDescriptorP (msg [startIndex++]);

			operatingTime = new CP16Time2a (msg, startIndex);
			startIndex += 2;

			/* parse CP56Time2a (time stamp) */
			timestamp = new CP24Time2a (msg, startIndex);
		}
	}
}
