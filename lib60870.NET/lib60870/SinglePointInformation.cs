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
		private bool value;

		public bool Value {
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

		public SinglePointInformation (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
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

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			byte val = quality.EncodedValue;

			if (value)
				val++;

			frame.SetNextByte (val);
		}

	}

	public class PackedSinglePointWithSCD : InformationObject
	{
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

		public PackedSinglePointWithSCD (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
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

		public override void Encode(Frame frame, ConnectionParameters parameters) {
			base.Encode(frame, parameters);

			frame.AppendBytes (scd.GetEncodedValue ());

			frame.SetNextByte (qds.EncodedValue);
		}
	}

}

