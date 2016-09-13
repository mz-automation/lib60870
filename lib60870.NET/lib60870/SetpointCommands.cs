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
		private int scaledValue;

		public float NormalizedValue {
			get {
				float nv = (float) scaledValue / 32767f;

				return nv;
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
			// TODO check if value is in range

			this.scaledValue = (int) (value * 32767f);
			this.qos = qos;
		}

		public SetpointCommandNormalized (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			scaledValue = msg [startIndex++];
			scaledValue += (msg [startIndex++] * 0x100);

			if (scaledValue > 32767)
				scaledValue = scaledValue - 65536;

			this.qos = new SetpointCommandQualifier (msg [startIndex++]);
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			int valueToEncode;

			if (scaledValue < 0)
				valueToEncode = scaledValue + 65536;
			else
				valueToEncode = scaledValue;

			frame.SetNextByte ((byte)(valueToEncode % 256));
			frame.SetNextByte ((byte)(valueToEncode / 256));

			frame.SetNextByte (this.qos.GetEncodedValue ());
		}
	}

	public class SetpointCommandNormalizedWithCP56Time2a : SetpointCommandNormalized
	{
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

		public SetpointCommandNormalizedWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
		base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA + 3; /* skip IOA */

			this.timestamp = new CP56Time2a (msg, startIndex);
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.AppendBytes (this.timestamp.GetEncodedValue ());
		}
	}

	public class SetpointCommandScaled : InformationObject 
	{
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

		public SetpointCommandScaled (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			scaledValue = new ScaledValue (msg, startIndex);
			startIndex += 2;

			this.qos = new SetpointCommandQualifier (msg [startIndex++]);
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.AppendBytes (this.scaledValue.GetEncodedValue ());

			frame.SetNextByte (this.qos.GetEncodedValue ());
		}
	}

	public class SetpointCommandScaledWithCP56Time2a : SetpointCommandScaled
	{
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

		public SetpointCommandScaledWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
		base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA + 3; /* skip IOA */

			this.timestamp = new CP56Time2a (msg, startIndex);
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.AppendBytes (this.timestamp.GetEncodedValue ());
		}
	}

	public class SetpointCommandShort : InformationObject 
	{
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

		public SetpointCommandShort (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse float value */
			value = System.BitConverter.ToSingle (msg, startIndex);
			startIndex += 4;

			this.qos = new SetpointCommandQualifier (msg [startIndex++]);
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.AppendBytes (System.BitConverter.GetBytes (value));

			frame.SetNextByte (this.qos.GetEncodedValue ());
		}
	}

	public class SetpointCommandShortWithCP56Time2a : SetpointCommandShort
	{
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

		public SetpointCommandShortWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
		base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA + 5; /* skip IOA + float + QOS*/

			this.timestamp = new CP56Time2a (msg, startIndex);
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.AppendBytes (this.timestamp.GetEncodedValue ());
		}
	}


	public class Bitstring32Command : InformationObject
	{

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

		public Bitstring32Command (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			value = msg [startIndex++];
			value += ((uint)msg [startIndex++] * 0x100);
			value += ((uint)msg [startIndex++] * 0x10000);
			value += ((uint)msg [startIndex++] * 0x1000000);
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.SetNextByte ((byte) (value % 256));
			frame.SetNextByte ((byte) ((value / 0x100) % 256));
			frame.SetNextByte ((byte) ((value / 0x10000) % 256));
			frame.SetNextByte ((byte) ((value / 0x1000000) % 256));
		}
	}

	public class Bitstring32CommandWithCP56Time2a : Bitstring32Command
	{
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

		public Bitstring32CommandWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
		base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA + 4; /* skip IOA + bitstring */

			this.timestamp = new CP56Time2a (msg, startIndex);
		}

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.AppendBytes (this.timestamp.GetEncodedValue ());
		}
	}
}
