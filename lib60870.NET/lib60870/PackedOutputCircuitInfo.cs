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

	public class PackedOutputCircuitInfo : InformationObject
	{
		OutputCircuitInfo oci;

		public OutputCircuitInfo OCI {
			get {
				return this.oci;
			}
		}

		QualityDescriptorP qdp;

		public QualityDescriptorP QDP {
			get {
				return this.qdp;
			}
		}

		private CP16Time2a operatingTime;

		public CP16Time2a OperatingTime {
			get {
				return this.operatingTime;
			}
		}

		private CP24Time2a timestamp;

		public CP24Time2a Timestamp {
			get {
				return this.timestamp;
			}
		}

		public PackedOutputCircuitInfo (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			oci = new OutputCircuitInfo (msg [startIndex++]);

			qdp = new QualityDescriptorP (msg [startIndex++]);

			operatingTime = new CP16Time2a (msg, startIndex);
			startIndex += 2;

			/* parse CP56Time2a (time stamp) */
			timestamp = new CP24Time2a (msg, startIndex);
		}
	}

	public class PacketOutputCircuitInfoWithCP56Time2a : InformationObject
	{
		OutputCircuitInfo oci;

		public OutputCircuitInfo OCI {
			get {
				return this.oci;
			}
		}

		QualityDescriptorP qdp;

		public QualityDescriptorP QDP {
			get {
				return this.qdp;
			}
		}

		private CP16Time2a operatingTime;

		public CP16Time2a OperatingTime {
			get {
				return this.operatingTime;
			}
		}

		private CP56Time2a timestamp;

		public CP56Time2a Timestamp {
			get {
				return this.timestamp;
			}
		}

		public PacketOutputCircuitInfoWithCP56Time2a (ConnectionParameters parameters, byte[] msg, int startIndex) :
			base(parameters, msg, startIndex)
		{
			startIndex += parameters.SizeOfIOA; /* skip IOA */

			oci = new OutputCircuitInfo (msg [startIndex++]);

			qdp = new QualityDescriptorP (msg [startIndex++]);

			operatingTime = new CP16Time2a (msg, startIndex);
			startIndex += 2;

			/* parse CP56Time2a (time stamp) */
			timestamp = new CP56Time2a (msg, startIndex);
		}
	}
}
