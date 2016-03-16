using System;

using lib60870;
using System.Net;
using System.Net.Sockets;
using System.Threading;

namespace lib60870
{

	public delegate bool InterrogationHandler (object parameter, ServerConnection connection, ASDU asdu);

	public delegate bool ASDUHandler (object parameter, ServerConnection connection, ASDU asdu);

	public class Server {

		private string localHostname = "127.0.0.1";

		private bool running = false;

		private ConnectionParameters parameters = null;

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

		public void Start() 
		{
			IPAddress ipAddress = IPAddress.Parse(localHostname);
			IPEndPoint remoteEP = new IPEndPoint(ipAddress, parameters.TcpPort);

			// Create a TCP/IP  socket.
			Socket socket = new Socket(AddressFamily.InterNetwork, 
			                           SocketType.Stream, ProtocolType.Tcp );

			socket.Bind (remoteEP);

			socket.Listen (100);

			running = true;

			Console.WriteLine ("Waiting for connections...");

			while (running) {

				Socket newSocket = socket.Accept ();

				if (newSocket != null) {
					Console.WriteLine ("Connected");

					new ServerConnection (newSocket, parameters, this);
				}
			}

		}

		public void Stop()
		{
			running = false;
		}

	}
	
}
