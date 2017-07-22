/*
 *  OutputCircuitInfo.cs
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
using System.Text;

namespace lib60870
{

	/// <summary>
	/// Output circuit information of protection equipment
	/// According to IEC 60870-5-101:2003 7.2.6.12
	/// </summary>
	public class OutputCircuitInfo {

		internal byte encodedValue;

		public byte EncodedValue {
			get {
				return this.encodedValue;
			}
			set {
				encodedValue = value;
			}
		}

		public OutputCircuitInfo () 
		{
			this.encodedValue = 0;
		}

		public OutputCircuitInfo (byte encodedValue)
		{
			this.encodedValue = encodedValue;
		}

		public OutputCircuitInfo(bool gc, bool cl1, bool cl2, bool cl3)
		{
			GC = gc;
			CL1 = cl1;
			CL2 = cl2;
			CL3 = cl3;
		}

		/// <summary>
		/// General command to output circuit
		/// </summary>
		/// <value><c>true</c> if set, otherwise, <c>false</c>.</value>
		public bool GC {
			get {
				return ((encodedValue & 0x01) != 0);
			}

			set {
				if (value) 
					encodedValue |= 0x01;
				else
					encodedValue &= 0xfe;
			}
		}

		/// <summary>
		/// Command to output circuit phase L1
		/// </summary>
		/// <value><c>true</c> if set, otherwise, <c>false</c>.</value>
		public bool CL1 {
			get {
				return ((encodedValue & 0x02) != 0);
			}

			set {
				if (value) 
					encodedValue |= 0x02;
				else
					encodedValue &= 0xfd;
			}
		}

		/// <summary>
		/// Command to output circuit phase L2
		/// </summary>
		/// <value><c>true</c> if set, otherwise, <c>false</c>.</value>
		public bool CL2 {
			get {
				return ((encodedValue & 0x04) != 0);
			}

			set {
				if (value) 
					encodedValue |= 0x04;
				else
					encodedValue &= 0xfb;
			}
		}

		/// <summary>
		/// Command to output circuit phase L3
		/// </summary>
		/// <value><c>true</c> if set, otherwise, <c>false</c>.</value>
		public bool CL3 {
			get {
				return ((encodedValue & 0x08) != 0);
			}

			set {
				if (value) 
					encodedValue |= 0x08;
				else
					encodedValue &= 0xf7;
			}
		}

        public override string ToString()
        {
            StringBuilder sb = new StringBuilder(20);

            if (GC)
                sb.Append("[GC]");
            if (CL1)
                sb.Append("[CL1]");
            if (CL2)
                sb.Append("[CL2]");
            if (CL3)
                sb.Append("[CL3]");

            return sb.ToString();
        }
    }

}
