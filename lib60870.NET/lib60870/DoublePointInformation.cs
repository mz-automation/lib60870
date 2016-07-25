/*
 *  DoublePointInformation.cs
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
	public enum DoublePointValue {
		INTERMEDIATE = 0,
		OFF = 1,
		ON = 2,
		INDETERMINATE = 3
	}

	public class DoublePointInformation : InformationObject
	{

		private DoublePointValue value;

		public DoublePointValue Value {
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

		public DoublePointInformation (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse DIQ (double point information with qualitiy) */
			byte siq = msg [startIndex++];

			value = (DoublePointValue)(siq & 0x03);

			quality = new QualityDescriptor ((byte) (siq & 0xf0));
		}
	}

	//TODO refactor: use DoublePointInformation as common base class

	public class DoublePointWithCP24Time2a : InformationObject
	{
		private DoublePointValue value;

		public DoublePointValue Value {
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

		private CP24Time2a timestamp;

		public CP24Time2a Timestamp {
			get {
				return this.timestamp;
			}
		}

		public DoublePointWithCP24Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse DIQ (double point information with qualitiy) */
			byte siq = msg [startIndex++];

			value = (DoublePointValue)(siq & 0x03);

			quality = new QualityDescriptor ((byte) (siq & 0xf0));

			/* parse CP24Time2a (time stamp) */
			timestamp = new CP24Time2a (msg, startIndex);
		}
	}

	public class DoublePointWithCP56Time2a : InformationObject
	{
		private DoublePointValue value;

		public DoublePointValue Value {
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

		private CP56Time2a timestamp;

		public CP56Time2a Timestamp {
			get {
				return this.timestamp;
			}
		}

		public DoublePointWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse DIQ (double point information with qualitiy) */
			byte siq = msg [startIndex++];

			value = (DoublePointValue)(siq & 0x03);

			quality = new QualityDescriptor ((byte) (siq & 0xf0));

			/* parse CP24Time2a (time stamp) */
			timestamp = new CP56Time2a (msg, startIndex);
		}
	}

}

