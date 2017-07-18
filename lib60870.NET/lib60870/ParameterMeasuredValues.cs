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

	public class ParameterNormalizedValue : InformationObject
	{
		override public int GetEncodedSize() {
			return 3;
		}

		override public TypeID Type {
			get {
				return TypeID.P_ME_NA_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return false;
			}
		}

		private ScaledValue scaledValue;

        public short RawValue
        {
            get
            {
                return scaledValue.ShortValue;
            }
            set
            {
                scaledValue.ShortValue = value;
            }
        }

        public float NormalizedValue
        {
            get
            {
                return (float)(scaledValue.Value + 0.5) / (float)32767.5;
            }
            set
            {
                /* Check value range */
                if (value > 1.0f)
                    value = 1.0f;
                else if (value < -1.0f)
                    value = -1.0f;

                this.scaledValue.Value = (int)((value * 32767.5) - 0.5);
            }
        }

        private byte qpm;

		public byte QPM {
			get {
				return qpm;
			}
		}

		public ParameterNormalizedValue (int objectAddress, float normalizedValue, byte qpm) :
			base (objectAddress)
		{
            scaledValue = new ScaledValue();

			this.NormalizedValue = normalizedValue;

			this.qpm = qpm;
		}

        public ParameterNormalizedValue (int objectAddress, short rawValue, byte qpm) :
            base (objectAddress)
        {
            scaledValue = new ScaledValue(rawValue);
            this.qpm = qpm;
        }

		internal ParameterNormalizedValue (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex, false)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			scaledValue = new ScaledValue (msg, startIndex);
			startIndex += 2;

			/* parse QDS (quality) */
			qpm = msg [startIndex++];
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.AppendBytes (scaledValue.GetEncodedValue ());

			frame.SetNextByte (qpm);
		}
	}

	public class ParameterScaledValue : InformationObject
	{
		override public int GetEncodedSize() {
			return 3;
		}

		override public TypeID Type {
			get {
				return TypeID.P_ME_NB_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return false;
			}
		}

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

		internal ParameterScaledValue (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex, false)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			scaledValue = new ScaledValue (msg, startIndex);
			startIndex += 2;

			/* parse QDS (quality) */
			qpm = msg [startIndex++];
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.AppendBytes (scaledValue.GetEncodedValue ());

			frame.SetNextByte (qpm);
		}
	}

	public class ParameterFloatValue : InformationObject
	{
		override public int GetEncodedSize() {
			return 5;
		}

		override public TypeID Type {
			get {
				return TypeID.P_ME_NC_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return false;
			}
		}

		private float value;

		public float Value {
			get {
				return this.value;
			}
		}

		private byte qpm;

		public float QPM {
			get {
				return qpm;
			}
		}

		public ParameterFloatValue (int objectAddress, float value, byte qpm) :
			base (objectAddress)
		{
			this.value = value;

			this.qpm = qpm;
		}

		internal ParameterFloatValue (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex, false)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse float value */
			value = System.BitConverter.ToSingle (msg, startIndex);
			startIndex += 4;

			/* parse QDS (quality) */
			qpm = msg [startIndex++];
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			byte[] floatEncoded = BitConverter.GetBytes (value);

			if (BitConverter.IsLittleEndian == false)
				Array.Reverse (floatEncoded);

			frame.AppendBytes (floatEncoded);

			frame.SetNextByte (qpm);
		}

	}

	public class ParameterActivation : InformationObject
	{
		override public int GetEncodedSize() {
			return 1;
		}

		override public TypeID Type {
			get {
				return TypeID.P_AC_NA_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return false;
			}
		}

		private byte qpa;

		public static byte NOT_USED = 0;
		public static byte DE_ACT_PREV_LOADED_PARAMETER = 1;
		public static byte DE_ACT_OBJECT_PARAMETER = 2;
		public static byte DE_ACT_OBJECT_TRANSMISSION= 3;

		/// <summary>
		/// Gets the Qualifier of Parameter Activation (QPA)
		/// </summary>
		/// <value>The QPA value</value>
		public byte QPA {
			get {
				return qpa;
			}
		}

		public ParameterActivation (int objectAddress, byte qpa) :
			base (objectAddress)
		{
			this.qpa = qpa;
		}

		internal ParameterActivation (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex, false)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse QPA */
			qpa = msg [startIndex++];
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.SetNextByte (qpa);
		}

	}


}
