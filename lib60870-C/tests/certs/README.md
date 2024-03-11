This folder include test certificates for TLS related tests

* Three different root CAs
** root_CA1
** root_CA2
** root_CA3

* For each root CAs there are two clients and two server keys/certificates
** server_CA1_1
** server_CA1_2
** client_CA1_1
** client_CA1_2
** server_CA2_1
** server_CA2_2
** client_CA2_1
** client_CA2_2
** server_CA3_1
** server_CA3_2
** client_CA3_1
** client_CA3_2


Example: Generate root CA keys and certificates:

  openssl genrsa -out root_CA1.key 2048
  openssl req -x509 -new -nodes -key root_CA1.key -sha256 -days 3650 -out root_CA1.pem

Example: Create endpoint (client/server) certificates:

  openssl genrsa -out server_CA1_2.key 2048

  openssl req -new -key server_CA1_1.key -out server_CA1_1.csr

  openssl x509 -req -in server_CA1_1.csr -CA root_CA1.pem -CAkey root_CA1.key -CAcreateserial -out server_CA1_1.pem

