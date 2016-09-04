using System;

using lib60870;
using System.Net;
using System.Net.Sockets;
using System.Threading;

namespace testserver
{
	
	class MainClass
	{
	
		private static bool interrogationHandler(object parameter, ServerConnection connection, ASDU asdu, byte qoi)
		{
			Console.WriteLine ("Interrogation for group " + qoi);

			connection.SendACT_CON (asdu, false);

			// send information objects
			ASDU newAsdu = new ASDU(TypeID.M_ME_NB_1, CauseOfTransmission.INTERROGATED_BY_STATION, false, false, 2, 1, false);

			newAsdu.AddInformationObject (new MeasuredValueScaled (100, -1, new QualityDescriptor ()));

			newAsdu.AddInformationObject (new MeasuredValueScaled (101, 23, new QualityDescriptor ()));

			newAsdu.AddInformationObject (new MeasuredValueScaled (102, 2300, new QualityDescriptor ()));

			connection.SendASDU (newAsdu);

			newAsdu = new ASDU (TypeID.M_ME_TE_1, CauseOfTransmission.INTERROGATED_BY_STATION, false, false, 3, 1, false);

			newAsdu.AddInformationObject(new MeasuredValueScaledWithCP56Time2a(103, 3456, new QualityDescriptor (), new CP56Time2a(DateTime.Now)));

			connection.SendASDU (newAsdu);

			newAsdu = new ASDU (TypeID.M_SP_TB_1, CauseOfTransmission.INTERROGATED_BY_STATION, false, false, 2, 1, false);

			newAsdu.AddInformationObject (new SinglePointWithCP56Time2a (104, true, new QualityDescriptor (), new CP56Time2a (DateTime.Now)));

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
			} 
			else if (asdu.TypeId == TypeID.C_CS_NA_1){
				

				ClockSynchronizationCommand qsc = (ClockSynchronizationCommand)asdu.GetElement (0);

				Console.WriteLine ("Received clock sync command with time " + qsc.NewTime.ToString());
			}

			return true;
		}

		public static void Main (string[] args)
		{
			bool running = true;

			Console.CancelKeyPress += delegate(object sender, ConsoleCancelEventArgs e) {
				e.Cancel = true;
				running = false;
			};

			Server server = new Server ();

			if (BitConverter.IsLittleEndian)
				Console.WriteLine ("Platform is little endian");

			server.MaxQueueSize = 10;

			server.SetInterrogationHandler (interrogationHandler, null);

			server.SetASDUHandler (asduHandler, null);

			server.Start ();

			int waitTime = 1000;

			while (running) {
				Thread.Sleep(100);

				if (waitTime > 0)
					waitTime -= 100;
				else {

					ASDU newAsdu = new ASDU (TypeID.M_ME_NB_1, CauseOfTransmission.PERIODIC, false, false, 2, 1, false);

					newAsdu.AddInformationObject (new MeasuredValueScaled (110, -1, new QualityDescriptor ()));
				
					server.EnqueueASDU (newAsdu);

					waitTime = 1000;
				}
			}

			Console.WriteLine ("Stop server");
			server.Stop ();
		}
	}
}
