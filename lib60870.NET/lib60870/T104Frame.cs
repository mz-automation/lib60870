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

using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;

namespace lib60870
{

	public class T104Frame : Frame
	{
		byte[] buffer;

		int msgSize;
	
		public T104Frame () {
			buffer = new byte[256];

			buffer [0] = 0x68;

			msgSize = 6;
		}

		public override void PrepareToSend(int sendCounter, int receiveCounter) {
			/* set size field */
			buffer [1] = (byte)(msgSize - 2);

			buffer [2] = (byte) ((sendCounter % 128) * 2);
			buffer [3] = (byte) (sendCounter / 128);

			buffer [4] = (byte) ((receiveCounter % 128) * 2);
			buffer [5] = (byte) (receiveCounter / 128);
		}

		public override void ResetFrame() {
			msgSize = 6;
		}

		public override void SetNextByte(byte value) {
			buffer [msgSize++] = value;
		}

		public override void AppendBytes (byte[] bytes) {
			for (int i = 0; i < bytes.Length; i++) {
				buffer [msgSize++] = bytes [i];
			}
		}

		public override int GetMsgSize() {
			return msgSize;
		}

		public override byte[] GetBuffer() {
			return buffer;
		}
	}
	
}
