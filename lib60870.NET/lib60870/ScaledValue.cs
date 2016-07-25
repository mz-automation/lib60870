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
	public class ScaledValue {
		private byte[] encodedValue = new byte[2];

		public ScaledValue (byte[] msg, int startIndex)
		{
			if (msg.Length < startIndex + 2)
				throw new ASDUParsingException ("Message too small for parsing ScaledValue");

			for (int i = 0; i < 2; i++)
				encodedValue [i] = msg [startIndex + i];
		}

		public ScaledValue(int value)
		{
			this.Value = value;
		}

		public byte[] GetEncodedValue() 
		{
			return encodedValue;
		}

		public int Value {
			get {
				int value;

				value = encodedValue [0];
				value += (encodedValue [1] * 0x100);

				if (value > 32767)
					value = value - 65536;

				return value;
			}
			set {
				int valueToEncode;

				if (value < 0)
					valueToEncode = value + 65536;
				else
					valueToEncode = value;

				encodedValue[0] = (byte)(valueToEncode % 256);
				encodedValue[1] = (byte)(valueToEncode / 256);
			}
		}

	}
	
}
