using System;

namespace lib60870
{
	
	public class Bitstring32 : InformationObject
	{

		private UInt32 value;

		public UInt32 Value {
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

		public Bitstring32 (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			value = msg [startIndex++];
			value += ((uint)msg [startIndex++] * 0x100);
			value += ((uint)msg [startIndex++] * 0x10000);
			value += ((uint)msg [startIndex++] * 0x1000000);

			quality = new QualityDescriptor (msg[startIndex++]);

		}

	}

	public class Bitstring32WithCP24Time2a : InformationObject
	{

		private UInt32 value;

		public UInt32 Value {
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

		private CP24Time2a timestamp;

		public CP24Time2a Timestamp {
			get {
				return this.timestamp;
			}
		}

		public Bitstring32WithCP24Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			value = msg [startIndex++];
			value += ((uint)msg [startIndex++] * 0x100);
			value += ((uint)msg [startIndex++] * 0x10000);
			value += ((uint)msg [startIndex++] * 0x1000000);

			quality = new QualityDescriptor (msg[startIndex++]);

			/* parse CP24Time2a (time stamp) */
			timestamp = new CP24Time2a (msg, startIndex);
		}

	}

	public class Bitstring32WithCP56Time2a : InformationObject
	{

		private UInt32 value;

		public UInt32 Value {
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

		private CP56Time2a timestamp;

		public CP56Time2a Timestamp {
			get {
				return this.timestamp;
			}
		}

		public Bitstring32WithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			value = msg [startIndex++];
			value += ((uint)msg [startIndex++] * 0x100);
			value += ((uint)msg [startIndex++] * 0x10000);
			value += ((uint)msg [startIndex++] * 0x1000000);

			quality = new QualityDescriptor (msg[startIndex++]);

			/* parse CP56Time2a (time stamp) */
			timestamp = new CP56Time2a (msg, startIndex);
		}

	}

}

