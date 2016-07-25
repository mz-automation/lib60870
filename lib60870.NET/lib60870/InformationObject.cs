/*
 *  InformationObject.cs
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
	public abstract class InformationObject
	{
		private int objectAddress;

		public InformationObject (ConnectionParameters parameters, byte[] msg, int startIndex)
		{
			/* parse information object address */
			objectAddress = msg [startIndex];

			if (parameters.SizeOfIOA > 1)
				objectAddress += (msg [startIndex + 1] * 0x100);

			if (parameters.SizeOfIOA > 2)
				objectAddress += (msg [startIndex + 2] * 0x10000);
		}

		public InformationObject(int objectAddress) {
			this.objectAddress = objectAddress;
		}

		public int ObjectAddress {
			get {
				return this.objectAddress;
			}
		}

		public virtual void Encode(Frame frame, ConnectionParameters parameters) {
			frame.SetNextByte((byte)(objectAddress & 0xff));

			if (parameters.SizeOfIOA > 1)
            	frame.SetNextByte((byte)((objectAddress / 0x100) & 0xff));

			if (parameters.SizeOfIOA > 2)
              	frame.SetNextByte((byte)((objectAddress / 0x10000) & 0xff));
		}


	}
}

