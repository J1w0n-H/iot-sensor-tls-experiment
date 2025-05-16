# 🔬 experiments.md: RTOS Performance Exploration – Task Design, Core Allocation, and Real-Time Scheduling

This document focuses on the **real-time operating system (RTOS)** experimentation aspect of the IoT project, while also addressing observed **network-related behaviors** that influenced real-time performance. It details the system design, benchmarking strategy, timing behavior, and network environment limitations affecting dual-core ESP32 telemetry under FreeRTOS.

---

## 🎯 RTOS Experiment Goals

As IoT devices scale in complexity, they must balance:
- **Determinism** (guaranteed timing)
- **Concurrency** (parallel task execution)
- **Resource constraints** (limited memory/CPU)

This study explores:
- ✅ Task-to-core mappings and their effects on publish latency
- ✅ Real-time scheduling integrity under encryption and Wi-Fi load
- ✅ ESP32's handling of mutex, timer, and Wi-Fi stack in real-world traffic
- ✅ How network characteristics (e.g., mobile hotspot) degrade timing precision

---

## ⚙️ RTOS System Design

### 📋 Task Structure

```c
xTaskCreatePinnedToCore(SensorReadTask, "SensorTask", 4096, NULL, 1, NULL, 0);
xTaskCreatePinnedToCore(MqttPublishTask, "MQTTPublish", 4096, NULL, 1, NULL, 1);
```

- `SensorTask`: Reads BME680 every 1s with `vTaskDelayUntil()`
- `MQTTPublishTask`: Publishes JSON over MQTT with optional TLS
- `SemaphoreHandle_t sensorDataMutex` used for data integrity

### 🔁 Synchronization
- `xSemaphoreTake()` before accessing sensor buffer
- `xSemaphoreGive()` after publish to prevent data collision

---

## 🧪 Test Scenarios

Each test ran 300 seconds with:
- `millis()` before/after `client.publish()`
- Average publish time = `(total time) / (successful publishes)`

All tests used fixed topic, QoS=0, and 1Hz publish rate:

| Case | TLS | Core Pinned | Description |
|------|-----|-------------|-------------|
| 1    | ❌  | ❌          | Unpinned baseline |
| 2    | ❌  | ✅          | Sensor=Core0, MQTT=Core1 |
| 3    | ✅  | ❌          | TLS enabled, unpinned |
| 4    | ✅  | ✅          | TLS + pinned: Sensor=Core0, MQTT=Core1 |
| B1   | ✅  | ✅ (Swapped)| Sensor=Core1, MQTT=Core0 |
| B2   | ❌  | ✅ (Swapped)| Sensor=Core1, MQTT=Core0 |

---

## 📊 Publish Time Results

| Case | TLS | Core Config       | Avg Time | Publishes |
|------|-----|--------------------|----------|-----------|
| 1    | ❌  | Unpinned           | 3.46 ms  | 619       |
| 2    | ❌  | Sensor=0, MQTT=1   | 17.96 ms | 622       |
| 3    | ✅  | Unpinned           | 2.00 ms  | 642       |
| 4    | ✅  | Sensor=0, MQTT=1   | 3.44 ms  | 642       |
| B1   | ✅  | Sensor=1, MQTT=0   | 2.00 ms  | 581       |
| B2   | ❌  | Sensor=1, MQTT=0   | 1.84 ms  | 580       |

### Measurement Notes
- `millis()` used inside MQTT publish task
- Timing included TLS encryption + Wi-Fi I/O + OS delay
- `vTaskDelayUntil()` maintained timing intervals at 1Hz

---

## 🌐 Network Behavior Observations

Although RTOS performance was the main focus, **network conditions** had visible effects:

### 🛰️ Network Setup
- All experiments ran over **mobile hotspot (4G)** with NAT + DHCP bridge
- Broker (Ubuntu) and ESP32 both connected to same Wi-Fi SSID

### 🔎 Wireshark Insights
- ESP32 sent periodic MQTT publishes at 1Hz
- Under load, observed:
  - **Retransmissions** (likely Wi-Fi interference)
  - **Duplicate ACKs** from server
  - **Out-of-Order** packet arrivals
- Delay clusters appeared ~2–4s apart (bursty latency)

### ❗ Hotspot Limitations
- Inconsistent latency during TLS handshake (up to ~400ms)
- Higher variance in ACK arrival time
- Performance degradation not from ESP32/RTOS but **network jitter**

### ✏️ Example Issue
```bash
Previous segment not captured
Spurious Retransmission
Dup ACK #5
```
These logs confirm inconsistent delivery paths over hotspot

### 🔧 Interpretation
- TLS did not cause major delay by itself
- Performance bottlenecks often arose from **wireless retransmission**
- Core pinning made things worse when Wi-Fi stack and MQTT ran on separate cores

---

## 🧠 RTOS Behavior Summary

- `vTaskDelayUntil()` upheld 1Hz scheduling ±2ms jitter
- Mutex-protected data shared cleanly between tasks
- Default pinning (MQTT=Core1) clashed with Wi-Fi stack (Core0)
- Swapping fixed cache + scheduling stalls
- Network instability can mask scheduler accuracy unless isolated

---

## ✅ Final Recommendations

- ✅ Avoid benchmarking over mobile hotspots if precise timing matters
- ✅ Use Wi-Fi channels with minimal interference
- ✅ Place MQTT task on **Core 0** (Wi-Fi context)
- ✅ Use `esp_timer_get_time()` for finer-grain timing if needed

---

## 💡 Conclusion

RTOS timing and core mapping analysis reveals that:
- FreeRTOS performs well under constrained conditions
- Real-world networks introduce variability that overshadows OS behavior
- Accurate real-time telemetry requires both OS-level tuning and **network-aware design**

> "In RTOS systems, the scheduler might be perfect—but the network never is."


ESP32 sent periodic MQTT publishes at 1Hz

Under load, observed:

Retransmissions (likely Wi-Fi interference)

Duplicate ACKs from server

Out-of-Order packet arrivals

Delay clusters appeared ~2–4s apart (bursty latency)

❗ Hotspot Limitations

Inconsistent latency during TLS handshake (up to ~400ms)

Higher variance in ACK arrival time

Performance degradation not from ESP32/RTOS but network jitter

✏️ Example Issue

Previous segment not captured
Spurious Retransmission
Dup ACK #5

These logs confirm inconsistent delivery paths over hotspot

🔧 Interpretation

TLS did not cause major delay by itself

Performance bottlenecks often arose from wireless retransmission

Core pinning made things worse when Wi-Fi stack and MQTT ran on separate cores

🧠 RTOS Behavior Summary

vTaskDelayUntil() upheld 1Hz scheduling ±2ms jitter

Mutex-protected data shared cleanly between tasks

Default pinning (MQTT=Core1) clashed with Wi-Fi stack (Core0)

Swapping fixed cache + scheduling stalls

Network instability can mask scheduler accuracy unless isolated

✅ Final Recommendations

✅ Avoid benchmarking over mobile hotspots if precise timing matters

✅ Use Wi-Fi channels with minimal interference

✅ Place MQTT task on Core 0 (Wi-Fi context)

✅ Use esp_timer_get_time() for finer-grain timing if needed

💡 Conclusion

RTOS timing and core mapping analysis reveals that:

FreeRTOS performs well under constrained conditions

Real-world networks introduce variability that overshadows OS behavior

Accurate real-time telemetry requires both OS-level tuning and network-aware design

"In RTOS systems, the scheduler might be perfect—but the network never is."
