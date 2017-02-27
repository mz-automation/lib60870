using System;
using System.Threading;

using lib60870;

namespace testclient
{
	class MainClass
	{

		private static void ConnectionHandler (object parameter, ConnectionEvent connectionEvent)
		{
			switch (connectionEvent) {
			case ConnectionEvent.OPENED:
				Console.WriteLine ("Connected");
				break;
			case ConnectionEvent.CLOSED:
				Console.WriteLine ("Connection closed");
				break;
			case ConnectionEvent.STARTDT_CON_RECEIVED:
				Console.WriteLine ("STARTDT CON received");
				break;
			case ConnectionEvent.STOPDT_CON_RECEIVED:
				Console.WriteLine ("STOPDT CON received");
				break;
			}
		}

		private static bool asduReceivedHandler(object parameter, ASDU asdu)
		{
			Console.WriteLine (asdu.ToString ());

			if (asdu.TypeId == TypeID.M_SP_NA_1) {

				for (int i = 0; i < asdu.NumberOfElements; i++) {

					var val = (SinglePointInformation)asdu.GetElement (i);

					Console.WriteLine ("  IOA: " + val.ObjectAddress + " SP value: " + val.Value);
					Console.WriteLine ("   " + val.Quality.ToString ());
				}
			} else if (asdu.TypeId == TypeID.M_ME_TE_1) {
			
				for (int i = 0; i < asdu.NumberOfElements; i++) {

					var msv = (MeasuredValueScaledWithCP56Time2a)asdu.GetElement (i);

					Console.WriteLine ("  IOA: " + msv.ObjectAddress + " scaled value: " + msv.ScaledValue);
					Console.WriteLine ("   " + msv.Quality.ToString ());
					Console.WriteLine ("   " + msv.Timestamp.ToString ());
				}

			} else if (asdu.TypeId == TypeID.M_ME_TF_1) {

				for (int i = 0; i < asdu.NumberOfElements; i++) {
					var mfv = (MeasuredValueShortWithCP56Time2a)asdu.GetElement (i);

					Console.WriteLine ("  IOA: " + mfv.ObjectAddress + " float value: " + mfv.Value);
					Console.WriteLine ("   " + mfv.Quality.ToString ());
					Console.WriteLine ("   " + mfv.Timestamp.ToString ());
					Console.WriteLine ("   " + mfv.Timestamp.GetDateTime ().ToString ());
				}
			} else if (asdu.TypeId == TypeID.M_SP_TB_1) {

				for (int i = 0; i < asdu.NumberOfElements; i++) {

					var val = (SinglePointWithCP56Time2a)asdu.GetElement (i);

					Console.WriteLine ("  IOA: " + val.ObjectAddress + " SP value: " + val.Value);
					Console.WriteLine ("   " + val.Quality.ToString ());
					Console.WriteLine ("   " + val.Timestamp.ToString ());
				}
			} else if (asdu.TypeId == TypeID.M_ME_NC_1) {

				for (int i = 0; i < asdu.NumberOfElements; i++) {
					var mfv = (MeasuredValueShort)asdu.GetElement (i);

					Console.WriteLine ("  IOA: " + mfv.ObjectAddress + " float value: " + mfv.Value);
					Console.WriteLine ("   " + mfv.Quality.ToString ());
				}
			} else if (asdu.TypeId == TypeID.M_ME_NB_1) {

				for (int i = 0; i < asdu.NumberOfElements; i++) {

					var msv = (MeasuredValueScaled)asdu.GetElement (i);

					Console.WriteLine ("  IOA: " + msv.ObjectAddress + " scaled value: " + msv.ScaledValue);
					Console.WriteLine ("   " + msv.Quality.ToString ());
				}

			} else if (asdu.TypeId == TypeID.M_ME_ND_1) {

				for (int i = 0; i < asdu.NumberOfElements; i++) {

					var msv = (MeasuredValueNormalizedWithoutQuality)asdu.GetElement (i);

					Console.WriteLine ("  IOA: " + msv.ObjectAddress + " scaled value: " + msv.NormalizedValue);
				}

			} else if (asdu.TypeId == TypeID.C_IC_NA_1) {
				if (asdu.Cot == CauseOfTransmission.ACTIVATION_CON)
					Console.WriteLine ((asdu.IsNegative ? "Negative" : "Positive") + "confirmation for interrogation command");
				else if (asdu.Cot == CauseOfTransmission.ACTIVATION_TERMINATION)
					Console.WriteLine ("Interrogation command terminated");
			}
			else {
				Console.WriteLine ("Unknown message type!");
			}

			return true;
		}

		public static void Main (string[] args)
		{
			Console.WriteLine ("Using lib60870.NET version " + LibraryCommon.GetLibraryVersionString ());

			//Connection con = new Connection ("192.168.1.50");
			Connection con = new Connection ("127.0.0.1");

			con.DebugOutput = true;

			con.SetASDUReceivedHandler (asduReceivedHandler, null);
			con.SetConnectionHandler (ConnectionHandler, null);

			con.Connect ();

			Thread.Sleep (5000);

			con.SendTestCommand (1);

			con.SendInterrogationCommand (CauseOfTransmission.ACTIVATION, 1, QualifierOfInterrogation.STATION);

			Thread.Sleep (5000);

			con.SendControlCommand (CauseOfTransmission.ACTIVATION, 1, new SingleCommand (5000, true, false, 0));

			con.SendControlCommand (CauseOfTransmission.ACTIVATION, 1, new DoubleCommand (5001, DoubleCommand.ON, false, 0));

			con.SendControlCommand (CauseOfTransmission.ACTIVATION, 1, new StepCommand (5002, StepCommandValue.HIGHER, false, 0));

			con.SendControlCommand (CauseOfTransmission.ACTIVATION, 1, 
			                        new SingleCommandWithCP56Time2a (5000, false, false, 0, new CP56Time2a (DateTime.Now)));

			/* Synchronize clock of the controlled station */
			con.SendClockSyncCommand (1 /* CA */, new CP56Time2a (DateTime.Now)); 


			Console.WriteLine ("CLOSE");

			con.Close ();

			Console.WriteLine ("RECONNECT");

			con.Connect ();

			Thread.Sleep (5000);

			con.Close ();
		}
	}
}
