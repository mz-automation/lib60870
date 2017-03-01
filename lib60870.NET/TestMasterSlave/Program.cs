using System;
using lib60870;
using System.Threading;

namespace TestMasterSlave
{
	class MainClass
	{
		private static bool interrogationHandler(object parameter, ServerConnection connection, ASDU asdu, byte qoi)
		{
			Console.WriteLine ("Interrogation for group " + qoi);

			ConnectionParameters cp = connection.GetConnectionParameters ();

			connection.SendACT_CON (asdu, false);

			// send information objects
			ASDU newAsdu = new ASDU(cp, CauseOfTransmission.INTERROGATED_BY_STATION, false, false, 2, 1, false);

			newAsdu.AddInformationObject (new MeasuredValueScaled (100, -1, new QualityDescriptor ()));

			newAsdu.AddInformationObject (new MeasuredValueScaled (101, 23, new QualityDescriptor ()));

			newAsdu.AddInformationObject (new MeasuredValueScaled (102, 2300, new QualityDescriptor ()));

			connection.SendASDU (newAsdu);

			newAsdu = new ASDU (cp, CauseOfTransmission.INTERROGATED_BY_STATION, false, false, 3, 1, false);

			newAsdu.AddInformationObject(new MeasuredValueScaledWithCP56Time2a(103, 3456, new QualityDescriptor (), new CP56Time2a(DateTime.Now)));

			connection.SendASDU (newAsdu);

			newAsdu = new ASDU (cp, CauseOfTransmission.INTERROGATED_BY_STATION, false, false, 2, 1, false);

			newAsdu.AddInformationObject (new SinglePointWithCP56Time2a (104, true, new QualityDescriptor (), new CP56Time2a (DateTime.Now)));

			connection.SendASDU (newAsdu);

			// send sequence of information objects
			newAsdu = new ASDU (cp, CauseOfTransmission.INTERROGATED_BY_STATION, false, false, 2, 1, true);

			newAsdu.AddInformationObject (new SinglePointInformation (200, true, new QualityDescriptor ()));
			newAsdu.AddInformationObject (new SinglePointInformation (201, false, new QualityDescriptor ()));
			newAsdu.AddInformationObject (new SinglePointInformation (202, true, new QualityDescriptor ()));
			newAsdu.AddInformationObject (new SinglePointInformation (203, false, new QualityDescriptor ()));
			newAsdu.AddInformationObject (new SinglePointInformation (204, true, new QualityDescriptor ()));
			newAsdu.AddInformationObject (new SinglePointInformation (205, false, new QualityDescriptor ()));
			newAsdu.AddInformationObject (new SinglePointInformation (206, true, new QualityDescriptor ()));
			newAsdu.AddInformationObject (new SinglePointInformation (207, false, new QualityDescriptor ()));

			connection.SendASDU (newAsdu);

			newAsdu = new ASDU (cp, CauseOfTransmission.INTERROGATED_BY_STATION, false, false, 2, 1, true);

			newAsdu.AddInformationObject (new MeasuredValueNormalizedWithoutQuality (300, -1.0f));
			newAsdu.AddInformationObject (new MeasuredValueNormalizedWithoutQuality (301, -0.5f));
			newAsdu.AddInformationObject (new MeasuredValueNormalizedWithoutQuality (302, -0.1f));
			newAsdu.AddInformationObject (new MeasuredValueNormalizedWithoutQuality (303, .0f));
			newAsdu.AddInformationObject (new MeasuredValueNormalizedWithoutQuality (304, 0.1f));
			newAsdu.AddInformationObject (new MeasuredValueNormalizedWithoutQuality (305, 0.2f));
			newAsdu.AddInformationObject (new MeasuredValueNormalizedWithoutQuality (306, 0.5f));
			newAsdu.AddInformationObject (new MeasuredValueNormalizedWithoutQuality (307, 0.7f));
			newAsdu.AddInformationObject (new MeasuredValueNormalizedWithoutQuality (308, 0.99f));
			newAsdu.AddInformationObject (new MeasuredValueNormalizedWithoutQuality (309, 1f));

			connection.SendASDU (newAsdu);

			connection.SendACT_TERM (asdu);

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
			Connection con = new Connection ("127.0.0.1");

			con.DebugOutput = true;
			server.DebugOutput = true;
			server.MaxQueueSize = 10;
			server.SetInterrogationHandler (interrogationHandler, null);

			server.Start ();

			Thread.Sleep (1000);

			ASDU asdu = new ASDU (con.Parameters, CauseOfTransmission.SPONTANEOUS, false, false, 0, 1, false);

			asdu.AddInformationObject (new ParameterNormalizedValue (102, 0.1f, 0));

			con.Connect ();
			con.SendTestCommand (1);
			//con.SendInterrogationCommand (CauseOfTransmission.ACTIVATION, 1, 20);

			con.SendASDU (asdu);
			con.SendASDU (asdu);
			con.SendASDU (asdu);
			con.SendASDU (asdu);
			con.SendASDU (asdu);
			con.SendASDU (asdu);
			con.SendASDU (asdu);
			con.SendASDU (asdu);

			while (running) {
				Thread.Sleep(100);	
			}

			Console.WriteLine ("Stop server");
			con.Close ();
			server.Stop ();

			
		}
	}
}
