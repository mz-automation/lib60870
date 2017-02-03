using System;

using lib60870;

namespace testlib
{
	class MainClass
	{
		static void testCP56Time2A() 
		{
			CP56Time2a myTime = new CP56Time2a (DateTime.Now);

			Console.WriteLine (myTime.ToString ());

			myTime.Invalid = true;

			Console.WriteLine (myTime.ToString ());

			myTime.Invalid = false;
			myTime.Substituted = true;

			Console.WriteLine (myTime.ToString ());

			myTime.SummerTime = true;
			myTime.Substituted = false;

			Console.WriteLine (myTime.ToString ());
		}

		public static void Main (string[] args)
		{
			//testCP56Time2A


		}
	}
}
