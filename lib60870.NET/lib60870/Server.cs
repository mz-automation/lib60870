using System;

using lib60870;
using System.Net;
using System.Net.Sockets;
using System.Threading;
//using System.Collections;
using System.Collections.Generic;
using System.Collections.Concurrent;

namespace lib60870
{

	/// <summary>
	/// Handler for interrogation command (C_IC_NA_1 - 100).
	/// </summary>
	public delegate bool InterrogationHandler (object parameter, ServerConnection connection, ASDU asdu, byte qoi);

	/// <summary>
	/// Handler for counter interrogation command (C_CI_NA_1 - 101).
	/// </summary>
	public delegate bool CounterInterrogationHandler (object parameter, ServerConnection connection, ASDU asdu, byte qoi);

	/// <summary>
	/// Handler for read command (C_RD_NA_1 - 102)
	/// </summary>
	public delegate bool ReadHandler (object parameter, ServerConnection connection, ASDU asdu, int ioa);

	/// <summary>
	/// Handler for clock synchronization command (C_CS_NA_1 - 103)
	/// </summary>
	public delegate bool ClockSynchronizationHandler (object parameter, ServerConnection connection, ASDU asdu, CP56Time2a newTime);

	/// <summary>
	/// Handler for reset process command (C_RP_NA_1 - 105)
	/// </summary>
	public delegate bool ResetProcessHandler (object parameter, ServerConnection connection, ASDU asdu, byte  qrp);

	/// <summary>
	/// Handler for delay acquisition command (C_CD_NA:1 - 106)
	/// </summary>
	public delegate bool DelayAcquisitionHandler (object parameter, ServerConnection connection, ASDU asdu, CP16Time2a delayTime);


	/// <summary>
	/// Handler for ASDUs that are not handled by other handlers
	/// </summary>
	public delegate bool ASDUHandler (object parameter, ServerConnection connection, ASDU asdu);

	public class Server {

		private string localHostname = "127.0.0.1";

		private bool running = false;

		private Socket listeningSocket;

		private int maxQueueSize = 1000;

		public int MaxQueueSize {
			get {
				return this.maxQueueSize;
			}
			set {
				maxQueueSize = value;
			}
		}

		private ConnectionParameters parameters = null;

		// List of all open connections
		private List<ServerConnection> allOpenConnections = new List<ServerConnection>();

		// Queue for messages (ASDUs)
		private Queue<ASDU> enqueuedASDUs = null;

		public Server()
		{
			this.parameters = new ConnectionParameters ();
		}

		public Server(ConnectionParameters parameters) {
			this.parameters = parameters;
		}

		public InterrogationHandler interrogationHandler = null;
		public object InterrogationHandlerParameter = null;

		public CounterInterrogationHandler counterInterrogationHandler = null;
		public object counterInterrogationHandlerParameter = null;

		public ReadHandler readHandler = null;
		public object readHandlerParameter = null;

		public ClockSynchronizationHandler clockSynchronizationHandler = null;
		public object clockSynchronizationHandlerParameter = null;

		public ResetProcessHandler resetProcessHandler = null;
		public object resetProcessHandlerParameter = null;

		public DelayAcquisitionHandler delayAcquisitionHandler = null;
		public object delayAcquisitionHandlerParameter = null;

		public void SetInterrogationHandler(InterrogationHandler handler, object parameter)
		{
			this.interrogationHandler = handler;
			this.InterrogationHandlerParameter = parameter;
		}

		public void SetCounterInterrogationHandler(CounterInterrogationHandler handler, object parameter)
		{
			this.counterInterrogationHandler = handler;
			this.counterInterrogationHandlerParameter = parameter;
		}

		public void SetReadHandler(ReadHandler handler, object parameter)
		{
			this.readHandler = handler;
			this.readHandlerParameter = parameter;
		}

		public void SetClockSynchronizationHandler(ClockSynchronizationHandler handler, object parameter)
		{
			this.clockSynchronizationHandler = handler;
			this.clockSynchronizationHandlerParameter = parameter;
		}

		public void SetResetProcessHandler(ResetProcessHandler handler, object parameter)
		{
			this.resetProcessHandler = handler;
			this.resetProcessHandlerParameter = parameter;
		}

		public void SetDelayAcquisitionHandler(DelayAcquisitionHandler handler, object parameter)
		{
			this.delayAcquisitionHandler = handler;
			this.delayAcquisitionHandlerParameter = parameter;
		}

		public ASDUHandler asduHandler = null;
		public object asduHandlerParameter = null;

		public void SetASDUHandler(ASDUHandler handler, object parameter)
		{
			this.asduHandler = handler;
			this.asduHandlerParameter = parameter;
		}
			
		public int ActiveConnections {
			get {
				return this.allOpenConnections.Count;
			}
		}

		private void ServerAcceptThread()
		{
			running = true;

			Console.WriteLine ("Waiting for connections...");

			while (running) {

				try {
					
					Socket newSocket = listeningSocket.Accept ();

					if (newSocket != null) {
						Console.WriteLine ("Connected");

						allOpenConnections.Add(
							new ServerConnection (newSocket, parameters, this));
					}

				} catch (Exception) {
					running = false;
				}


			}
		}

		internal void Remove(ServerConnection connection)
		{
			allOpenConnections.Remove (connection);
		}

		public void Start() 
		{
			IPAddress ipAddress = IPAddress.Parse(localHostname);
			IPEndPoint remoteEP = new IPEndPoint(ipAddress, parameters.TcpPort);

			// Create a TCP/IP  socket.
			listeningSocket = new Socket(AddressFamily.InterNetwork, 
			                           SocketType.Stream, ProtocolType.Tcp );

			listeningSocket.Bind (remoteEP);

			listeningSocket.Listen (100);

			Thread acceptThread = new Thread(ServerAcceptThread);

			acceptThread.Start ();

		}

		public void Stop()
		{
			running = false;

			try {
				listeningSocket.Close();
				
				// close all open connection
				foreach (ServerConnection connection in allOpenConnections) {
					connection.Close();
				}
					

			} catch (Exception e) {
				Console.WriteLine (e);
			}

			listeningSocket.Close();
		}


		public void EnqueueASDU(ASDU asdu) 
		{
			if (enqueuedASDUs == null) {
				enqueuedASDUs = new Queue<ASDU> ();
			}

			if (enqueuedASDUs.Count == maxQueueSize)
				enqueuedASDUs.Dequeue ();

			enqueuedASDUs.Enqueue (asdu);

			Console.WriteLine ("Queue contains " + enqueuedASDUs.Count + " messages");

			foreach (ServerConnection connection in allOpenConnections) {
				if (connection.IsActive)
					connection.ASDUReadyToSend ();
			}
		}

		internal ASDU DequeueASDU() 
		{
			if (enqueuedASDUs == null)
				return null;

			if (enqueuedASDUs.Count > 0)
				return enqueuedASDUs.Dequeue ();
			else
				return null;
		}

		internal void Activated(ServerConnection activeConnection)
		{
			// deactivate all other connections

			foreach (ServerConnection connection in allOpenConnections) {
				if (connection != activeConnection)
					connection.IsActive = false;
			}
		}
	}
	
}
