#!/bin/bash
set -e

# === [1] 기본 설정 ===
CA_CN="MyRootCA"
SERVER_CN="localhost"
CLIENT_CN="esp32client"
SERVER_IP="172.20.10.10"
CERT_DIR="./"
HEADER_FILE="certificates_$CLIENT_CN.h"

echo "📁 Working in $CERT_DIR"
cd "$CERT_DIR"

# === [2] Root CA 생성 ===
echo "🔐 Generating Root CA..."
openssl genrsa -out ca.key 2048
openssl req -x509 -new -nodes -key ca.key -sha256 -days 1024 \
  -subj "/C=US/ST=MD/L=UMD/O=MQTTBroker/CN=$CA_CN" \
  -out ca.crt

# === [3] 서버 인증서 + SAN 포함 ===
echo "📡 Generating Server Certificate (with SAN)..."
cat > server_ext.cnf <<EOF
[ req ]
distinguished_name = req_distinguished_name
req_extensions = v3_req
[ req_distinguished_name ]
[ v3_req ]
subjectAltName = @alt_names
[ alt_names ]
DNS.1 = localhost
IP.1 = 127.0.0.1
IP.2 = $SERVER_IP
EOF

openssl genrsa -out server.key 2048
openssl req -new -key server.key \
  -subj "/C=US/ST=MD/L=UMD/O=MQTTBroker/CN=$SERVER_CN" \
  -out server.csr
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
  -out server.crt -days 365 -sha256 -extfile server_ext.cnf -extensions v3_req

# === [4] 클라이언트 인증서 + clientAuth 확장 ===
echo "📲 Generating Client Certificate (clientAuth)..."
cat > client_ext.cnf <<EOF
extendedKeyUsage = clientAuth
EOF

openssl genrsa -out telegraf.key 2048
openssl req -new -key telegraf.key \
  -subj "/C=US/ST=MD/L=UMD/O=IoTDevices/CN=$CLIENT_CN" \
  -out telegraf.csr
openssl x509 -req -in telegraf.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
  -out telegraf.crt -days 365 -sha256 -extfile client_ext.cnf

# === [5] 권한 설정 ===
echo "🔧 Setting file permissions..."
chmod 600 *.key
chmod 644 *.crt
chown mosquitto:mosquitto *.crt *.key 2>/dev/null || echo "ℹ️ Skipping chown (not root or mosquitto user not found)"

# === [6] ESP32용 헤더 생성 ===
echo "📦 Creating certificates.h for ESP32..."
echo '// Auto-generated certificates.h for ESP32 Mutual TLS' > "$HEADER_FILE"
echo '' >> "$HEADER_FILE"

for VAR in ca_cert client_cert client_key; do
  FILE=""
  case $VAR in
    ca_cert) FILE="ca.crt";;
    client_cert) FILE="telegraf.crt";;
    client_key) FILE="telegraf.key";;
  esac

  echo "const char* $VAR = " >> "$HEADER_FILE"
  sed 's/^/"/; s/$/\\n"/' "$FILE" >> "$HEADER_FILE"
  echo ";" >> "$HEADER_FILE"
  echo "" >> "$HEADER_FILE"
done

echo "✅ Done! All files generated:"
ls -lh ca.* server.* telegraf.* "$HEADER_FILE"


