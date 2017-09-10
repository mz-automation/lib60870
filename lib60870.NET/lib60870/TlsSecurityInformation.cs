using System;
using System.Security.Cryptography.X509Certificates;
using System.Collections.Generic;

namespace lib60870
{
	public class TlsSecurityInformation
	{
		private X509Certificate2 ownCertificate;

		private List<X509Certificate2> otherCertificates;

		private List<X509Certificate2> caCertificates;

		private string targetHostName = null;

		// Check certificate chain validity with registered CAs
		private bool chainValidation = true;

		private bool allowOnlySpecificCertificates = false;

		/// <summary>
		/// Gets or sets a value indicating whether this <see cref="lib60870.TlsSecurityInformation"/> performs a X509 chain validation
		/// against the registered CA certificates
		/// </summary>
		/// <value><c>true</c> if chain validation; otherwise, <c>false</c>.</value>
		public bool ChainValidation {
			get {
				return this.chainValidation;
			}
			set {
				chainValidation = value;
			}
		}

		/// <summary>
		/// Gets or sets a value indicating whether this <see cref="lib60870.TlsSecurityInformation"/> allow only specific certificates.
		/// </summary>
		/// <value><c>true</c> if allow only specific certificates; otherwise, <c>false</c>.</value>
		public bool AllowOnlySpecificCertificates {
			get {
				return this.allowOnlySpecificCertificates;
			}
			set {
				allowOnlySpecificCertificates = value;
			}
		}

		public TlsSecurityInformation (string targetHostName, X509Certificate2 ownCertificate)
		{

			this.targetHostName = targetHostName;
			this.ownCertificate = ownCertificate;

			otherCertificates = new List<X509Certificate2> ();
			caCertificates = new List<X509Certificate2> ();
		}


		public TlsSecurityInformation (X509Certificate2 ownCertificate)
		{
			this.ownCertificate = ownCertificate;

			otherCertificates = new List<X509Certificate2> ();
			caCertificates = new List<X509Certificate2> ();
		}

		public X509Certificate2 OwnCertificate {
			get {
				return this.ownCertificate;
			}
			set {
				ownCertificate = value;
			}
		}

		public List<X509Certificate2> AllowedCertificates {
			get {
				return this.otherCertificates;
			}
			set {
				otherCertificates = value;
			}
		}

		public List<X509Certificate2> CaCertificates {
			get {
				return this.caCertificates;
			}
		}

		public string TargetHostName {
			get {
				return this.targetHostName;
			}
		}

		public void AddAllowedCertificate(X509Certificate2 allowedCertificate)
		{
			otherCertificates.Add (allowedCertificate);
		}

		public void AddCA(X509Certificate2 caCertificate)
		{
			caCertificates.Add (caCertificate);
		}
	}
}

