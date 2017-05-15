using System;

namespace lib60870
{
	public class LibraryCommon
	{
		public const int VERSION_MAJOR = 0;
		public const int VERSION_MINOR = 9;
		public const int VERSION_PATCH = 5;

		public static string GetLibraryVersionString()
		{
			return "" + VERSION_MAJOR + "." + VERSION_MINOR + "." + VERSION_PATCH;
		}
	}

	/// <summary>
	/// Provides some Connection statistics.
	/// </summary>
	public class ConnectionStatistics {

		private int sentMsgCounter = 0;
		private int rcvdMsgCounter = 0;
		private int rcvdTestFrActCounter = 0;
		private int rcvdTestFrConCounter = 0;

		internal void Reset () 
		{
			sentMsgCounter = 0;
			rcvdMsgCounter = 0;
			rcvdTestFrActCounter = 0;
			rcvdTestFrConCounter = 0;
		}

		public int SentMsgCounter {
			get {
				return this.sentMsgCounter;
			}
			internal set {
				this.sentMsgCounter = value;
			}
		}

		public int RcvdMsgCounter {
			get {
				return this.rcvdMsgCounter;
			}
			internal set {
				this.rcvdMsgCounter = value;
			}
		}

		/// <summary>
		/// Counter for the TEST_FR_ACT messages received.
		/// </summary>
		/// <value>The TEST_FR_ACT counter.</value>
		public int RcvdTestFrActCounter {
			get {
				return this.rcvdTestFrActCounter;
			}
			internal set {
				this.rcvdTestFrActCounter = value;
			}
		}

		/// <summary>
		/// Counter for the TEST_FR_CON messages received.
		/// </summary>
		/// <value>The TEST_FR_CON counter.</value>
		public int RcvdTestFrConCounter {
			get {
				return this.rcvdTestFrConCounter;
			}
			internal set {
				this.rcvdTestFrConCounter = value;
			}
		}
			
	}
}

