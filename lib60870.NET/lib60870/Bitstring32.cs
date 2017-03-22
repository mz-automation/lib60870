/*
 *  BitString32.cs
 *
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
	
	public class Bitstring32 : InformationObject
	{
		override public int GetEncodedSize() {
			return 5;
		}

		override public TypeID Type {
			get {
				return TypeID.M_BO_NA_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return true;
			}
		}

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

		public Bitstring32 (int ioa, UInt32 value, QualityDescriptor quality) : base(ioa)
		{
			this.value = value;
			this.quality = quality;
		}

		internal Bitstring32 (ConnectionParameters parameters, byte[] msg, int startIndex, bool isSequence) :
			base(parameters, msg, startIndex, isSequence)
		{
			if (!isSequence)
				startIndex += parameters.SizeOfIOA; /* skip IOA */

			value = msg [startIndex++];
			value += ((uint)msg [startIndex++] * 0x100);
			value += ((uint)msg [startIndex++] * 0x10000);
			value += ((uint)msg [startIndex++] * 0x1000000);

			quality = new QualityDescriptor (msg[startIndex++]);

		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.SetNextByte((byte) (value % 0x100));
			frame.SetNextByte((byte) ((value / 0x100) % 0x100));
			frame.SetNextByte((byte) ((value / 0x10000) % 0x100));
			frame.SetNextByte((byte) (value / 0x1000000));

			frame.SetNextByte (quality.EncodedValue);
		}
	}

	public class Bitstring32WithCP24Time2a : Bitstring32
	{
		override public int GetEncodedSize() {
			return 8;
		}

		override public TypeID Type {
			get {
				return TypeID.M_BO_TA_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return false;
			}
		}

		private CP24Time2a timestamp;

		public CP24Time2a Timestamp {
			get {
				return this.timestamp;
			}
		}

		public Bitstring32WithCP24Time2a(int ioa, UInt32 value, QualityDescriptor quality, CP24Time2a timestamp) :
			base(ioa, value, quality)
		{
			this.timestamp = timestamp;
		}

		internal Bitstring32WithCP24Time2a (ConnectionParameters parameters, byte[] msg, int startIndex, bool isSequence) :
		base(parameters, msg, startIndex, isSequence)
		{
			if (!isSequence)
				startIndex += parameters.SizeOfIOA; /* skip IOA */

			startIndex += 5; /* value + quality */

			/* parse CP24Time2a (time stamp) */
			timestamp = new CP24Time2a (msg, startIndex);
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.AppendBytes (timestamp.GetEncodedValue ());
		}
	}

	public class Bitstring32WithCP56Time2a : Bitstring32
	{
		override public int GetEncodedSize() {
			return 12;
		}

		override public TypeID Type {
			get {
				return TypeID.M_BO_TB_1;
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
				return this.timestamp;
			}
		}

		public Bitstring32WithCP56Time2a(int ioa, UInt32 value, QualityDescriptor quality, CP56Time2a timestamp) :
			base(ioa, value, quality)
		{
			this.timestamp = timestamp;
		}

		internal Bitstring32WithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex, bool isSequence) :
		base(parameters, msg, startIndex, isSequence)
		{
			if (!isSequence)
				startIndex += parameters.SizeOfIOA; /* skip IOA */

			startIndex += 5; /* value + quality */

			/* parse CP56Time2a (time stamp) */
			timestamp = new CP56Time2a (msg, startIndex);
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.AppendBytes (timestamp.GetEncodedValue ());
		}

	}

}

