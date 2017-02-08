/*
 *  IntegratedTotals.cs
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
	public class IntegratedTotals : InformationObject
	{
		override public int GetEncodedSize() {
			return 5;
		}

		override public TypeID Type {
			get {
				return TypeID.M_IT_NA_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return true;
			}
		}

		private BinaryCounterReading bcr;

		public BinaryCounterReading BCR {
			get {
				return bcr;
			}
		}

		public IntegratedTotals(int ioa, BinaryCounterReading bcr)
			:base(ioa)
		{
			this.bcr = bcr;
		}

		internal IntegratedTotals (ConnectionParameters parameters, byte[] msg, int startIndex, bool isSquence) :
			base(parameters, msg, startIndex, isSquence)
		{
			if (!isSquence)
				startIndex += parameters.SizeOfIOA; /* skip IOA */

			bcr = new BinaryCounterReading(msg, startIndex);
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.AppendBytes (bcr.GetEncodedValue ());
		}
	}

	public class IntegratedTotalsWithCP24Time2a : IntegratedTotals
	{
		override public int GetEncodedSize() {
			return 8;
		}

		override public TypeID Type {
			get {
				return TypeID.M_IT_TA_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return true;
			}
		}

		private CP24Time2a timestamp;

		public CP24Time2a Timestamp {
			get {
				return this.timestamp;
			}
		}

		public IntegratedTotalsWithCP24Time2a(int ioa, BinaryCounterReading bcr, CP24Time2a timestamp)
			:base(ioa, bcr)
		{
			this.timestamp = timestamp;
		}

		internal IntegratedTotalsWithCP24Time2a (ConnectionParameters parameters, byte[] msg, int startIndex, bool isSequence) :
		base(parameters, msg, startIndex, isSequence)
		{
			if (!isSequence)
				startIndex += parameters.SizeOfIOA; /* skip IOA */

			startIndex += 5; /* BCR */

			timestamp = new CP24Time2a (msg, startIndex);
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.AppendBytes (timestamp.GetEncodedValue ());
		}
	}

	public class IntegratedTotalsWithCP56Time2a : IntegratedTotals
	{
		override public int GetEncodedSize() {
			return 12;
		}

		override public TypeID Type {
			get {
				return TypeID.M_IT_TB_1;
			}
		}

		override public bool SupportsSequence {
			get {
				return true;
			}
		}

		private CP56Time2a timestamp;

		public CP56Time2a Timestamp {
			get {
				return this.timestamp;
			}
		}

		public IntegratedTotalsWithCP56Time2a(int ioa, BinaryCounterReading bcr, CP56Time2a timestamp)
			:base(ioa, bcr)
		{
			this.timestamp = timestamp;
		}

		public IntegratedTotalsWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex, bool isSequence) :
		base(parameters, msg, startIndex, isSequence)
		{
			if (!isSequence)
				startIndex += parameters.SizeOfIOA; /* skip IOA */

			startIndex += 5; /* BCR */

			timestamp = new CP56Time2a (msg, startIndex);
		}

		internal override void Encode(Frame frame, ConnectionParameters parameters, bool isSequence) {
			base.Encode(frame, parameters, isSequence);

			frame.AppendBytes (timestamp.GetEncodedValue ());
		}
	}
}

