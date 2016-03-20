using System;

namespace lib60870
{

	/// <summary>
	/// Output circuit information of protection equipment
	/// According to IEC 60870-5-101:2003 7.2.6.12
	/// </summary>
	public class OutputCircuitInfo {

		public byte encodedValue;

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
	}

}
