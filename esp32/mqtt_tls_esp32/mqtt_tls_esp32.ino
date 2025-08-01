#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include "fake-server.h"  // TLS certificates for secure MQTT communication

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"  // For RTOS mutex (mutual exclusion)

#define SEC 2           // 0: Plain, 1: ID/PW, 2: TLS, 3: mTLS
#define USE_CORE_PINNING false  // Use core-pinning for real concurrency (Sensor=Core 0, MQTT=Core 1)
#define INTERVAL_MS 100        // Sensor read + publish interval in milliseconds
#define BENCH_TIME_MS 300000   // Benchmark duration (e.g., 5 min = 300,000 ms)

// Wi-Fi credentials
const char* ssid = "Jiwon";
const char* password = "aaaaaaaa";

// MQTT broker configuration
const char* mqtt_server = "172.20.10.10";
const char* mqtt_user = "esp32client";
const char* mqtt_password = "password";

// Global structure for holding shared sensor data
struct SensorData {
  float temperature;
  float humidity;
  float pressure;
  float gas_resistance;
  bool newData = false;  // Set true when fresh data is available for publishing
};

SensorData sensorData;

#if (SEC == 2 || SEC == 3)
WiFiClientSecure espClient;
const int mqtt_port = 8883;
#else
WiFiClient espClient;
const int mqtt_port = 1883;
#endif

PubSubClient client(espClient);
Adafruit_BME680 bme;
String ipAddress = "0.0.0.0";
SemaphoreHandle_t xMutex;  // Protects sensorData in multi-threaded context
const TickType_t publishInterval = pdMS_TO_TICKS(INTERVAL_MS);

// Benchmark metrics
unsigned long totalPublishTime = 0;
unsigned int successfulPublishes = 0;
unsigned long benchmarkStartTime;
bool benchmarkDone = false;  // To print summary once only

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

void connectToWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  ipAddress = WiFi.localIP().toString();
  Serial.println("\n✅ WiFi connected. IP: " + ipAddress);
}

void settingTLS() {
#if (SEC == 2 || SEC == 3)
  Serial.println("🔐 TLS Enabled: Loading certificates...");
  espClient.setCACert(ca_cert);
  //espClient.setInsecure();
  Serial.println("CA Cert Begins:");
  Serial.println(ca_cert);
#endif

#if (SEC == 3)
  espClient.setCertificate(client_cert);
  espClient.setPrivateKey(client_key);
#endif

#if (SEC == 0 || SEC == 1)
  Serial.println("🔓 TLS Disabled: Using plain MQTT over TCP");
#endif
}

void connectToMQTT() {
  client.setServer(mqtt_server, mqtt_port);
  while (!client.connected()) {
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);

    bool connected = false;
    if (SEC == 1 || SEC == 2) {
      connected = client.connect(clientId.c_str(), mqtt_user, mqtt_password);
    } else {
      connected = client.connect(clientId.c_str());
    }

    if (connected) {
      Serial.println("✅ MQTT connected");
    } else {
      Serial.printf("❌ MQTT failed, rc=%d ➜ %s\n", client.state(), getMqttErrorMessage(client.state()).c_str());
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }
}
void SensorReadTask(void *parameter) {
  Serial.println("🌡️ Initializing BME680 sensor...");
  Serial.println("Trying BME680 at address 0x77...");
  
  if (!bme.begin(0x77)) {
    Serial.println("❌ BME680 not found. Halting");
    while (true) { vTaskDelay(1000); }
  }
  
  Serial.println("✅ BME680 initialized! Configuring...");
  
  // 설정 단계마다 디버그 출력 추가
  Serial.println("Setting temperature oversampling...");
  bme.setTemperatureOversampling(BME680_OS_8X);
  
  Serial.println("Setting humidity oversampling...");
  bme.setHumidityOversampling(BME680_OS_2X);
  
  Serial.println("Setting pressure oversampling...");
  bme.setPressureOversampling(BME680_OS_4X);
  
  Serial.println("Setting IIR filter...");
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  
  Serial.println("Setting gas heater...");
  bme.setGasHeater(320, 150);
  
  Serial.println("✅ BME680 configuration complete!");

  while (true) {
    if (!bme.performReading()) {
      Serial.println("⚠️ Sensor reading failed");
      vTaskDelay(publishInterval);
      continue;
    }
    if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
      sensorData.temperature = bme.temperature;
      sensorData.humidity = bme.humidity;
      sensorData.pressure = bme.pressure / 100.0;
      sensorData.gas_resistance = bme.gas_resistance / 1000.0;
      sensorData.newData = true;
      xSemaphoreGive(xMutex);
    }
    vTaskDelay(publishInterval);
  }
}

