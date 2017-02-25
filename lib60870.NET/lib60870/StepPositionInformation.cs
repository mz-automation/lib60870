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
	public class StepPositionInformation : InformationObject
	{
		override public int GetEncodedSize() {
			return 2;
		}

		override public TypeID Type {
			get {
				return TypeID.M_ST_NA_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return true;
			}
		}

		private int value;

		/// <summary>
		/// Step position (range -64 ... +63)
		/// </summary>
		/// <value>The value.</value>
		public int Value {
			get {
				return this.value;
			}
			set {
				if (value > 63)
					this.value = 63;
				else if (value < -64)
					this.value = -64;
				else
					this.value = value;
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
			set {
				this.isTransient = value;
			}
		}

		private QualityDescriptor quality;

		public QualityDescriptor Quality {
			get {
				return this.quality;
			}
		}

		public StepPositionInformation(int ioa, int value, bool isTransient, QualityDescriptor quality) :
			base(ioa)
		{
			if ((value < -64) || (value > 63))
				throw new ArgumentOutOfRangeException ("value has to be in range -64 .. 63");

			Value = value;
			Transient = isTransient;
			this.quality = quality;
		}

		internal StepPositionInformation (ConnectionParameters parameters, byte[] msg, int startIndex, bool isSequence) :
			base(parameters, msg, startIndex, isSequence)
		{
			if (!isSequence)
				startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse VTI (value with transient state indication) */
			byte vti = msg [startIndex++];

			isTransient = ((vti & 0x80) == 0x80);

			value = (vti & 0x7f);

			if (value > 63)
				value = value - 128;

			quality = new QualityDescriptor (msg[startIndex++]);
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			byte vti;

			if (value < 0)
				vti = (byte)(value + 128);
			else
				vti = (byte)value;

			if (isTransient)
				vti += 0x80;

			frame.SetNextByte (vti);
				
			frame.SetNextByte (quality.EncodedValue);
		}

	}

	public class StepPositionWithCP24Time2a : StepPositionInformation
	{
		override public int GetEncodedSize() {
			return 5;
		}

		override public TypeID Type {
			get {
				return TypeID.M_ST_TA_1;
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
			set {
				this.timestamp = value;
			}
		}
			
		public StepPositionWithCP24Time2a(int ioa, int value, bool isTransient, QualityDescriptor quality, CP24Time2a timestamp) :
		base(ioa, value, isTransient, quality)
		{
			Timestamp = timestamp;
		}

		internal StepPositionWithCP24Time2a (ConnectionParameters parameters, byte[] msg, int startIndex, bool isSequence) :
		base(parameters, msg, startIndex, isSequence)
		{
			if (!isSequence)
				startIndex += parameters.SizeOfIOA; /* skip IOA */

			startIndex += 2; /* VTI + quality*/

			/* parse CP24Time2a (time stamp) */
			timestamp = new CP24Time2a (msg, startIndex);
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.AppendBytes (timestamp.GetEncodedValue ());
		}

	}

	public class StepPositionWithCP56Time2a : StepPositionInformation
	{
		override public int GetEncodedSize() {
			return 9;
		}

		override public TypeID Type {
			get {
				return TypeID.M_ST_TB_1;
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
			set {
				this.timestamp = value;
			}
		}

		public StepPositionWithCP56Time2a(int ioa, int value, bool isTransient, QualityDescriptor quality, CP56Time2a timestamp) :
		base(ioa, value, isTransient, quality)
		{
			Timestamp = timestamp;
		}


		internal StepPositionWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex, bool isSequence) :
		base(parameters, msg, startIndex, isSequence)
		{
			if (!isSequence)
				startIndex += parameters.SizeOfIOA; /* skip IOA */

			startIndex += 2; /* skip VTI + quality*/

			/* parse CP24Time2a (time stamp) */
			timestamp = new CP56Time2a (msg, startIndex);
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.AppendBytes (timestamp.GetEncodedValue ());
		}
	}
	
}

