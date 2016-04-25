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

	public delegate bool InterrogationHandler (object parameter, ServerConnection connection, ASDU asdu);

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

		public void SetInterrogationHandler(InterrogationHandler handler, object parameter)
		{
			this.interrogationHandler = handler;
			this.InterrogationHandlerParameter = parameter;
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
