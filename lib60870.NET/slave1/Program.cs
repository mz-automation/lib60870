using System;
using lib60870;
using System.Threading;

namespace slave1
{
	class MainClass
	{
		static SinglePointInformation[] spiObjects = new SinglePointInformation[400];
		static StepPositionWithCP56Time2a[] stepPositionObjects = new StepPositionWithCP56Time2a[100];

		public static void Main (string[] args)
		{
			/* Initialize data objects */
			for (int i = 0; i < 400; i++)
				spiObjects [i] = new SinglePointInformation (1000 + i, true, new QualityDescriptor ());

			for (int i = 0; i < 100; i++)
				stepPositionObjects [i] = new StepPositionWithCP56Time2a (10000 + i, 0, false, 
					new QualityDescriptor (), new CP56Time2a ());


			bool running = true;

			Console.CancelKeyPress += delegate(object sender, ConsoleCancelEventArgs e) {
				e.Cancel = true;
				running = false;
			};

			Server server = new Server ();

			server.DebugOutput = true;
			server.MaxQueueSize = 100;

			//server.SetInterrogationHandler (interrogationHandler, null);


			server.Start ();

			int waitTime = 2000;

			while (running) {
				Thread.Sleep(100);

				if (waitTime > 0)
					waitTime -= 100;
				else {

					ASDU newAsdu = null; 

					/* send SPI objects */
					for (int i = 0; i < 400; i++) {
						spiObjects [i].Value = !(spiObjects [i].Value);

						if (newAsdu == null)
							newAsdu = new ASDU (server.GetConnectionParameters(), CauseOfTransmission.PERIODIC, false, false, 1, 1, false);

						if (newAsdu.AddInformationObject (spiObjects [i]) == false) {
							server.EnqueueASDU (newAsdu);
							newAsdu = null;
							i--;
						}
					}

					if (newAsdu != null)
						server.EnqueueASDU (newAsdu);

					/* send step position objects */
					newAsdu = null;

					for (int i = 0; i < 100; i++) {

						stepPositionObjects [i].Value = (stepPositionObjects [i].Value + 1) % 63;

						if (newAsdu == null)
							newAsdu = new ASDU (server.GetConnectionParameters (), CauseOfTransmission.PERIODIC, false, false, 1, 1, false);

						if (newAsdu.AddInformationObject (stepPositionObjects [i]) == false) {
							server.EnqueueASDU (newAsdu);
							newAsdu = null;
							i--;
						}
					}

					if (newAsdu != null)
						server.EnqueueASDU (newAsdu);

					waitTime = 2000;
				}
			}

			Console.WriteLine ("Stop server");
			server.Stop ();
		}
	}
}
