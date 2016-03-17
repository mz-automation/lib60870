using System;

namespace lib60870
{
	public class StepPositionInformation : InformationObject
	{
		private int value;

		/// <summary>
		/// Step position (range -64 ... +63)
		/// </summary>
		/// <value>The value.</value>
		public int Value {
			get {
				return this.value;
			}
		}

		private bool isTransient;

		/// <summary>
		/// Gets a value indicating whether this <see cref="lib60870.StepPositionInformation"/> is in transient state.
		/// </summary>
		/// <value><c>true</c> if transient; otherwise, <c>false</c>.</value>
		public bool Transient {
			get {
				return this.isTransient;
			}
		}

		private QualityDescriptor quality;

		public QualityDescriptor Quality {
			get {
				return this.quality;
			}
		}

		public StepPositionInformation (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse VTI (value with transient state indication) */
			byte vti = msg [startIndex++];

			isTransient = ((vti & 0x80) == 0x80);

			value = (vti & 0x7f);

			if (value > 63)
				value = value - 128;

			quality = new QualityDescriptor (msg[startIndex++]);
		}


	}

	public class StepPositionWithCP24Time2a : InformationObject
	{
		private int value;

		public int Value {
			get {
				return this.value;
			}
		}

		private bool isTransient;

		public bool Transient {
			get {
				return this.isTransient;
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


		public StepPositionWithCP24Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse VTI (value with transient state indication) */
			byte vti = msg [startIndex++];

			isTransient = ((vti & 0x80) == 0x80);

			value = (vti & 0x7f);

			if (value > 63)
				value = value - 128;

			quality = new QualityDescriptor (msg[startIndex++]);

			/* parse CP24Time2a (time stamp) */
			timestamp = new CP24Time2a (msg, startIndex);
		}

	}

	public class StepPositionWithCP56Time2a : InformationObject
	{
		private int value;

		public int Value {
			get {
				return this.value;
			}
		}

		private bool isTransient;

		public bool Transient {
			get {
				return this.isTransient;
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


		public StepPositionWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse VTI (value with transient state indication) */
			byte vti = msg [startIndex++];

			isTransient = ((vti & 0x80) == 0x80);

			value = (vti & 0x7f);

			if (value > 63)
				value = value - 128;

			quality = new QualityDescriptor (msg[startIndex++]);

			/* parse CP24Time2a (time stamp) */
			timestamp = new CP56Time2a (msg, startIndex);
		}

	}
	
}

