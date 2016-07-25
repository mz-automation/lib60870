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

	public class SetpointCommandShortFloat : InformationObject 
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

		public SetpointCommandShortFloat (int objectAddress, float value, SetpointCommandQualifier qos)
			: base(objectAddress)
		{
			this.value = value;
			this.qos = qos;
		}

		public SetpointCommandShortFloat (ConnectionParameters parameters, byte[] msg, int startIndex) :
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
}
