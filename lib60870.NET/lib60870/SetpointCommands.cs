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

	public class SetpointCommandNormalized : InformationObject
	{
		override public int GetEncodedSize() {
			return 3;
		}

		override public TypeID Type {
			get {
				return TypeID.C_SE_NA_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return false;
			}
		}

		private ScaledValue scaledValue;

		public short RawValue {
			get {
				return scaledValue.ShortValue;
			}
			set {
				scaledValue.ShortValue = value;
			}
		}

		public float NormalizedValue {
			get {
				return (float) (scaledValue.Value + 0.5) / (float) 32767.5;
			}
            set
            {
                /* Check value range */
                if (value > 1.0f)
                    value = 1.0f;
                else if (value < -1.0f)
                    value = -1.0f;

                this.scaledValue.Value = (int) ((value * 32767.5) - 0.5);
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
			this.scaledValue = new ScaledValue((int) ((value * 32767.5) - 0.5));
			this.qos = qos;
		}

		public SetpointCommandNormalized (int ObjectAddress, short value, SetpointCommandQualifier qos)
			: base (ObjectAddress)
		{
			this.scaledValue = new ScaledValue (value);
			this.qos = qos;
		}

		internal SetpointCommandNormalized (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex, false)
		{
            startIndex += parameters.SizeOfIOA; /* skip IOA */

            scaledValue = new ScaledValue(msg, startIndex);
            startIndex += 2;

			this.qos = new SetpointCommandQualifier (msg [startIndex++]);
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

            frame.AppendBytes(scaledValue.GetEncodedValue());

			frame.SetNextByte (this.qos.GetEncodedValue ());
		}
	}

	public class SetpointCommandNormalizedWithCP56Time2a : SetpointCommandNormalized
	{
		override public int GetEncodedSize() {
			return 10;
		}

		override public TypeID Type {
			get {
				return TypeID.C_SE_TA_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return false;
			}
		}

		private CP56Time2a timestamp;

		public CP56Time2a Timestamp {
			get {
				return timestamp;
			}
		}

		public SetpointCommandNormalizedWithCP56Time2a (int objectAddress, float value, SetpointCommandQualifier qos, CP56Time2a timestamp)
			: base(objectAddress, value, qos)
		{
			this.timestamp = timestamp;
		}

		public SetpointCommandNormalizedWithCP56Time2a (int objectAddress, short value, SetpointCommandQualifier qos, CP56Time2a timestamp)
			: base(objectAddress, value, qos)
		{
			this.timestamp = timestamp;
		}

		internal SetpointCommandNormalizedWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA + 3; /* skip IOA */

			this.timestamp = new CP56Time2a (msg, startIndex);
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.AppendBytes (this.timestamp.GetEncodedValue ());
		}
	}

	public class SetpointCommandScaled : InformationObject 
	{
		override public int GetEncodedSize() {
			return 3;
		}

		override public TypeID Type {
			get {
				return TypeID.C_SE_NB_1;
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
		}

		private SetpointCommandQualifier qos;

		public SetpointCommandQualifier QOS {
			get {
				return qos;
			}
		}

		public SetpointCommandScaled (int objectAddress, ScaledValue value, SetpointCommandQualifier qos)
			: base(objectAddress)
		{
			this.scaledValue = value;
			this.qos = qos;
		}

		internal SetpointCommandScaled (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex, false)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			scaledValue = new ScaledValue (msg, startIndex);
			startIndex += 2;

			this.qos = new SetpointCommandQualifier (msg [startIndex++]);
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.AppendBytes (this.scaledValue.GetEncodedValue ());

			frame.SetNextByte (this.qos.GetEncodedValue ());
		}
	}

	public class SetpointCommandScaledWithCP56Time2a : SetpointCommandScaled
	{
		override public int GetEncodedSize() {
			return 10;
		}

		override public TypeID Type {
			get {
				return TypeID.C_SE_TB_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return false;
			}
		}

		private CP56Time2a timestamp;

		public CP56Time2a Timestamp {
			get {
				return timestamp;
			}
		}

		public SetpointCommandScaledWithCP56Time2a (int objectAddress, ScaledValue value, SetpointCommandQualifier qos, CP56Time2a timestamp)
			: base(objectAddress, value, qos)
		{
			this.timestamp = timestamp;
		}

		internal SetpointCommandScaledWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA + 3; /* skip IOA */

			this.timestamp = new CP56Time2a (msg, startIndex);
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.AppendBytes (this.timestamp.GetEncodedValue ());
		}
	}

	public class SetpointCommandShort : InformationObject 
	{
		override public int GetEncodedSize() {
			return 5;
		}

		override public TypeID Type {
			get {
				return TypeID.C_SE_NC_1;
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
				return value;
			}
		}

		private SetpointCommandQualifier qos;

		public SetpointCommandQualifier QOS {
			get {
				return qos;
			}
		}

		public SetpointCommandShort (int objectAddress, float value, SetpointCommandQualifier qos)
			: base(objectAddress)
		{
			this.value = value;
			this.qos = qos;
		}

		internal SetpointCommandShort (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex, false)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse float value */
			value = System.BitConverter.ToSingle (msg, startIndex);
			startIndex += 4;

			this.qos = new SetpointCommandQualifier (msg [startIndex++]);
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.AppendBytes (System.BitConverter.GetBytes (value));

			frame.SetNextByte (this.qos.GetEncodedValue ());
		}
	}

	public class SetpointCommandShortWithCP56Time2a : SetpointCommandShort
	{
		override public int GetEncodedSize() {
			return 12;
		}

		override public TypeID Type {
			get {
				return TypeID.C_SE_TC_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return false;
			}
		}

		private CP56Time2a timestamp;

		public CP56Time2a Timestamp {
			get {
				return timestamp;
			}
		}

		public SetpointCommandShortWithCP56Time2a (int objectAddress, float value, SetpointCommandQualifier qos, CP56Time2a timestamp)
			: base(objectAddress, value, qos)
		{
			this.timestamp = timestamp;
		}

		internal SetpointCommandShortWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA + 5; /* skip IOA + float + QOS*/

			this.timestamp = new CP56Time2a (msg, startIndex);
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.AppendBytes (this.timestamp.GetEncodedValue ());
		}
	}


	public class Bitstring32Command : InformationObject
	{
		override public int GetEncodedSize() {
			return 4;
		}

		override public TypeID Type {
			get {
				return TypeID.C_BO_NA_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return false;
			}
		}

		private UInt32 value;

		public UInt32 Value {
			get {
				return this.value;
			}
		}

		public Bitstring32Command (int objectAddress, UInt32 bitstring) :
			base (objectAddress)
		{
			this.value = bitstring;
		}

		internal Bitstring32Command (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex, false)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			value = msg [startIndex++];
			value += ((uint)msg [startIndex++] * 0x100);
			value += ((uint)msg [startIndex++] * 0x10000);
			value += ((uint)msg [startIndex++] * 0x1000000);
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.SetNextByte ((byte) (value % 256));
			frame.SetNextByte ((byte) ((value / 0x100) % 256));
			frame.SetNextByte ((byte) ((value / 0x10000) % 256));
			frame.SetNextByte ((byte) ((value / 0x1000000) % 256));
		}
	}

	public class Bitstring32CommandWithCP56Time2a : Bitstring32Command
	{
		override public int GetEncodedSize() {
			return 11;
		}

		override public TypeID Type {
			get {
				return TypeID.C_BO_TA_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return false;
			}
		}

		private CP56Time2a timestamp;

		public CP56Time2a Timestamp {
			get {
				return timestamp;
			}
		}

		public Bitstring32CommandWithCP56Time2a (int objectAddress, UInt32 bitstring, CP56Time2a timestamp)
			: base(objectAddress, bitstring)
		{
			this.timestamp = timestamp;
		}

		internal Bitstring32CommandWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA + 4; /* skip IOA + bitstring */

			this.timestamp = new CP56Time2a (msg, startIndex);
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.AppendBytes (this.timestamp.GetEncodedValue ());
		}
	}
}
