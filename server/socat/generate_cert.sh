#!/bin/bash

# [1] SAN config 파일 생성
cat > fake_server_san.cnf <<EOF
[req]
distinguished_name=req
x509_extensions=v3_req
[req_distinguished_name]
[ v3_req ]
subjectAltName = @alt_names
[ alt_names ]
DNS.1 = localhost
IP.1 = 127.0.0.1
EOF

# [2] Private key 생성
openssl genrsa -out fake-server.key 2048

# [3] Self-signed cert (with SANs)
openssl req -x509 -new -nodes -key fake-server.key \
  -sha256 -days 365 \
  -out fake-server.crt \
  -subj "/CN=127.0.0.1" \
  -extensions v3_req -config fake_server_san.cnf