void MqttPublishTask(void *parameter) {
  Serial.println("👉 Starting MQTT task...");
  
  Serial.println("👉 Connecting to WiFi...");
  connectToWiFi();
  
  Serial.println("👉 Setting up TLS...");
  settingTLS();
  
  Serial.println("👉 Connecting to MQTT broker...");
  connectToMQTT();

  unsigned long start, end;
  benchmarkStartTime = millis();

  while (true) {
    start = millis();
    if (millis() - benchmarkStartTime <= BENCH_TIME_MS) {
      if (sensorData.newData) {
        if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
          String payload = "{";
          payload += "\"temperature\":" + String(sensorData.temperature, 2) + ",";
          payload += "\"humidity\":" + String(sensorData.humidity, 2) + ",";
          payload += "\"pressure\":" + String(sensorData.pressure, 2) + ",";
          payload += "\"gas_resistance_kohm\":" + String(sensorData.gas_resistance, 2) + ",";
          payload += "\"ip\":\"" + ipAddress + "\"}";

          if (!client.connected()) {
            Serial.println("🔄 Reconnecting MQTT...");
            connectToMQTT();
          }

          if (client.publish("sensor/data", payload.c_str(), true)) {
            end = millis();
            totalPublishTime += (end - start);
            successfulPublishes++;
            float avgPublishTime = (float)totalPublishTime / successfulPublishes;
            float leftTimeSec = (BENCH_TIME_MS - (millis() - benchmarkStartTime)) / 1000.0;
            Serial.printf("📤 Published: %s\n", payload.c_str());
            Serial.printf("⏱ Publish Duration: %lu ms, Avg: %.2f ms, left: %.2f s\n", (end - start), avgPublishTime, leftTimeSec);
          } else {
            Serial.println("❌ Publish failed.");
          }

          sensorData.newData = false;
          xSemaphoreGive(xMutex);
        }
      }
    } else if (!benchmarkDone) {
      Serial.println("\n===== 📊 Benchmark Summary =====");
      const char* sec_mode =
        (SEC == 0) ? "Plain MQTT (No Auth)" :
        (SEC == 1) ? "Plain MQTT + ID/PW" :
        (SEC == 2) ? "TLS (Server-only)" :
                    "mTLS (Mutual TLS)";
      Serial.printf("🔐 Security Mode: %s, Core pinned: %s\n", sec_mode, USE_CORE_PINNING ? "true" : "false");


      Serial.printf("🔁 Total successful publishes: %d\n", successfulPublishes);
      Serial.printf("⏱ Total time: %.2f seconds\n", (millis() - benchmarkStartTime) / 1000.0);
      Serial.printf("📉 Average publish time: %.2f ms\n", (float)totalPublishTime / successfulPublishes);
      Serial.println("=================================\n");
      benchmarkDone = true;
    }

    vTaskDelay(publishInterval);
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(); // I2C 버스 초기화 추가
  delay(1000);
  Serial.println("🚀 Starting MQTT TLS RTOS Benchmark...");
  xMutex = xSemaphoreCreateMutex();

  if (USE_CORE_PINNING) {
    xTaskCreatePinnedToCore(SensorReadTask, "SensorTask", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(MqttPublishTask, "MQTTPublish", 4096, NULL, 1, NULL, 0);
  } else {
    xTaskCreate(SensorReadTask, "SensorTask", 4096, NULL, 1, NULL);
    xTaskCreate(MqttPublishTask, "MQTTPublish", 4096, NULL, 1, NULL);
  }
}

void loop() {
  // Not used in RTOS model
}