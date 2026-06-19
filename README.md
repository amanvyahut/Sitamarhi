# 🌾 TSCRIC-LoRa: Intelligent Precision Agriculture and Smart Irrigation Platform

<div align="center">

![Platform](https://img.shields.io/badge/Platform-ESP8266%20%2B%20LoRa-1DB954?style=for-the-badge)
![Firebase](https://img.shields.io/badge/Cloud-Firebase%20RTDB-FF9800?style=for-the-badge)
![Civil Engg](https://img.shields.io/badge/Dept-Civil%20Engineering-8B5CF6?style=for-the-badge)
![License](https://img.shields.io/badge/License-Academic-8B5CF6?style=for-the-badge)

**Minor Project · B.Tech Civil Engineering · Oriental College of Technology, Bhopal · RGPV · 2024–2027**

[🌐 Live Dashboard](https://amanvyahut.github.io/Sitamarhi/) • [📱 Download Android App](https://github.com/amanvyahut/Sitamarhi/releases/download/v1.0/TSCRIC-LoRa.apk) • [📄 Project Report](#)

</div>

---

## 📌 What is TSCRIC-LoRa?

**TSCRIC** stands for **Temporal Soil-Crop Resonance Irrigation Controller**.

It is an IoT-based intelligent precision agriculture and smart irrigation platform developed by civil engineering students at Oriental College of Technology, Bhopal. The system integrates real-time multi-depth soil moisture monitoring, LoRa long-range telemetry, sensor-driven irrigation decision-making, a multi-page SCADA-inspired web dashboard, and complete offline autonomous operation — all assembled from commodity hardware at approximately ₹3,000.

> This project demonstrates that classical **civil engineering irrigation principles** — duty of water, crop delta, net irrigation requirement, ETo, water balance modelling — can be implemented in real-time embedded firmware on an affordable microcontroller and presented through a professional web dashboard.

---

## 🏗️ Civil Engineering Relevance

Irrigation engineering is a core sub-discipline of civil engineering. TSCRIC-LoRa directly implements the following engineering parameters in firmware:

| Parameter | Equation | Implementation |
|-----------|----------|----------------|
| Duty of Water (D) | D = A / Q [ha/cumec] | YF-S201 flow sensor measures Q; plot area from config |
| Crop Delta (Δ) | Δ = 8.64 × B / D [m] | FAO-56 standard — Wheat 450 mm, Rice 1200 mm |
| Net Irrigation Req. (NIR) | NIR = (ETc − Pe) × A | ETc from Hargreaves-Samani × Kc; Pe from SCS CN |
| Gross Irrigation Req. (GIR) | GIR = NIR / Ea | Ea from applied vs delivered via flow meter |
| ETo (Hargreaves-Samani) | ETo = 0.0023 × Ra × √ΔT × (T+17.8) | DHT22 sensor, Ra = 10 MJ/m²/day for Central India |
| Root-Zone Water Balance | ΔS = I + P − ETc − DP − R | All terms computed in firmware, shown in dashboard |
| SCS Curve Number | Q = (P−Ia)² / (P−Ia+S); CN = 75 | Tipping bucket rainfall → effective rainfall |
| Readily Available Water | RAW = AWC × Zr × p | CSMI threshold calibrated to RAW depletion point |

Based on laboratory prototype testing, the system is estimated to have the potential to achieve **20–50% water savings** compared to conventional flood irrigation if deployed at field scale.

---

## ✨ Key Features

### 🌱 Core Irrigation Intelligence
- **CSMI (Composite Soil Moisture Index)** — tri-depth weighted average at 15 cm, 30 cm, and 45 cm
- **GDD-driven crop stage progression** — weights automatically shift as the crop grows (Stage 0 to Stage 3)
- **IIS (Intelligent Irrigation Score)** — 0–100 composite score from CSMI + SMV + TPR + ETo − Rain Penalty
- **SMV / SMA / TPR** — Soil Moisture Velocity, Acceleration, and Temporal Pattern Recognition for predictive irrigation

### 🌧️ Three-Tier Rainfall Intelligence
| Priority | Source | Internet Required? | Accuracy |
|----------|--------|--------------------|----------|
| 1 — PRIMARY | Tipping Bucket Rain Gauge (0.2 mm/tip) | No — fully offline | WMO standard direct measurement |
| 2 — BACKUP | OpenWeatherMap API | Online only | Regional NWP forecast |
| 3 — ESTIMATE | BMP280 Pressure Trend | No — fully offline | ±20%, 1–6 hour horizon |

### 📡 Communication Layers
- **Primary:** Firebase Realtime Database — 15-second cloud sync
- **Fallback:** LoRa SX1278 433 MHz — 30-second broadcast, 2–5 km range
- **Offline:** Local WiFi hotspot (SSID: `TSCRIC_AI`, IP: `192.168.4.1`) — full dashboard on farmer's phone

### 🖥️ Multi-Page Dashboard
- System Dashboard with IIS gauge, CSMI sparkline, Decision Explanation Engine
- Irrigation Control — pump panel, event log, seasonal water budget
- Sensor Network — 12-node LoRa monitoring, tri-depth VWC, environmental sensors
- Analytics — 7-day IIS history, water balance trend, ETo chart, soil moisture trends
- Soil Health — DS18B20 soil temperature, EC, pH, indicative NPK trend
- Smart Farm Advisory Assistant — rule-based, works completely offline in Hinglish

### ⚡ Other Highlights
- **Pulse irrigation** — 30-second ON pulses prevent surface ponding and runoff (application rate 16.7 mm/hr, below infiltration capacity)
- **Pump health monitoring** — motor condition, seal, bearing, cavity, performance index
- **Sensor failure fallback matrix** — SAFE MODE, OWM fallback, weight rescaling on partial sensor failure
- **EEPROM offline storage** — 20-event ring buffer, auto-syncs to Firebase on reconnection
- **millis() overflow bug fix** — prevents pump re-trigger after 49.7 days

---

## 🔧 Hardware Components

| Component | Specification | Est. Cost (₹) |
|-----------|--------------|---------------|
| ESP8266 NodeMCU V3 | ESP-12E, 4MB flash, 80MHz, WiFi | 280 |
| Capacitive Soil Moisture Sensor v2.0 (×3) | 3.3V, corrosion-resistant | 150 |
| CD4051 8-Channel MUX | Analog multiplexer for 3 sensors on 1 ADC | 25 |
| DHT22 Sensor | ±0.5°C, ±2% RH | 130 |
| BMP280 Module | I2C, 0–1100 hPa, ±1 hPa | 120 |
| LoRa SX1278 Module | 433 MHz, 17 dBm, SPI | 420 |
| YF-S201 Flow Sensor | 1–30 L/min, 7.5 pulses/litre | 250 |
| 5V Relay Module | Optoisolated, Active LOW | 80 |
| Mini DC Water Pump | 2.5–6V submersible, 80–120 L/hr | 150 |
| DS18B20 (optional) | Waterproof soil temperature | 120 |
| EC Sensor (optional) | 0–6 mS/cm | 200 |
| Soil pH Sensor (optional) | 0–14 pH range | 280 |
| Tipping Bucket Rain Gauge (optional) | 0.2 mm/tip, reed switch | 480 |
| Breadboard + Wires + Resistors | 830-point, Dupont, 4.7kΩ pull-ups | 175 |
| **Total (core)** | | **~₹1,785** |
| **Total (full system with optionals)** | | **~₹3,045** |

---

## 🧠 Algorithm Overview

### CSMI — Composite Soil Moisture Index
```
CSMI = w₁×VWC_15cm + w₂×VWC_30cm + w₃×VWC_45cm
```
Weights shift automatically by GDD-determined crop stage:

| Stage | GDD Range | w₁ (15cm) | w₂ (30cm) | w₃ (45cm) | Threshold |
|-------|-----------|-----------|-----------|-----------|-----------|
| Germination | 0–150 | 0.50 | 0.30 | 0.20 | 25% |
| Tillering | 150–400 | 0.40 | 0.35 | 0.25 | 28% |
| Grain-Fill | 400–800 | 0.30 | 0.40 | 0.30 | 30% |
| Maturation | >800 | 0.25 | 0.38 | 0.37 | 22% |

### IIS — Intelligent Irrigation Score
```
IIS = moistureScore + velocityScore + tprBonus + etoScore − rainPenalty
```

| Component | Max Points | Rationale |
|-----------|-----------|-----------|
| Moisture Score | 52.5 | CSMI deficit below threshold |
| Velocity Score (SMV) | 20.0 | Rate of moisture depletion |
| TPR Bonus | 15.0 | Cosine similarity of drying pattern |
| ETo Score | 15.0 | Atmospheric evaporative demand |
| Rain Penalty | −40 | Suppresses irrigation before rain |

Irrigation triggers when: **CSMI < threshold AND IIS > 65 AND rain probability < 75% AND cooldown elapsed AND not in SAFE MODE**

### ETo — Hargreaves-Samani (1985)
```
ETo = 0.0023 × Ra × √(tRange) × (T_mean + 17.8)   [mm/day]
ETc = ETo × Kc
```

### SCS Curve Number (CN = 75)
```
S = 25400/CN − 254
Ia = 0.2 × S
Q = (P − Ia)² / (P − Ia + S)     [when P > Ia]
Pe = P − Q   [effective rainfall]
```

---

## 🗂️ Repository Structure

```
Sitamarhi/
├── index.html        # Main multi-page dashboard (HTML5 + CSS3 + JS)
├── style.css         # Dark-theme SCADA-inspired stylesheet
├── app.js            # Firebase integration, sensor logic, AI assistant, charts
├── tscric.ino        # ESP8266 Arduino firmware (CSMI, IIS, ETo, LoRa, EEPROM)
└── README.md         # This file
```

---

## 🚀 Getting Started

### Dashboard (Web)
Visit the live dashboard directly:
```
https://amanvyahut.github.io/Sitamarhi/
```
For offline use, connect to the ESP8266 hotspot (`TSCRIC_AI`, password: `12345678`) and open:
```
http://192.168.4.1
```

### Firmware (ESP8266)
1. Install [Arduino IDE](https://www.arduino.cc/en/software) with ESP8266 board support
2. Install required libraries: `Firebase ESP8266`, `DHT sensor library`, `Adafruit BMP280`, `LoRa`, `OneWire`, `DallasTemperature`
3. Open `tscric.ino` in Arduino IDE
4. Configure your WiFi credentials and Firebase project URL in the firmware
5. Flash to ESP8266 NodeMCU via USB

### Firebase Setup
1. Create a Firebase project at [console.firebase.google.com](https://console.firebase.google.com)
2. Enable Realtime Database (set rules to allow read/write for testing)
3. Copy your database URL into `tscric.ino` and `app.js`

---

## 📊 System Performance (7-Day Lab Testing)

| Metric | Result |
|--------|--------|
| Irrigation Efficiency | 78% (Good — above 70% benchmark) |
| Water Savings (estimated vs flood baseline) | ~28–30% in test window |
| Projected seasonal savings (estimated) | 20–50% at field scale |
| Water Use Efficiency | 87% |
| LoRa Network Uptime | 99.2% |
| LoRa Packet Delivery Rate | 98.7% |
| Firebase Push Latency | avg 380 ms (4G) |
| Sensor Reading Cycle | 10 seconds (±50 ms) |

> ⚠️ **Note:** Results above are from laboratory prototype testing with a 6 m² plot. Field-scale validation has not yet been conducted. Water saving percentages are estimates based on a traditional flood irrigation baseline.

---

## 🔮 Future Scope

- **Solar power integration** — 5–10W panel + LiFePO4 + MPPT (PM-KUSUM compatible)
- **PCB + IP67 enclosure** — field-grade production deployment
- **DS3231 RTC** — accurate offline event timestamping
- **ESP32 upgrade** — dual-core, dual UART, Bluetooth
- **CNN crop disease detection** — ESP32-CAM + TensorFlow Lite (Phase 3)
- **Reinforcement Learning** — optimal irrigation policy from multi-season data (Phase 5)
- **LoRaWAN gateway** — command-area-wide monitoring at gram panchayat level
- **PM-KUSUM / PMKSY integration** — government scheme alignment

---

## 👥 Project Team

| Name | Role |
|------|------|
| **Aman Kumar** (0126CE243D04) | Team Lead · Project Developer |
| **Akash Kumar** (0126CE243D03) | Team Member |
| **Aditya Kumar** (0126CE243D01) | Team Member |
| **Akash Khargande** (0126CE243D02) | Team Member |
| **Dr. Yogesh Iyer Murthy** | Project Guide · Dept. of Civil Engineering, OCT Bhopal |

**Institution:** Oriental College of Technology, Bhopal · Department of Civil Engineering  
**University:** Rajiv Gandhi Proudyogiki Vishwavidyalaya (RGPV), Bhopal  
**Academic Year:** 2024–2027

---

## 📚 Key References

1. Allen et al. — FAO Irrigation and Drainage Paper 56 (1998) — Crop Evapotranspiration Guidelines
2. Hargreaves & Samani — Reference Crop ETo from Temperature (1985)
3. A.M. Michael — Irrigation Theory and Practice (Vikas Publishing, 2010)
4. S.K. Garg — Irrigation Engineering and Hydraulic Structures (Khanna Publishers, 2012)
5. WMO-No. 8 — Guide to Meteorological Instruments and Methods of Observation (2018)
6. Hsiao (1990) — Root-zone weighted averaging at multiple depths
7. Semtech — SX1276/77/78/79 LoRa Transceiver Datasheet

---

## 📄 License

This project is submitted as a **B.Tech Minor Project** at Oriental College of Technology, Bhopal under RGPV. The code and documentation are made available for academic reference. Commercial use is not permitted without prior permission from the project team and institution.

---

<div align="center">

**TSCRIC-LoRa** · Oriental College of Technology, Bhopal · Civil Engineering 2024–2027  
Guide: Dr. Yogesh Iyer Murthy · Team: Aman Kumar · Akash Kumar · Aditya Kumar · Akash Khargande

[🌐 Live Dashboard](https://amanvyahut.github.io/Sitamarhi/) | [📱 Android App](https://github.com/amanvyahut/Sitamarhi/releases/download/v1.0/TSCRIC-LoRa.apk)

</div>
