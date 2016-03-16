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

			if (parameters.SizeOfIOA > 1)
              	frame.SetNextByte((byte)((objectAddress / 0x10000) & 0xff));
		}


	}
}

