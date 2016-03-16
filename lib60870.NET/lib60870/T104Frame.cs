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
