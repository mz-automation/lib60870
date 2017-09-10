using System;
using System.Security.Cryptography.X509Certificates;
using System.Net.Security;
using System.Net;
using System.Security.Authentication;
using lib60870;
using System.Threading;



namespace tlsserver
{
	class MainClass
	{

		private static bool interrogationHandler(object parameter, ServerConnection connection, ASDU asdu, byte qoi)
		{
			Console.WriteLine ("Interrogation for group " + qoi);

			ConnectionParameters cp = connection.GetConnectionParameters ();

			try {

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
			}
			catch (ConnectionException) {
				Console.WriteLine ("Client exception closed unexpectedly");
			}

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
				
			// Own certificate has to be a pfx file that contains the private key
			X509Certificate2 ownCertificate = new X509Certificate2 ("server.pfx");

			// Create a new security information object to configure TLS
			TlsSecurityInformation secInfo = new TlsSecurityInformation (ownCertificate);

			// Add allowed client certificates - not required when AllowOnlySpecificCertificates == false
			secInfo.AddAllowedCertificate (new X509Certificate2 ("client1.cer"));
			secInfo.AddAllowedCertificate (new X509Certificate2 ("client2.cer"));

			// Add a CA certificate to check the certificate provided by the server - not required when ChainValidation == false
			secInfo.AddCA (new X509Certificate2 ("root.cer"));

			// Check if the certificate is signed by a provided CA
			secInfo.ChainValidation = true;

			// Check that the shown client certificate is in the list of allowed certificates
			secInfo.AllowOnlySpecificCertificates = true;

			// Use constructor with security information object, this will force the connection using TLS (using TCP port 19998)
			Server server = new Server (secInfo);

			server.DebugOutput = true;

			server.MaxQueueSize = 10;

			server.SetInterrogationHandler (interrogationHandler, null);

			server.SetASDUHandler (asduHandler, null);

			server.Start ();

			ASDU newAsdu = new ASDU(server.GetConnectionParameters(), CauseOfTransmission.INITIALIZED, false, false, 0, 1, false);
			EndOfInitialization eoi = new EndOfInitialization(0);
			newAsdu.AddInformationObject(eoi);
			server.EnqueueASDU(newAsdu);

			int waitTime = 1000;

			while (running) {
				Thread.Sleep(100);

				if (waitTime > 0)
					waitTime -= 100;
				else {

					newAsdu = new ASDU (server.GetConnectionParameters(), CauseOfTransmission.PERIODIC, false, false, 2, 1, false);

					newAsdu.AddInformationObject (new MeasuredValueScaled (110, -1, new QualityDescriptor ()));

					server.EnqueueASDU (newAsdu);

					waitTime = 1000;
				}
			}

			Console.WriteLine ("Stop server");
			server.Stop ();;
		}
	}
}
