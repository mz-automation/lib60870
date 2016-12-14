using System;

namespace lib60870
{
	public class LibraryCommon
	{
		public const int VERSION_MAJOR = 0;
		public const int VERSION_MINOR = 9;
		public const int VERSION_PATCH = 3;

		public static string GetLibraryVersionString()
		{
			return "" + VERSION_MAJOR + "." + VERSION_MINOR + "." + VERSION_PATCH;
		}
	}
}

