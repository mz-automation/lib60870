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

	public class StatusAndStatusChangeDetection {

		public UInt16 STn {
			get {
				return (ushort) (encodedValue[0] + 256 * encodedValue[1]);
			}
		}

		public UInt16 CDn {
			get {
				return (ushort) (encodedValue[2] + 256 * encodedValue[3]);
			}
		}

		public bool ST(int i) {
			if ((i >= 0) && (i < 16))
				return ((int) (STn & (2^i)) != 0);
			else
				return false;
		}

		public bool CD(int i) {
			if ((i >= 0) && (i < 16))
				return ((int) (CDn & (2^i)) != 0);
			else
				return false;
		}

		public StatusAndStatusChangeDetection (byte[] msg, int startIndex)
		{
			if (msg.Length < startIndex + 4)
				throw new ASDUParsingException ("Message too small for parsing StatusAndStatusChangeDetection");

			for (int i = 0; i < 4; i++)
				encodedValue [i] = msg [startIndex + i];
		}

		private byte[] encodedValue = new byte[0];

		public byte[] GetEncodedValue() 
		{
			return encodedValue;
		}
	}
}
