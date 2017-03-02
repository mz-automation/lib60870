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
	public class SinglePointInformation : InformationObject
	{
		override public int GetEncodedSize() {
			return 1;
		}

		override public TypeID Type {
			get {
				return TypeID.M_SP_NA_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return true;
			}
		}

		private bool value;

		public bool Value {
			get {
				return this.value;
			}
			set {
				this.value = value;
			}
		}

		private QualityDescriptor quality;

		public QualityDescriptor Quality {
			get {
				return this.quality;
			}
		}

		internal SinglePointInformation (ConnectionParameters parameters, byte[] msg, int startIndex, bool isSequence) :
			base(parameters, msg, startIndex, isSequence)
		{
			if (!isSequence)
				startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse SIQ (single point information with qualitiy) */
			byte siq = msg [startIndex++];

			value = ((siq & 0x01) == 0x01);

			quality = new QualityDescriptor ((byte) (siq & 0xf0));
		}


		public SinglePointInformation(int objectAddress, bool value, QualityDescriptor quality):
			base(objectAddress)
		{
			this.value = value;
			this.quality = quality;
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			byte val = quality.EncodedValue;

			if (value)
				val++;

			frame.SetNextByte (val);
		}

	}

	public class SinglePointWithCP24Time2a : SinglePointInformation
	{
		override public int GetEncodedSize() {
			return 4;
		}

		override public TypeID Type {
			get {
				return TypeID.M_SP_TA_1;
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

		internal SinglePointWithCP24Time2a (ConnectionParameters parameters, byte[] msg, int startIndex, bool isSequence) :
		base(parameters, msg, startIndex, isSequence)
		{
			if (!isSequence)
				startIndex += parameters.SizeOfIOA; /* skip IOA */
		
			startIndex += 1; /* skip SIQ */

			/* parse CP24Time2a (time stamp) */
			timestamp = new CP24Time2a (msg, startIndex);
		}

		public SinglePointWithCP24Time2a(int objectAddress, bool value, QualityDescriptor quality, CP24Time2a timestamp):
			base(objectAddress, value, quality)
		{
			this.timestamp = timestamp;
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.AppendBytes (timestamp.GetEncodedValue ());
		}
	}

	/// <summary>
	/// Single point with CP56Time2a timestamp (M_SP_TB_1)
	/// </summary>
	public class SinglePointWithCP56Time2a : SinglePointInformation
	{
		override public int GetEncodedSize() {
			return 8;
		}

		override public TypeID Type {
			get {
				return TypeID.M_SP_TB_1;
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

		internal SinglePointWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex, bool isSequence) :
		base(parameters, msg, startIndex, isSequence)
		{
			if (!isSequence)
				startIndex += parameters.SizeOfIOA; /* skip IOA */

			startIndex += 1; /* skip SIQ */

			/* parse CP56Time2a (time stamp) */
			timestamp = new CP56Time2a (msg, startIndex);
		}

		public SinglePointWithCP56Time2a(int objectAddress, bool value, QualityDescriptor quality, CP56Time2a timestamp):
		base(objectAddress, value, quality)
		{
			this.timestamp = timestamp;
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.AppendBytes (timestamp.GetEncodedValue ());
		}
	}

	public class PackedSinglePointWithSCD : InformationObject
	{
		override public int GetEncodedSize() {
			return 5;
		}

		override public TypeID Type {
			get {
				return TypeID.M_PS_NA_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return true;
			}
		}


		private StatusAndStatusChangeDetection scd;

		private QualityDescriptor qds;

		public StatusAndStatusChangeDetection SCD {
			get {
				return this.scd;
			}
			set {
				scd = value;
			}
		}

		public QualityDescriptor QDS {
			get {
				return this.qds;
			}
			set {
				qds = value;
			}
		}

		internal PackedSinglePointWithSCD (ConnectionParameters parameters, byte[] msg, int startIndex, bool isSquence) :
			base(parameters, msg, startIndex, isSquence)
		{
			if (!isSquence)
				startIndex += parameters.SizeOfIOA; /* skip IOA */

			scd = new StatusAndStatusChangeDetection (msg, startIndex);
			startIndex += 4;

			qds = new QualityDescriptor (msg [startIndex++]);
		}
			
		public PackedSinglePointWithSCD(int objectAddress, StatusAndStatusChangeDetection scd, QualityDescriptor quality)
			:base(objectAddress)
		{
			this.scd = scd;
			this.qds = quality;
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.AppendBytes (scd.GetEncodedValue ());

			frame.SetNextByte (qds.EncodedValue);
		}
	}

}

