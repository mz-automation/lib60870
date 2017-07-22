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
using System.Text;

namespace lib60870
{

	public class StatusAndStatusChangeDetection {

		public UInt16 STn {
			get {
				return (ushort) (encodedValue[0] + (256 * encodedValue[1]));
			}

            set
            {
                encodedValue[0] = (byte)(value % 256);
                encodedValue[1] = (byte)(value / 256);
            }
		}

		public UInt16 CDn {
			get {
				return (ushort) (encodedValue[2] + (256 * encodedValue[3]));
			}

            set
            {
                encodedValue[2] = (byte)(value % 256);
                encodedValue[3] = (byte)(value / 256);
            }
		}

		public bool ST(int i) {
			if ((i >= 0) && (i < 16))
				return ((int) (STn & (1 << i)) != 0);
			else
				return false;
		}

        public void ST(int i, bool value)
        {
            if ((i >= 0) && (i < 16))
            {
                if (value)
                    STn = (UInt16)(STn | (1 << i));
                else
                    STn = (UInt16)(STn & ~(1 << i));
            }
        }

        public bool CD(int i) {
			if ((i >= 0) && (i < 16))
				return ((int) (CDn & (1 << i)) != 0);
			else
				return false;
		}

        public void CD(int i, bool value)
        {
            if ((i >= 0) && (i < 16))
            {
                if (value)
					CDn = (UInt16)(CDn | (1 << i));
                else
					CDn = (UInt16)(CDn & ~(1 << i));
            }
        }

		public StatusAndStatusChangeDetection ()
		{
		}

		public StatusAndStatusChangeDetection (byte[] msg, int startIndex)
		{
			if (msg.Length < startIndex + 4)
				throw new ASDUParsingException ("Message too small for parsing StatusAndStatusChangeDetection");

			for (int i = 0; i < 4; i++)
				encodedValue [i] = msg [startIndex + i];
		}

		private byte[] encodedValue = new byte[4];

		public byte[] GetEncodedValue() 
		{
			return encodedValue;
		}

        public override string ToString()
        {
            StringBuilder sb = new StringBuilder(50);

            sb.Append("ST:");

            for (int i = 0; i < 16; i++)
                sb.Append(ST(i) ? "1" : "0");

            sb.Append(" CD:");

            for (int i = 0; i < 16; i++)
                sb.Append(CD(i) ? "1" : "0");

            return sb.ToString();
        }
    }
}
