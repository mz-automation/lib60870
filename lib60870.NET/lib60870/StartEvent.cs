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
using System.Collections.Generic;
using System.Text;

namespace lib60870
{
	/// <summary>
	/// SPE - Start events of protection equipment
	/// according to IEC 60870-5-101:2003 7.2.6.11
	/// </summary>
	public class StartEvent 
	{
		private byte encodedValue;

		public StartEvent()
		{
			this.encodedValue = 0;
		}

		public StartEvent (byte encodedValue)
		{
			this.encodedValue = encodedValue;
		}

		/// <summary>
		/// General start of operation
		/// </summary>
		/// <value><c>true</c> if started; otherwise, <c>false</c>.</value>
		public bool GS {
			get {
				if ((encodedValue & 0x01) != 0)
					return true;
				else
					return false;
			}

			set {
				if (value) 
					encodedValue |= 0x01;
				else
					encodedValue &= 0xfe;
			}
		}

		/// <summary>
		/// Start of operation phase L1
		/// </summary>
		/// <value><c>true</c> if started; otherwise, <c>false</c>.</value>
		public bool SL1 {
			get {
				if ((encodedValue & 0x02) != 0)
					return true;
				else
					return false;
			}

			set {
				if (value) 
					encodedValue |= 0x02;
				else
					encodedValue &= 0xfd;
			}
		}

		/// <summary>
		/// Start of operation phase L2
		/// </summary>
		/// <value><c>true</c> if started; otherwise, <c>false</c>.</value>
		public bool SL2 {
			get {
				if ((encodedValue & 0x04) != 0)
					return true;
				else
					return false;
			}

			set {
				if (value) 
					encodedValue |= 0x04;
				else
					encodedValue &= 0xfb;
			}
		}

		/// <summary>
		/// Start of operation phase L3
		/// </summary>
		/// <value><c>true</c> if started; otherwise, <c>false</c>.</value>
		public bool SL3 {
			get {
				if ((encodedValue & 0x08) != 0)
					return true;
				else
					return false;
			}

			set {
				if (value) 
					encodedValue |= 0x08;
				else
					encodedValue &= 0xf7;
			}
		}

		/// <summary>
		/// Start of operation IE (earth current)
		/// </summary>
		/// <value><c>true</c> if started; otherwise, <c>false</c>.</value>
		public bool SIE {
			get {
				if ((encodedValue & 0x10) != 0)
					return true;
				else
					return false;
			}

			set {
				if (value) 
					encodedValue |= 0x10;
				else
					encodedValue &= 0xef;
			}
		}

		/// <summary>
		/// Start of operation in reverse direction
		/// </summary>
		/// <value><c>true</c> if started; otherwise, <c>false</c>.</value>
		public bool SRD {
			get {
				if ((encodedValue & 0x20) != 0)
					return true;
				else
					return false;
			}

			set {
				if (value) 
					encodedValue |= 0x20;
				else
					encodedValue &= 0xdf;
			}
		}

		public bool RES1 {
			get {
				if ((encodedValue & 0x40) != 0)
					return true;
				else
					return false;
			}

			set {
				if (value) 
					encodedValue |= 0x40;
				else
					encodedValue &= 0xbf;
			}
		}

		public bool RES2 {
			get {
				if ((encodedValue & 0x80) != 0)
					return true;
				else
					return false;
			}

			set {
				if (value) 
					encodedValue |= 0x80;
				else
					encodedValue &= 0x7f;
			}
		}

		public byte EncodedValue {
			get {
				return this.encodedValue;
			}
			set {
				encodedValue = value;
			}
		}

        public override string ToString()
        {
            StringBuilder sb = new StringBuilder(30);

            if (GS)
                sb.Append("[GS]");
            if (SL1)
                sb.Append("[SL1]");
            if (SL2)
                sb.Append("[SL2]");
            if (SL3)
                sb.Append("[SL3]");
            if (SIE)
                sb.Append("[SIE]");
            if (SRD)
                sb.Append("[SRD]");
            if (RES1)
                sb.Append("[RES1]");
            if (RES2)
                sb.Append("[RES2]");

            return sb.ToString();
        }
    }
	
}
