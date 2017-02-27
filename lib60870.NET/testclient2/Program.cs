/// <summary>
/// This example client is intended to test the server to work correctly under heavy load
/// </summary>

using System;
using System.Threading;
using lib60870;


namespace testclient2
{
	class MainClass
	{
		static int interrogationTerminationReceived = 0;
		static int interrogationConfirmationReceived = 0;

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
				if (asdu.Cot == CauseOfTransmission.ACTIVATION_CON) {
					Console.WriteLine ((asdu.IsNegative ? "Negative" : "Positive") + "confirmation for interrogation command");
					interrogationConfirmationReceived++;
				}	else if (asdu.Cot == CauseOfTransmission.ACTIVATION_TERMINATION) {
					Console.WriteLine ("Interrogation command terminated");
					interrogationTerminationReceived++;
				}
			}
			else {
				Console.WriteLine ("Unknown message type!");
			}

			Console.WriteLine ("interrogationConfirmationReceived: " + interrogationConfirmationReceived);
			Console.WriteLine ("interrogationTerminationReceived:  " + interrogationTerminationReceived);

			return true;
		}

		public static void Main (string[] args)
		{
			Console.WriteLine ("Using lib60870.NET version " + LibraryCommon.GetLibraryVersionString ());

			Connection con = new Connection ("127.0.0.1");

			con.DebugOutput = true;
			con.UseSendMessageQueue = false;

			con.SetASDUReceivedHandler (asduReceivedHandler, null);
			con.SetConnectionHandler (ConnectionHandler, null);

			con.Connect ();

			for (int i = 0; i < 5000; i++) {
				Console.WriteLine ("Send GI " + i);

				if (con.IsTransmitBufferFull()) {
					while (con.IsTransmitBufferFull() && (con.IsRunning == true))
						Thread.Sleep (50);
				
				}
				con.SendInterrogationCommand (CauseOfTransmission.ACTIVATION, 1, QualifierOfInterrogation.STATION);
			}

			while (interrogationConfirmationReceived < 4999)
				Thread.Sleep (100);

			Console.WriteLine ("interrogationTerminationReceived: " + interrogationTerminationReceived);

			con.Close ();

		}
	}
}
