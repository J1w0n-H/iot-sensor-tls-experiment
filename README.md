
# 🔧 RTOS Meets TLS: Real-Time Secure Telemetry for IoT Systems

> ESP32 + FreeRTOS + MQTT over TLS/mTLS
> 
> 
> Real-time scheduling meets network security — under constrained hardware, unstable networks, and tight deadlines.
> 

---

## 📌 Project Overview

This project explores a critical challenge in embedded systems and IoT:

> Can a real-time operating system (RTOS) on a resource-constrained device reliably send encrypted telemetry data over unstable networks without violating timing guarantees?
> 

To answer this, I built and benchmarked a full system on ESP32 using FreeRTOS and MQTT, with optional TLS/mTLS security.

**Key questions:**

- 🔁 Does adding TLS or Mutual TLS break the real-time performance of the system?
- 🧩 How does core assignment (core affinity) affect task timing under dual-core RTOS?
- 🌐 How do real-world network conditions (e.g., mobile hotspot) influence reliability?

This was implemented and evaluated as part of the course **ENPM818J – Real-Time Operating Systems** at the University of Maryland.

---

## 🧠 Motivation

IoT systems are expected to be:

- **Real-time**: react within strict timing windows
- **Secure**: protect data against MITM and forgery
- **Resilient**: tolerate jitter, congestion, and poor networks

But embedded devices like ESP32 have:

- Limited CPU, RAM, and buffer capacity
- Complex Wi-Fi stack behavior
- Constraints in encryption, especially under dual-core concurrency

This project quantifies and visualizes those tradeoffs.

---

## 🧪 System Architecture
![Real-Time IoT Data Pipeline](https://github.com/J1w0n-H/iot-sensor-tls-experiment/blob/main/image/Real-Time%20IoT%20Data%20Pipeline.png?raw=true)
```
[SensorReadTask] --(mutex)--> [MQTTPublishTask] --TLS/mTLS--> Mosquitto Broker
         |                         |
      Core 1                   Core 0 (Wi-Fi stack)

```

- **FreeRTOS** dual-core scheduler
- Sensor task and MQTT publish task run in parallel
- Shared buffer protected via semaphore
- Data sent at 1 Hz via MQTT (QoS 0)
- Publish loop wrapped with TLS or mTLS (client cert + server CA)
- Benchmarked publish time, successful sends, and Wireshark packet traces

---

## 📊 What Was Measured

### 📈 RTOS Task Performance

- Publish timing (with/without TLS)
- Core pinned vs unpinned vs core-swapped
- Synchronization delay due to semaphores

### 📉 Network Reliability

- Packet loss, retransmission, and ACK behavior via Wireshark
- mTLS certificate handshake behavior
- Impact of hotspot vs local Wi-Fi

### 📊 Visualization

- Grafana dashboard for real-time sensor data
- Gaps, delay spikes, and bursty delivery patterns analyzed

---

## 📂 Repository Structure

```
graphql
CopyEdit
.
├── README.md                    # Project overview (this file)
├── experiment_rtos.md          # Detailed analysis of RTOS + MQTT TLS performance
├── docs/                       # Supplemental notes or scripts
├── esp32/
│   └── mqtt_tls_esp32/         # ESP32 firmware source
│       ├── mqtt_tls_esp32.ino  # Main Arduino source file
│       ├── certificates_esp32client.h
│       ├── fake-server.h
├── grafana/                    # Grafana dashboard JSONs (time-series visualization)
├── image/                      # Screenshots for Wireshark, Grafana, and system diagrams
├── index/                      # (unspecified, possibly for logging or indexing)
├── scripts/
│   └── make_cert_h.sh          # Script to generate certificates_*.h
├── server/
│   ├── certs/                  # Root CA, client/server certificates and keys
│   ├── mosquitto/              # Mosquitto TLS/mTLS configs
│   ├── telegraf/               # Telegraf configs per security mode
│   └── socat/                  # MITM test configs and fake CA infrastructure

```

---

## 🔍 Key Findings

✅ TLS on ESP32 adds <2ms overhead thanks to hardware crypto

❌ Core pinning (MQTT=Core1) worsens performance (~18ms publish time)

✅ Swapping MQTT to Core 0 (Wi-Fi aligned) improves latency (~1.8ms)

⚠️ TLS helps consistency, but packet drops still occur

❗ Real bottleneck = mobile hotspot → unstable RTT, NAT buffering, packet loss

> “RTOS scheduling can be perfect — but the network never is.”
> 

---

## 📦 Technologies Used

| Category | Tools / Frameworks |
| --- | --- |
| Microcontroller | ESP32-WROOM |
| OS | FreeRTOS (dual-core, tasks, semaphores) |
| Protocol | MQTT 3.1.1 (PubSubClient) |
| Security | OpenSSL (Root CA, TLS/mTLS certs) |
| Networking | Wi-Fi (via mobile hotspot) |
| Broker | Mosquitto on Ubuntu |
| Monitoring | Wireshark + Grafana + InfluxDB |

---

## 📌 Coming Next: `experiment_network.md`

This upcoming section will focus on:

- **TLS vs mTLS** latency comparison
- **Unauthorized client denial**
- **MITM attack simulation** using Burp Suite
- **Certificate forgery attempts**
- Broker rejection logs + mTLS validation behavior

---

## 🙋 Future Improvements

- Log publish success/failure at the application level
- Capture retransmission count on ESP32 (LwIP stack, socket diagnostics)
- Compare wired vs wireless real-time performance
- Add replay attack defense (broker-side logic)

---

## 👤 Author

**Jiwon Hwang**

Graduate student in Cybersecurity Engineering

University of Maryland (ENPM818J – Spring 2025)

📧 jhwang97@umd.edu

---

## 📎 For Reviewers

This project aims to contribute not only as a functional IoT pipeline, but as an **educational case study** on integrating:

- RTOS scheduling
- TLS cryptographic overhead
- Core-to-task mapping
- Real-world network dynamics

For feedback or collaboration inquiries, feel free to reach out!

---

Thank you!
