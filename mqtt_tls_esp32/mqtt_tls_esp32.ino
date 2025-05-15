#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include "certificates.h"  // 인증서: ca_cert, client_cert, client_key

#define SEALEVELPRESSURE_HPA (1013.25)
const char* ssid = "Jiwon";
const char* password = "aaaaaaaa";


// MQTT broker settings
const char* mqtt_server = "172.20.10.10";
//const char* ssid = "University View";
//const char* password = "qydwbt39";
//const char* mqtt_server = "192.168.124.130";
const int mqtt_port = 8883;

// 객체 생성
WiFiClientSecure espClient;
PubSubClient client(espClient);
Adafruit_BME680 bme;

// 디버깅 타이밍
unsigned long lastSend = 0;
unsigned long sendInterval = 5000;

// 상태 출력 헬퍼
void printStatus(const char* stage) {
  Serial.print("📌 ");
  Serial.println(stage);
}

// MQTT 에러 코드 설명
String getMqttErrorMessage(int code) {
  switch (code) {
    case -4: return "Connection Timeout";
    case -3: return "Connection Lost";
    case -2: return "Connect Failed (likely TLS or auth)";
    case -1: return "Disconnected";
    case  0: return "Connected";
    case  1: return "Bad Protocol";
    case  2: return "ID Rejected";
    case  3: return "Server Unavailable";
    case  4: return "Bad Credentials";
    case  5: return "Not Authorized";
    default: return "Unknown";
  }
}



void setup() {
  Wire.begin();
  Serial.begin(115200);
  delay(1000);

  printStatus("🚀 Starting...");

  // WiFi 연결
  WiFi.begin(ssid, password);
  Serial.print("📶 Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected");

  // TLS 인증서 (테스트 목적 - CN 무시)
  printStatus("🔐 Setting certificates...");
  espClient.setCACert(ca_cert);
  espClient.setCertificate(client_cert);
  espClient.setPrivateKey(client_key);

  //espClient.setInsecure(); // CN 미일치 허용 (실제 운영환경에서는 제거)
  Serial.println("✅ Certificates set");

  // MQTT 설정
  client.setServer(mqtt_server, mqtt_port);

  // BME680 센서 초기화
  printStatus("🌡️ Initializing BME680 sensor...");
  if (!bme.begin(0x77)) {
    Serial.println("❌ BME680 not found at 0x77! Check wiring.");
    while (true); // 센서 미연결 시 멈춤
  }
  Serial.println("✅ BME680 sensor initialized");

  // BME680 필수 설정
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setGasHeater(320, 150);  // 320°C for 150 ms
}
// MQTT 연결 시도
void reconnect() {
  while (!client.connected()) {
    Serial.print("🔁 Connecting to MQTT... ");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), "esp32client", "password")) {
      Serial.println("✅ Connected");
      client.subscribe("sensor/data");
    } else {
      Serial.print("❌ MQTT connection failed, rc=");
      Serial.print(client.state());
      Serial.print(" ➜ ");
      Serial.println(getMqttErrorMessage(client.state()));
      delay(5000);
    }
  }
}
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (millis() - lastSend > sendInterval) {
    Serial.println("📡 Reading BME680 sensor...");
    if (!bme.performReading()) {
      Serial.println("⚠️ Failed to perform BME680 reading");
      return;
    }

    float temperature = bme.temperature;
    float humidity = bme.humidity;

    // JSON 형식으로 변환
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"temperature\": %.2f, \"humidity\": %.2f}",
             temperature, humidity);

    // MQTT 전송
    bool success = client.publish("sensor/data", payload);
    if (success) {
      Serial.print("📤 Published: ");
      Serial.println(payload);
    } else {
      Serial.println("❌ Failed to publish MQTT message");
    }

    lastSend = millis();
  }
}
