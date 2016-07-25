/*
 *  BinaryCounterReading.cs
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
	public class BinaryCounterReading
	{

		private byte[] encodedValue = new byte[5];

		public byte[] GetEncodedValue() 
		{
			return encodedValue;
		}

		public Int32 Value {
			get {
				Int32 value = encodedValue [0];
				value += (encodedValue [1] * 0x100);
				value += (encodedValue [2] * 0x10000);
				value += (encodedValue [3] * 0x1000000);

				return value;
			}
		}

		public int SequenceNumber {
			get {
				return (encodedValue [4] & 0x1f);
			}
		}

		public bool Carry {
			get {
				return ((encodedValue[4] & 0x20) == 0x20);
			}
		}

		public bool Adjusted {
			get {
				return ((encodedValue[4] & 0x40) == 0x40);
			}
		}

		public bool Invalid {
			get {
				return ((encodedValue[4] & 0x80) == 0x80);
			}
		}

		public BinaryCounterReading (byte[] msg, int startIndex)
		{
			if (msg.Length < startIndex + 5)
				throw new ASDUParsingException ("Message too small for parsing BinaryCounterReading");

			for (int i = 0; i < 5; i++)
				encodedValue [i] = msg [startIndex + i];
		}

		public BinaryCounterReading ()
		{
		}
	}
}

