
# 🔬 experiment_rtos.md: RTOS Performance & Network Behavior

**ESP32 + FreeRTOS + MQTT (with/without TLS/mTLS)**

**Task Mapping · Real-Time Scheduling · Packet Reliability Analysis**

---

## 🧭 Purpose & Motivation

This experiment was driven by two questions that emerged during development:

1. **Why does core pinning sometimes make publish timing worse, not better?**
2. **Why are MQTT packets dropped intermittently under mTLS, even when scheduling is correct?**

Through systematic testing and Wireshark packet analysis, the goal was to isolate the performance bottlenecks — whether they arise from RTOS scheduling, TLS overhead, or network-layer behavior.

---

## ⚙️ System Overview

- **Platform**: ESP32-WROOM (dual-core)
- **OS**: FreeRTOS
- **Sensor**: BME680 (Temp, Humidity, Pressure, Gas)
- **Broker**: Mosquitto 2.x on Ubuntu 22.04
- **Protocol**: MQTT (over TCP / TLS / Mutual TLS)
- **Network**: Wi-Fi (via 4G mobile hotspot, IPv6 NAT)

### Task Structure:

```cpp
xTaskCreatePinnedToCore(SensorReadTask, "SensorTask", 4096, NULL, 1, NULL, 0);
xTaskCreatePinnedToCore(MqttPublishTask, "MQTTPublish", 4096, NULL, 1, NULL, 1);
```

- `SensorTask`: Reads sensor data every 1s (`vTaskDelayUntil`)
- `MQTTPublishTask`: Publishes JSON-encoded payloads to broker
- Shared buffer protected by `SemaphoreHandle_t`

---

## 🧪 Experimental Matrix

| Case | TLS | Core Mapping | Avg Publish Time | Successful Publishes |
| --- | --- | --- | --- | --- |
| 1 | ❌ | Unpinned | 3.46 ms | 619 |
| 2 | ❌ | Sensor=0, MQTT=1 | 17.96 ms | 622 |
| 3 | ✅ | Unpinned | 2.00 ms | 642 |
| 4 | ✅ | Sensor=0, MQTT=1 | 3.44 ms | 642 |
| B1 | ✅ | Sensor=1, MQTT=0 | 2.00 ms | 581 |
| B2 | ❌ | Sensor=1, MQTT=0 | 1.84 ms | 580 |

**Timing:** Measured with `millis()` before/after `client.publish()`

**Duration:** 300 seconds per run

**Frequency:** 1 Hz publish cycle (`vTaskDelayUntil()`)

---

## 📈 1. Latency (Speed) Analysis

### ✅ Key Observations

- TLS/mTLS incurred **only ~1ms overhead** — acceptable for real-time use
- **Core pinning without consideration of Wi-Fi stack placement** led to severe performance drops
- Swapping `MQTTPublishTask` to Core 0 (same as Wi-Fi driver) drastically improved latency

### 🧠 Deeper Analysis

ESP32's Wi-Fi stack executes on **Core 0**. When `MQTTPublishTask` runs on **Core 1**, it leads to:

- Cross-core **cache coherence issues**
- **Mutex contention** across tasks
- Interrupts and driver context-switching across cores
- Possible **socket buffer misalignment** or queue delays

```c
// Problematic:
SensorTask → Core 0
MQTTPublishTask → Core 1

// Optimal:
SensorTask → Core 1
MQTTPublishTask → Core 0

```

> ❗ The MQTT task must stay on Core 0 to stay close to the Wi-Fi stack.
> 

---

## 📉 2. Stability (Reliability) Analysis

### 📡 Graphana Results

- Even when latency was low, **Grafana showed dropped points and bursty delivery**
- Intermittent **data gaps** of 1–4 seconds, regardless of core configuration
- **TLS-enabled** cases had *more stable timing* than plaintext — possibly due to session-level buffering

---

### 🧪 Wireshark Observations

| Symptom | Interpretation |
| --- | --- |
| Duplicate ACKs | Broker didn't receive full segment or was delayed |
| Retransmissions | ESP32 re-sent packet (common under load) |
| Spurious Retransmissions | Packet resent after successful delivery |
| Out-of-order delivery | Delayed arrival or reordering over unstable Wi-Fi |
| Previous segment not captured | Packet dropped (or not captured by Wireshark) |
| Empty MQTT publish payload | Possibly malformed retransmission or zero-length msg |
| PDU reassembly of multiple publishes | Nagle buffering or Wi-Fi delay |

Example:

```yaml
MQTT Publish – No payload visible
TCP Segment Len: 121
Conversation: Incomplete

```

---

## 🌐 Network Context

- **Network Type**: 4G Mobile Hotspot (IPv6 NAT)
- **Limitations**:
    - Unpredictable RTT
    - NAT queueing behavior
    - Signal strength fluctuation
    - No control over congestion or retransmission policies

> ❗ Even with perfectly timed publish cycles, network jitter distorted delivery
> 

---

## 🧠 Summary of Key Insights

### 🔹 Latency

| ✅ What Worked Well | ⚠️ What Hurt Performance |
| --- | --- |
| TLS overhead was minimal (<2 ms) | Core pinning without Wi-Fi awareness |
| Core 0 aligned with MQTT improved speed | Separate cores created cache/mutex issues |

### 🔹 Stability

| ✅ What Helped | ⚠️ What Broke It |
| --- | --- |
| TLS stabilized burst timing slightly | Mobile hotspot introduced dropouts, jitter |
| `vTaskDelayUntil()` kept local timing | Packet loss occurred in network → not RTOS |

---

## ❓ Questions for Further Validation

1. Are **empty MQTT publish packets** normal? Could they indicate send buffer underflow or retransmission?
2. Can **publish burst behavior** be controlled? Is it caused by Nagle's algorithm?
3. Would **TCP_NODELAY** on ESP32 WiFiClient help reduce packet coalescing?
4. Are there **LwIP diagnostics or socket-level counters** available on ESP32 for retransmit/drop tracking?
5. How can I confirm whether loss occurs **on ESP32**, **in transit**, or **at the broker**?

---

## ✅ Recommendations

| Category | Suggestion |
| --- | --- |
| Task Affinity | Pin MQTT to Core 0 (same as Wi-Fi stack) |
| Benchmark Env | Use dedicated Wi-Fi AP (not hotspot) for testing |
| Timing Tools | Use `esp_timer_get_time()` for higher resolution |
| TCP Behavior | Try disabling Nagle (set TCP_NODELAY) |
| Diagnostics | Log publish results + add drop counters manually |

---

Thank you!
