using System;

using lib60870;
using System.Net;
using System.Net.Sockets;
using System.Threading;

namespace testserver
{


	
	class MainClass
	{
	
		private static bool interrogationHandler(object parameter, ServerConnection connection, ASDU asdu)
		{
			Console.WriteLine ("Interrogation!");

			connection.SendACT_CON (asdu, false);

			// send information objects
			ASDU newAsdu = new ASDU(TypeID.M_ME_NB_1, CauseOfTransmission.INTERROGATED_BY_STATION, false, false, 2, 1, false);

			newAsdu.AddInformationObject (new MeasuredValueScaled (100, -1, new QualityDescriptor ()));

			newAsdu.AddInformationObject (new MeasuredValueScaled (101, 23, new QualityDescriptor ()));

			newAsdu.AddInformationObject (new MeasuredValueScaled (102, 2300, new QualityDescriptor ()));

			connection.SendASDU (newAsdu);


			connection.SendACT_TERM (asdu);

			return true;
		}

		private static bool asduHandler(object parameter, ServerConnection connection, ASDU asdu)
		{


			if (asdu.TypeId == TypeID.C_SC_NA_1) {
				Console.WriteLine ("Single command");

				SingleCommand sc = (SingleCommand)asdu.GetElement (0);

				Console.WriteLine (sc.ToString ());
			} else {
				Console.WriteLine ("unknown ASDU");
			}

			return true;
		}

		public static void Main (string[] args)
		{

			Server server = new Server ();

			server.SetInterrogationHandler (interrogationHandler, null);

			server.SetASDUHandler (asduHandler, null);

			server.Start ();

			Thread.Sleep (10000);

			Console.WriteLine ("Stop server");
			server.Stop ();
		}
	}
}
