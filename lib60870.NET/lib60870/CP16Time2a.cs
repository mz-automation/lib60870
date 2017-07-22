/*
 *  CP16Time2a.cs
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
	public class CP16Time2a
	{
		private byte[] encodedValue = new byte[2];

		internal CP16Time2a (byte[] msg, int startIndex)
		{
			if (msg.Length < startIndex + 3)
				throw new ASDUParsingException ("Message too small for parsing CP16Time2a");

			for (int i = 0; i < 2; i++)
				encodedValue [i] = msg [startIndex + i];
		}

		public CP16Time2a(int elapsedTimeInMs)
		{
			ElapsedTimeInMs = elapsedTimeInMs;
		}

		public int ElapsedTimeInMs {
			get {
				return (encodedValue[0] + (encodedValue[1] * 0x100));
			}

			set {
				encodedValue [0] = (byte) (value % 0x100);
				encodedValue [1] = (byte) (value / 0x100);
			}
		}

		public byte[] GetEncodedValue() 
		{
			return encodedValue;
		}

        public override string ToString()
        {
            return ElapsedTimeInMs.ToString();
        }
    }
}

