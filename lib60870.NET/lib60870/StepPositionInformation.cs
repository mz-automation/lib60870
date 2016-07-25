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
		private int value;

		/// <summary>
		/// Step position (range -64 ... +63)
		/// </summary>
		/// <value>The value.</value>
		public int Value {
			get {
				return this.value;
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
		}

		private QualityDescriptor quality;

		public QualityDescriptor Quality {
			get {
				return this.quality;
			}
		}

		public StepPositionInformation (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse VTI (value with transient state indication) */
			byte vti = msg [startIndex++];

			isTransient = ((vti & 0x80) == 0x80);

			value = (vti & 0x7f);

			if (value > 63)
				value = value - 128;

			quality = new QualityDescriptor (msg[startIndex++]);
		}


	}

	public class StepPositionWithCP24Time2a : InformationObject
	{
		private int value;

		public int Value {
			get {
				return this.value;
			}
		}

		private bool isTransient;

		public bool Transient {
			get {
				return this.isTransient;
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


		public StepPositionWithCP24Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse VTI (value with transient state indication) */
			byte vti = msg [startIndex++];

			isTransient = ((vti & 0x80) == 0x80);

			value = (vti & 0x7f);

			if (value > 63)
				value = value - 128;

			quality = new QualityDescriptor (msg[startIndex++]);

			/* parse CP24Time2a (time stamp) */
			timestamp = new CP24Time2a (msg, startIndex);
		}

	}

	public class StepPositionWithCP56Time2a : InformationObject
	{
		private int value;

		public int Value {
			get {
				return this.value;
			}
		}

		private bool isTransient;

		public bool Transient {
			get {
				return this.isTransient;
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


		public StepPositionWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			/* parse VTI (value with transient state indication) */
			byte vti = msg [startIndex++];

			isTransient = ((vti & 0x80) == 0x80);

			value = (vti & 0x7f);

			if (value > 63)
				value = value - 128;

			quality = new QualityDescriptor (msg[startIndex++]);

			/* parse CP24Time2a (time stamp) */
			timestamp = new CP56Time2a (msg, startIndex);
		}

	}
	
}

