
# experiment_rtos.md

## 1. Experimental Goals

- Measure how TLS and mutual TLS (mTLS) impact real-time MQTT publishing performance on an ESP32 using FreeRTOS.
- Determine the effect of core pinning (task-to-core assignment) on timing consistency and task interference.
- Identify the performance threshold of publish intervals (down to 100ms) in a dual-core RTOS environment.
- Evaluate whether task separation, mutex protection, and TLS overhead preserve real-time constraints (e.g., 1Hz or faster).

---

## 2. Hypotheses

- TLS introduces minimal overhead due to ESP32's hardware acceleration and should not violate soft real-time constraints.
- Pinned tasks may suffer synchronization delays due to inter-core mutex contention.
- 100ms publish intervals may start to exceed system throughput, causing message drops or increased jitter.
- Unpinned tasks will yield better publish consistency due to dynamic load balancing across cores.

---

## 3. System Configuration

### 3.1 Hardware

- ESP32 DevKitC v1 with dual-core FreeRTOS
- BME680 environmental sensor via I2C
- MQTT Broker: Ubuntu 22.04 host with Mosquitto (TLS/mTLS configured)
- Network: Mobile hotspot with NAT to simulate latency and jitter

### 3.2 Software

- Firmware: Arduino-based FreeRTOS with separate tasks:
  - `SensorReadTask` → collects data and writes to shared struct
  - `MqttPublishTask` → publishes data over MQTT with optional TLS/mTLS
- Shared data protected with `xSemaphoreCreateMutex()`
- TLS via `WiFiClientSecure` using PEM-formatted certificates
- Real-time interval control via `vTaskDelay(pdMS_TO_TICKS(INTERVAL_MS))`
- Publish timing benchmark via `millis()` and logging

---

## 4. Experimental Matrix

| Case ID | TLS Enabled | Core Pinned | Interval | Description |
|---------|-------------|-------------|----------|-------------|
| C1      | No          | No          | 1000ms   | Plain MQTT baseline |
| C2      | Yes (TLS)   | No          | 1000ms   | TLS without pinning |
| C3      | Yes (TLS)   | Yes         | 1000ms   | TLS with core pinning |
| C4      | No          | Yes         | 1000ms   | Plain MQTT with core pinning |
| C5      | Yes (TLS)   | No          | 100ms    | TLS + fast publish |
| C6      | Yes (TLS)   | Yes         | 100ms    | TLS + pinning + fast interval |

---

## 5. Raw Results (Per Case)

| Case | Avg Publish Time | Success Rate | Notes |
|------|------------------|---------------|-------|
| C1   | 4.70 ms          | 559 / 600     | Plain MQTT, frequent reconnects |
| C2   | 2.01 ms          | 580 / 580     | TLS stable, minimal overhead |
| C3   | 2.16 ms          | 580 / 580     | Slight delay due to pinning |
| C4   | 2.88 ms          | 580 / 580     | Pinning without TLS still introduces latency |
| C5   | >5 ms (spikes)   | 400 / 580     | 100ms interval shows instability |
| C6   | Dropped packets  | <100 / 580    | Core pinning + 100ms → worst results |

---

## 6. Visualization

- Grafana dashboard connected to InfluxDB via Telegraf (`mqtt_consumer` input)
- Dashboards show:
  - C1: visible gaps, reconnects
  - C2–C4: smooth time-series plots
  - C5–C6: missing points, bursty updates

---

## 7. Packet-Level Analysis

- C1: All MQTT packets in plaintext (including ID/PW and payload)
- C2/C3/C5/C6: TLS handshake (ClientHello → ServerHello → Certificate → Finished)
- Core-pinned TLS cases showed slight delays in ACK and Application Data compared to unpinned

---

## 8. Root Cause Analysis

### 8.1 Client-side

- Sensor and MQTT tasks contend over mutex in core-pinned case
- Fast intervals (100ms) exceed I/O and task switching ability

### 8.2 Network-side

- Mobile hotspot introduced NAT delays and TCP retransmissions
- No advanced congestion controls applied

### 8.3 Server-side

- Mosquitto behaved consistently; no CPU or memory issues
- TLS handshake failures logged properly under mTLS

---

## 9. Comparative Insights

| Rank | Configuration        | Performance | Stability | Notes |
|------|----------------------|-------------|-----------|-------|
| 🥇   | C2: TLS, No Pinning  | Excellent   | Stable    | Best real-time profile |
| 🥈   | C3: TLS + Pinned     | Good        | Stable    | Slight delay from core contention |
| 🥉   | C1: No TLS, No Pin   | Poor        | Unstable  | Frequent reconnects |
| 🛑   | C6: TLS + Pinned + 100ms | Unusable | Fails often | Too aggressive timing |

---

## 10. Design Implications

- FreeRTOS with TLS is capable of maintaining soft real-time constraints.
- Core pinning should be used carefully; mutexes across cores can introduce jitter.
- 1Hz intervals are safe with TLS; sub-500ms intervals require tuning and testing.
- Task separation and mutex protection enabled modularity and fault isolation.

---

## 11. Configuration Optimization

| Parameter       | Recommended Value   | Rationale |
|-----------------|---------------------|-----------|
| INTERVAL_MS     | ≥ 500ms             | Safe range for ESP32 tasks |
| Core Assignment | Sensor = Core0, MQTT = Core1 | Avoid same-core mutex conflict |
| TLS Version     | TLS 1.2             | Stable, supported by ESP32 |
| MQTT QoS        | 0 or 1              | Low overhead, sufficient for sensors |
| Buffering       | Mutex + NotifyGive  | Lightweight and thread-safe |

---

## 12. Future Experiments

- QoS 1 vs QoS 2 delivery in TLS/mTLS mode
- ECC certificate use for handshake performance
- Add simulated attacks (DoS, replay, auth failures)
- Compare performance over wired Ethernet or Wi-Fi 6

---
