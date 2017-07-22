/*
 *  Copyright 2016 MZ Automation GmbH
 *
 *  This file is part of lib60870.NET
 *
 *  lib60870.NET is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  lib60870.NET is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with lib60870.NET.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  See COPYING file for the complete license text.
 */

using System;

namespace lib60870
{

	public class QualifierOfInterrogation 
	{	
		public static byte STATION = 20;
		public static byte GROUP_1 = 21;
		public static byte GROUP_2 = 22;
		public static byte GROUP_3 = 23;
		public static byte GROUP_4 = 24;
		public static byte GROUP_5 = 25;
		public static byte GROUP_6 = 26;
		public static byte GROUP_7 = 27;
		public static byte GROUP_8 = 28;
		public static byte GROUP_9 = 29;
		public static byte GROUP_10 = 30;
		public static byte GROUP_11 = 31;
		public static byte GROUP_12 = 32;
		public static byte GROUP_13 = 33;
		public static byte GROUP_14 = 34;
		public static byte GROUP_15 = 35;
		public static byte GROUP_16 = 36;
	}

	public class InterrogationCommand : InformationObject
	{
		override public int GetEncodedSize() {
			return 1;
		}

		override public TypeID Type {
			get {
				return TypeID.C_IC_NA_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return false;
			}
		}

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

		internal InterrogationCommand (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex, false)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			qoi = msg [startIndex++];
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.SetNextByte (qoi);
		}

	}
		
	public class CounterInterrogationCommand : InformationObject
	{
		override public int GetEncodedSize() {
			return 1;
		}

		override public TypeID Type {
			get {
				return TypeID.C_CI_NA_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return false;
			}
		}

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

		internal CounterInterrogationCommand (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex, false)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			qcc = msg [startIndex++];
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.SetNextByte (qcc);
		}

	}


	public class ReadCommand : InformationObject
	{
		override public int GetEncodedSize() {
			return 0;
		}

		override public TypeID Type {
			get {
				return TypeID.C_RD_NA_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return false;
			}
		}

		public ReadCommand (int ioa) : base(ioa)
		{
		}

		internal ReadCommand (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex, false)
		{
		}
	}

    public class TestCommandWithCP56Time2a : InformationObject
    {
        private CP56Time2a time;

        private ushort tsc;

        /// <summary>
        /// Test time
        /// </summary>
        public CP56Time2a Time
        {
            get
            {
                return time;
            }

            set
            {
                time = value;
            }
        }

        /// <summary>
        /// Test sequence number
        /// </summary>
        public ushort TSC
        {
            get
            {
                return tsc;
            }

            set
            {
                tsc = value;
            }
        }

        override public int GetEncodedSize()
        {
            return 9;
        }

        override public TypeID Type
        {
            get
            {
                return TypeID.C_TS_TA_1;
            }
        }

        override public bool SupportsSequence
        {
            get
            {
                return false;
            }
        }

        public TestCommandWithCP56Time2a() : base(0)
        {
            time = new CP56Time2a();
        }

        public TestCommandWithCP56Time2a(ushort tsc, CP56Time2a time) : base(0)
        {
            this.time = time;
            this.tsc = tsc;
        }

        internal TestCommandWithCP56Time2a(ConnectionParameters parameters, byte[] msg, int startIndex) :
		base(parameters, msg, startIndex, false)
		{
            startIndex += parameters.SizeOfIOA; /* skip IOA */

            tsc = msg[startIndex++];
            tsc += (ushort) (msg[startIndex++] * 256);

            time = new CP56Time2a(msg, startIndex);
        }

        internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence)
        {
            base.Encode(frame, parameters, isSequence);

            frame.SetNextByte((byte)(tsc % 256));
            frame.SetNextByte((byte)(tsc / 256));

            frame.AppendBytes(time.GetEncodedValue());
        }
    }

	public class TestCommand : InformationObject
	{
		override public int GetEncodedSize() {
			return 2;
		}

		override public TypeID Type {
			get {
				return TypeID.C_TS_NA_1;
			}
		}

		private bool valid = true;

		public bool Valid {
			get {
				return valid;
			}
		}

		override public bool SupportsSequence {
			get {
				return false;
			}
		}

		public TestCommand () : base(0)
		{
		}


		internal TestCommand (ConnectionParameters parameters, byte[] msg, int startIndex) :
		base(parameters, msg, startIndex, false)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			if (msg [startIndex++] != 0xcc)
				valid = false;

			if (msg [startIndex] != 0x55)
				valid = false;
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.SetNextByte (0xcc);
			frame.SetNextByte (0x55);
		}
	}

	public class ClockSynchronizationCommand : InformationObject
	{
		override public int GetEncodedSize() {
			return 7;
		}

		override public TypeID Type {
			get {
				return TypeID.C_CS_NA_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return false;
			}
		}

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

		internal ClockSynchronizationCommand (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex, false)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse CP56Time2a (time stamp) */
			newTime = new CP56Time2a (msg, startIndex);
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.AppendBytes (newTime.GetEncodedValue ());
		}
	}

	public class ResetProcessCommand : InformationObject
	{
		override public int GetEncodedSize() {
			return 1;
		}

		override public TypeID Type {
			get {
				return TypeID.C_RP_NA_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return false;
			}
		}

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

		internal ResetProcessCommand (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex, false)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			qrp = msg [startIndex++];
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.SetNextByte (qrp);
		}

	}

	public class DelayAcquisitionCommand : InformationObject
	{
		override public int GetEncodedSize() {
			return 2;
		}

		override public TypeID Type {
			get {
				return TypeID.C_CD_NA_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return false;
			}
		}

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

		internal DelayAcquisitionCommand (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex, false)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse CP16Time2a (time stamp) */
			delay = new CP16Time2a (msg, startIndex);
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.AppendBytes (delay.GetEncodedValue ());
		}
	}

}

