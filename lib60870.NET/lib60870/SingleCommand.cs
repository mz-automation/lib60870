using System;

namespace lib60870
{
	public class SingleCommand : InformationObject
	{

		private byte sco;

		public SingleCommand (int ioa, bool command, bool selectCommand, int qu) : base(ioa)
		{
			sco = (byte) ((qu & 0x1f) * 4);

			if (command) sco |= 0x01;

			if (selectCommand)
				sco |= 0x80;
		}

		public SingleCommand (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			sco = msg [startIndex++];
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.SetNextByte (sco);
		}

		public int QU {
			get {
				return ((sco & 0x7c) / 4);
			}
		}

		public bool State {
			get {
				return ((sco & 0x01) == 0x01);
			}
		}

		public bool Select {
			get {
				return ((sco & 0x80) == 0x80);
			}
		}
	}

	public class SingleCommandWithCP56Time2a : SingleCommand
	{
		private CP56Time2a timestamp;

		public SingleCommandWithCP56Time2a (int ioa, bool command, bool selectCommand, int qu, CP56Time2a timestamp) : 
			base(ioa, command, selectCommand, qu)
		{
			this.timestamp = timestamp;
		}

		public SingleCommandWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA + 1; /* skip IOA + SCQ*/

			timestamp = new CP56Time2a (msg, startIndex);
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.AppendBytes (timestamp.GetEncodedValue ());
		}

		public CP56Time2a Timestamp {
			get {
				return timestamp;
			}
		}

	}

	public class DoubleCommand : InformationObject
	{
		public static int OFF = 1;
		public static int ON = 2;

		private byte dcq;

		public DoubleCommand (int ioa, int command, bool select, int quality) : base(ioa)
		{
			dcq = (byte) (command & 0x03);
			dcq += (byte)((quality & 0x1f) * 4);

			if (select)
				dcq |= 0x80;
		}

		public DoubleCommand (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			dcq = msg [startIndex++];
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.SetNextByte (dcq);
		}

		public int QU {
			get {
				return ((dcq & 0x7c) / 4);
			}
		}

		public int State {
			get {
				return (dcq & 0x03);
			}
		}

		public bool Select {
			get {
				return ((dcq & 0x80) == 0x80);
			}
		}
	}

	public class StepCommand : DoubleCommand 
	{
		public static int LOWER = 1;
		public static int HIGHER = 2;

		public StepCommand (int ioa, int command, bool select, int quality) : base(ioa, command, select, quality)
		{
		}

		public StepCommand (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
		}
	}

}

