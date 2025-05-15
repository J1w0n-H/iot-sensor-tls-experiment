#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include "certificates.h"  // TLS certificates for secure MQTT communication

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"  // For RTOS mutex (mutual exclusion)

#define USE_TLS true           // Enable TLS (set to false for plaintext MQTT)
#define USE_CORE_PINNING false  // Use core-pinning for real concurrency (Sensor=Core 0, MQTT=Core 1)
#define INTERVAL_MS 100        // Sensor read + publish interval in milliseconds
#define BENCH_TIME_MS 180000   // Benchmark duration (e.g., 5 min = 300,000 ms)

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

#if USE_TLS
WiFiClientSecure espClient;
const int mqtt_port = 8883;  // TLS port
#else
WiFiClient espClient;
const int mqtt_port = 1883;  // Non-TLS port
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
#if USE_TLS
  Serial.println("🔐 TLS Enabled: Setting certificates...");
  espClient.setCACert(ca_cert);
  espClient.setCertificate(client_cert);
  espClient.setPrivateKey(client_key);
#else
  Serial.println("🔓 TLS Disabled: Using insecure MQTT");
#endif
}

void connectToMQTT() {
  client.setServer(mqtt_server, mqtt_port);
  while (!client.connected()) {
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("✅ MQTT connected");
    } else {
      Serial.printf("❌ MQTT failed, rc=%d ➜ %s\n", client.state(), getMqttErrorMessage(client.state()).c_str());
      vTaskDelay(pdMS_TO_TICKS(5000));
    }
  }
}

void SensorReadTask(void *parameter) {
  Serial.println("🌡️ Initializing BME680 sensor...");
  if (!bme.begin()) {
    Serial.println("❌ Sensor not found. Halting");
    while (true);
  }

  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150);

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
  connectToWiFi();
  settingTLS();
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
      Serial.printf("TLS: %s, Core pinned: %s\n", USE_TLS ? "true" : "false", USE_CORE_PINNING ? "true" : "false");
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
  delay(1000);
  Serial.println("🚀 Starting MQTT TLS RTOS Benchmark...");
  xMutex = xSemaphoreCreateMutex();

  if (USE_CORE_PINNING) {
    xTaskCreatePinnedToCore(SensorReadTask, "SensorTask", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(MqttPublishTask, "MQTTPublish", 4096, NULL, 1, NULL, 1);
  } else {
    xTaskCreate(SensorReadTask, "SensorTask", 4096, NULL, 1, NULL);
    xTaskCreate(MqttPublishTask, "MQTTPublish", 4096, NULL, 1, NULL);
  }
}

void loop() {
  // Not used in RTOS model
}
