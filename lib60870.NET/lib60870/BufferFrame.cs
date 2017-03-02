/*
 *  Copyright 2017 MZ Automation GmbH
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
	internal class BufferFrame : Frame {

		private byte[] buffer;
		private int startPos;
		private int bufPos;

		public BufferFrame(byte[] buffer, int startPos) {
			this.buffer = buffer;
			this.startPos = startPos;
			this.bufPos = startPos;
		}

		public override void ResetFrame ()
		{
			bufPos = startPos;
		}

		public override void SetNextByte (byte value)
		{
			buffer [bufPos++] = value;
		}

		public override void AppendBytes (byte[] bytes)
		{
			for (int i = 0; i < bytes.Length; i++)
				buffer [bufPos++] = bytes [i];
		}

		public override int GetMsgSize () {
			return bufPos;
		}

		public override byte[] GetBuffer ()
		{
			return buffer;
		}
	}
}

