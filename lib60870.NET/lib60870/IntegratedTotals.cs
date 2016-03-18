using System;

namespace lib60870
{
	public class IntegratedTotals : InformationObject
	{

		private BinaryCounterReading bcr;

		public BinaryCounterReading BCR {
			get {
				return bcr;
			}
		}

		public IntegratedTotals (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			bcr = new BinaryCounterReading(msg, startIndex);

			startIndex += 5;
		}
	}

	public class IntegratedTotalsWithCP24Time2a : IntegratedTotals
	{
		private CP24Time2a timestamp;

		public CP24Time2a Timestamp {
			get {
				return this.timestamp;
			}
		}


		public IntegratedTotalsWithCP24Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA + 5; /* skip IOA + BCR */

			timestamp = new CP24Time2a (msg, startIndex);
		}
	}

	public class IntegratedTotalsWithCP56Time2a : IntegratedTotals
	{
		private CP56Time2a timestamp;

		public CP56Time2a Timestamp {
			get {
				return this.timestamp;
			}
		}


		public IntegratedTotalsWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA + 5; /* skip IOA + BCR */

			timestamp = new CP56Time2a (msg, startIndex);
		}
	}
}

