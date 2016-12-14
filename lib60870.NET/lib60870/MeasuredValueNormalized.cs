/*
 *  MeasuredValueNormalized.cs
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

	public class MeasuredValueNormalizedWithoutQuality : InformationObject
	{
		override public TypeID Type {
			get {
				return TypeID.M_ME_ND_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return false;
			}
		}

		private ScaledValue scaledValue;

		public float NormalizedValue {
			get {
				float nv = (float) (scaledValue.Value) / 32767f;

				return nv;
			}
			set {
				if (value > 1.0f)
					value = 1.0f;
				else if (value < -1.0f)
					value = -1.0f;
				
				scaledValue.Value = (int)(value * 32767f); 
			}
		}

		public MeasuredValueNormalizedWithoutQuality (int objectAddress, float value)
			: base(objectAddress)
		{
			this.scaledValue = new ScaledValue ((int)(value * 32767f));

			this.NormalizedValue = value;
		}

		internal MeasuredValueNormalizedWithoutQuality (ConnectionParameters parameters, byte[] msg, int startIndex, bool isSequence) :
			base(parameters, msg, startIndex, false)
		{
			if (!isSequence)
				startIndex += parameters.SizeOfIOA; /* skip IOA */

			scaledValue = new ScaledValue (msg, startIndex);
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.AppendBytes (scaledValue.GetEncodedValue ());
		}
	}


	public class MeasuredValueNormalized : MeasuredValueNormalizedWithoutQuality
	{
		override public TypeID Type {
			get {
				return TypeID.M_ME_NA_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return true;
			}
		}

		private QualityDescriptor quality;

		public QualityDescriptor Quality {
			get {
				return this.quality;
			}
		}

		public MeasuredValueNormalized (int objectAddress, float value, QualityDescriptor quality)
			: base(objectAddress, value)
		{
			this.quality = quality;
		}

		internal MeasuredValueNormalized (ConnectionParameters parameters, byte[] msg, int startIndex, bool isSequence) :
			base(parameters, msg, startIndex, isSequence)
		{
			if (!isSequence)
				startIndex += parameters.SizeOfIOA + 2; /* skip IOA + normalized value */
			else
				startIndex += 2; /* normalized value */

			/* parse QDS (quality) */
			quality = new QualityDescriptor (msg [startIndex++]);
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.SetNextByte (quality.EncodedValue);
		}
	}
	

	public class MeasuredValueNormalizedWithCP24Time2a : MeasuredValueNormalized
	{
		override public TypeID Type {
			get {
				return TypeID.M_ME_TA_1;
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


		public MeasuredValueNormalizedWithCP24Time2a (int objectAddress, float value, QualityDescriptor quality, CP24Time2a timestamp)
			: base(objectAddress, value, quality)
		{
			this.timestamp = timestamp;
		}

		internal MeasuredValueNormalizedWithCP24Time2a (ConnectionParameters parameters, byte[] msg, int startIndex, bool isSequence) :
		base(parameters, msg, startIndex, isSequence)
		{
			if (!isSequence)
				startIndex += parameters.SizeOfIOA; /* skip IOA */

			startIndex += 3; /* normalized value + quality */

			/* parse CP24Time2a (time stamp) */
			timestamp = new CP24Time2a (msg, startIndex);
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.AppendBytes (timestamp.GetEncodedValue ());
		}
	}

	public class MeasuredValueNormalizedWithCP56Time2a : MeasuredValueNormalized
	{
		override public TypeID Type {
			get {
				return TypeID.M_ME_TD_1;
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

		public MeasuredValueNormalizedWithCP56Time2a (int objectAddress, float value, QualityDescriptor quality, CP56Time2a timestamp)
			: base(objectAddress, value, quality)
		{
			this.timestamp = timestamp;
		}

		internal MeasuredValueNormalizedWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex, bool isSequence) :
		base(parameters, msg, startIndex, isSequence)
		{
			if (!isSequence)
				startIndex += parameters.SizeOfIOA; /* skip IOA */

			startIndex += 3; /* normalized value + quality */

			/* parse CP56Time2a (time stamp) */
			timestamp = new CP56Time2a (msg, startIndex);
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.AppendBytes (timestamp.GetEncodedValue ());
		}
	}



}

