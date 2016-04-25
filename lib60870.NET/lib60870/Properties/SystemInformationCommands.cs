using System;

namespace lib60870
{

	public class InterrogationCommand : InformationObject
	{
		byte qoi;

		public byte QOI {
			get {
				return this.qoi;
			}
			set {
				qoi = value;
			}
		}

		public InterrogationCommand (int ioa, byte qoi) : base(ioa)
		{
			this.qoi = qoi;
		}

		public InterrogationCommand (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			qoi = msg [startIndex++];
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.SetNextByte (qoi);
		}

	}
		
	public class CounterInterrogationCommand : InformationObject
	{
		byte qcc;

		/// <summary>
		/// Gets or sets the QCC (Qualifier of counter interrogation).
		/// </summary>
		/// <value>The QCC</value>
		public byte QCC {
			get {
				return this.qcc;
			}
			set {
				qcc = value;
			}
		}

		public CounterInterrogationCommand (int ioa, byte qoi) : base(ioa)
		{
			this.qcc = qoi;
		}

		public CounterInterrogationCommand (ConnectionParameters parameters, byte[] msg, int startIndex) :
		base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			qcc = msg [startIndex++];
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.SetNextByte (qcc);
		}

	}


	public class ReadCommand : InformationObject
	{

		public ReadCommand (int ioa) : base(ioa)
		{
		}

		public ReadCommand (ConnectionParameters parameters, byte[] msg, int startIndex) :
		base(parameters, msg, startIndex)
		{
		}

	}

	public class ClockSynchronizationCommand : InformationObject
	{

		private CP56Time2a newTime;

		public CP56Time2a NewTime {
			get {
				return this.newTime;
			}
			set {
				newTime = value;
			}
		}

		public ClockSynchronizationCommand (int ioa, CP56Time2a newTime) : base(ioa)
		{
			this.newTime = newTime;
		}

		public ClockSynchronizationCommand (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse CP56Time2a (time stamp) */
			newTime = new CP56Time2a (msg, startIndex);
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.AppendBytes (newTime.GetEncodedValue ());
		}
	}

	public class ResetProcessCommand : InformationObject
	{
		byte qrp;

		/// <summary>
		/// Gets or sets the QRP (Qualifier of reset process command).
		/// </summary>
		/// <value>The QRP</value>
		public byte QRP {
			get {
				return this.qrp;
			}
			set {
				qrp = value;
			}
		}

		public ResetProcessCommand (int ioa, byte qrp) : base(ioa)
		{
			this.qrp = qrp;
		}

		public ResetProcessCommand (ConnectionParameters parameters, byte[] msg, int startIndex) :
		base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			qrp = msg [startIndex++];
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.SetNextByte (qrp);
		}

	}

	public class DelayAcquisitionCommand : InformationObject
	{

		private CP16Time2a delay;

		public CP16Time2a Delay {
			get {
				return this.delay;
			}
			set {
				delay = value;
			}
		}

		public DelayAcquisitionCommand (int ioa, CP16Time2a delay) : base(ioa)
		{
			this.delay = delay;
		}

		public DelayAcquisitionCommand (ConnectionParameters parameters, byte[] msg, int startIndex) :
		base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse CP16Time2a (time stamp) */
			delay = new CP16Time2a (msg, startIndex);
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.AppendBytes (delay.GetEncodedValue ());
		}
	}

}

