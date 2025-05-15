---

## 🧪 Experiment Summary

We conducted a series of experiments to evaluate:

1. **RTOS Task Scheduling** (Core-pinned vs. non-pinned)
2. **MQTT Performance** with and without TLS
3. **Security Comparison**:
   - No authentication
   - ID/PW only (username/password auth)
   - Mutual TLS (mTLS)

---

## ⏱ Benchmark Table

| Experiment ID | TLS Enabled | Core Pinned | Avg Publish Time |
|---------------|-------------|-------------|------------------|
| Exp-1         | ✅ Yes      | ✅ Yes      | 2.16 ms          |
| Exp-2         | ❌ No       | ✅ Yes      | 2.88 ms          |
| Exp-3         | ❌ No       | ❌ No       | 1.94 ms          |
| Exp-4         | ✅ Yes      | ❌ No       | 2.01 ms          |

> **Interpretation**:
> - TLS overhead is minimal on ESP32, but present (~0.2–0.9 ms increase).
> - Core-pinning causes more cache overhead, especially for shared memory + mutex access.

---

## 🔐 Security Experiment

| Setup                | Result                               | Notes                                      |
|----------------------|--------------------------------------|--------------------------------------------|
| No Auth              | ❌ Unauthorized clients can connect  | Vulnerable to DoS and data injection       |
| Username/Password    | ⚠️ Brute-force possible             | Passwords visible unless TLS is used       |
| Mutual TLS (mTLS)    | ✅ Only valid clients allowed         | Client certs verified by server            |

- Simulated MITM attack using Burp Suite
  - 🔍 Observed client disconnection under mTLS
  - 🧪 TLS handshake fails with forged client cert
- Observed session rejection logs in `mosquitto.log`

---

## 📊 Visualization Snapshots

| Scenario               | Grafana Snapshot                               |
|------------------------|------------------------------------------------|
| TLS + Core Pinning     | ![TLS-Pinned](docs/grafana_tls_pinned.png)     |
| No TLS + No Pinning    | ![Fastest Case](docs/grafana_fastest_case.png) |
| Unauthorized Access    | ![Blocked Client](docs/mosquitto_block.png)    |

---

## 📌 Conclusion

- **TLS + RTOS** systems can handle encryption overhead within tight deadlines (~2ms).
- **Mutual TLS** significantly increases MQTT security and should be default in sensitive deployments.
- Core-pinning does not guarantee speed gains; scheduler flexibility often performs better.

---
