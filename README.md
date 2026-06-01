# 🌾 TSCRIC-LoRa — AI-Powered Smart Irrigation Dashboard

<div align="center">

![Version](https://img.shields.io/badge/Version-5.0%20Enterprise-00D4FF?style=for-the-badge)
![Platform](https://img.shields.io/badge/Platform-ESP8266%20%2B%20LoRa-1DB954?style=for-the-badge)
![Firebase](https://img.shields.io/badge/Cloud-Firebase%20RTDB-FF9800?style=for-the-badge)
![License](https://img.shields.io/badge/License-Academic-8B5CF6?style=for-the-badge)

**Temporal Soil-Crop Resonance Irrigation Controller**  
*AI-Powered Precision Agriculture | Multi-Depth Soil Intelligence | LoRa Mesh | SCADA Dashboard*

🔗 **[Live Dashboard →](https://amanvyahut.github.io/Sitamarhi/)**

*B.Tech Minor Project — Civil Engineering | Oriental College of Technology, Bhopal | RGPV | 2024–27*

</div>

---

## 📋 Table of Contents

- [Project Overview](#-project-overview)
- [Key Features](#-key-features)
- [System Architecture](#-system-architecture)
- [Hardware Components](#-hardware-components)
- [Firmware Algorithms](#-firmware-algorithms)
- [Dashboard Pages](#-dashboard-pages)
- [Offline Operation](#-offline-operation)
- [Civil Engineering Relevance](#-civil-engineering-relevance)
- [Results](#-results)
- [Repository Structure](#-repository-structure)
- [Setup & Deployment](#-setup--deployment)
- [Team](#-team)

---

## 🌱 Project Overview

TSCRIC-LoRa is an IoT-based precision agriculture system that integrates **real-time multi-depth soil moisture monitoring**, **long-range LoRa telemetry**, **AI-assisted irrigation decisions**, and a **multi-page SCADA-style web dashboard** — all built from commodity hardware at approximately ₹3,000.

India loses **40–60% of irrigation water** to inefficient practices. This system directly embeds classical civil engineering irrigation principles (duty of water, crop delta, NIR, GIR, ETo, FAO-56 water balance) into firmware running on an 80 MHz microcontroller.

**7-Day Prototype Testing Results:**
| Metric | Result |
|--------|--------|
| Irrigation Efficiency | **78%** |
| Water Saved vs Traditional | **28% (1,890 L)** |
| Water Use Efficiency | **87%** |
| LoRa Network Uptime | **99.2%** |
| Firebase Push Latency | **~380 ms** |
| Sensor Cycle | **10 seconds** |

---

## ✨ Key Features

### 🧠 AI & Decision Engine
- **Composite Soil Moisture Index (CSMI)** — GDD-driven weighted average across 3 depths
- **AI Score (0–120 scale)** — CSMI deficit + SMV + SMA + TPR + ETo − Rain Penalty
- **SMV / SMA / TPR** — Soil Moisture Velocity, Acceleration, Temporal Pattern Recognition
- **AI Explainability Engine** — structured reasoning cards for every irrigation decision
- **AI Farm Assistant** — chat interface for farm queries (rule-based + Gemini AI)

### 🌧️ Rainfall Intelligence (3-Tier Priority)
| Priority | Source | Internet Required |
|----------|--------|-------------------|
| 1 — PRIMARY | Tipping Bucket Rain Gauge (0.2 mm/tip, ISR) | ❌ No |
| 2 — BACKUP | OpenWeatherMap API (cloud NWP) | ✅ Yes |
| 3 — ESTIMATE | BMP280 Pressure Trend (hPa/hr) | ❌ No |

### 📡 Connectivity
- **WiFi → Firebase RTDB** (15s sync, primary)
- **LoRa SX1278 433 MHz** (30s broadcast, fallback, 2–5 km range)
- **Local WiFi Hotspot** (TSCRIC_AI, 192.168.4.1, offline dashboard)

### 🌱 Soil & Crop Monitoring
- **3-depth capacitive sensors**: 15 cm / 30 cm / 45 cm
- **GDD Crop Stage Automation**: 4 stages with automatic threshold & weight shifts
- **8 crop types**: Wheat, Rice, Maize, Cotton, Soybean, Chickpea, Mustard, Sugarcane
- **DS18B20** soil temperature, **EC** salinity, **pH** acidity, empirical **NPK** estimation
- **ETo** via Hargreaves-Samani (DHT22 + BMP280, FAO-56 standard)

### 💧 Irrigation Control
- **Pulse Irrigation**: 30-second ON pulses (prevents runoff, maximises infiltration)
- **SCS Curve Number** (CN=75) for effective rainfall calculation
- **Flow meter metering**: YF-S201 (~7.5 pulses/litre)
- **Pump Health Monitoring**: motor temp, vibration, current, voltage, health index
- **millis() overflow guard** for 49.7-day rollover protection

### 📊 Dashboard & Analytics
- Multi-page SCADA-inspired web dashboard (GitHub Pages, mobile-first)
- 7-day historical analytics: soil moisture, irrigation, rainfall, ETo, AI Score
- Seasonal water budget tracking
- Smart Alert Center with real-time fault diagnostics

---

## 🏗️ System Architecture

```
FIELD LAYER                  EDGE NODE               CLOUD & STORAGE
─────────────────────        ──────────────────       ─────────────────
Soil Sensors (15/30/45cm) ──▶                        Firebase RTDB
DHT22 (Temp/Humidity)     ──▶  ESP8266 NodeMCU  ──▶  OpenWeatherMap API
BMP280 (Pressure)         ──▶  (CSMI · AI Score ──▶  GitHub Pages Dashboard
YF-S201 (Flow Meter)      ──▶   · ETo · Pump)
Tipping Bucket            ──▶
                                    │ (offline)
                                    ▼
                               LoRa SX1278 433MHz
                               EEPROM Ring Buffer
                               Hotspot 192.168.4.1
```

**Communication Layers:**
- **Primary**: WiFi → Firebase RTDB (every 15s)
- **Fallback**: LoRa SX1278 broadcast (every 30s, 2–5 km)
- **Offline**: Local WiFi AP → Dashboard at 192.168.4.1

---

## 🔧 Hardware Components

| Component | Model | Qty | Cost (₹) |
|-----------|-------|-----|-----------|
| Microcontroller | ESP8266 NodeMCU V3 (4MB Flash) | 1 | 280 |
| Soil Moisture Sensor | Capacitive v2.0, 3.3V | 3 | 150 |
| Analog MUX | CD4051 8-Channel | 1 | 25 |
| Temp/Humidity | DHT22 (±0.5°C, ±2% RH) | 1 | 130 |
| Pressure | BMP280 I2C (±1 hPa) | 1 | 120 |
| LoRa Radio | SX1278 433 MHz (17 dBm) | 1 | 420 |
| Flow Meter | YF-S201 (1–30 L/min) | 1 | 250 |
| Relay Module | 5V 1-CH Optoisolated | 1 | 80 |
| Water Pump | Mini DC Submersible 5V | 1 | 150 |
| Rain Gauge (opt.) | Tipping Bucket 0.2mm/tip | 1 | 480 |
| **Total (core)** | | | **~₹1,785** |
| **Total (full)** | | | **~₹3,045** |

### GPIO Pin Allocation (ESP8266)

| GPIO | NodeMCU Pin | Connected To | Function |
|------|-------------|-------------|---------|
| GPIO14 | D5 | CD4051 S0 | MUX Select Bit 0 |
| GPIO12 | D6 | CD4051 S1 | MUX Select Bit 1 |
| GPIO16 | D0 | CD4051 S2 | MUX Select Bit 2 |
| A0 | A0 | CD4051 COM OUT | ADC Input |
| GPIO4 | D2 | DHT22 Data | Temperature/Humidity |
| GPIO2 | D4 | Relay IN | Pump Control (Active LOW) |
| GPIO13 | D7 | YF-S201 Signal | Flow Pulse Interrupt |
| GPIO15 | D8 | LoRa NSS | SPI Chip Select |
| GPIO0 | D3 | LoRa DIO0 | LoRa Interrupt |
| GPIO10 | SD3 | Tipping Bucket | Rain Gauge ISR |

---

## 🧮 Firmware Algorithms

### CSMI Calculation
```
CSMI = w₁×VWC_15cm + w₂×VWC_30cm + w₃×VWC_45cm
```
Weights shift automatically by GDD-determined crop stage:

| Stage | GDD Range | w₁ (15cm) | w₂ (30cm) | w₃ (45cm) | Threshold |
|-------|-----------|-----------|-----------|-----------|-----------|
| Germination | 0–150 | 0.50 | 0.30 | 0.20 | 25% |
| Vegetative | 150–400 | 0.40 | 0.35 | 0.25 | 28% |
| Reproductive | 400–800 | 0.30 | 0.40 | 0.30 | 30% |
| Maturation | >800 | 0.25 | 0.38 | 0.37 | 22% |

### AI Score Components
```
AI Score = moistureScore + velocityScore + tprBonus + etoScore − rainPenalty
```
| Component | Formula | Max |
|-----------|---------|-----|
| Moisture Score | max(0, (35−CSMI) × 1.5) | 52.5 |
| Velocity Score | \|SMV\| × 10 (if SMV < −0.5) | 20.0 |
| TPR Bonus | +15 if TPR ≥ 0.85 | 15.0 |
| ETo Score | (ETo−3.0) × 3.0 | 15.0 |
| Rain Penalty | −40 if P>75%; −15 if P>35% | −40 |

### Irrigation Decision Gate (5 Conditions — ALL must pass)
```
1. CSMI < stage threshold
2. Rain probability < 75%
3. AI Score > 65
4. Pump cooldown elapsed (1 hour)
5. Safe Mode NOT active
```

### ETo — Hargreaves-Samani
```
es  = 0.6108 × exp(17.27×T / (T+237.3))  [kPa]
ea  = es × RH / 100                        [kPa]
VPD = es − ea                              [kPa]
ETo = 0.0023 × Ra × √tRange × (T+17.8)   [mm/day]
```

### SCS Curve Number (CN=75)
```
S  = 25400/CN − 254 = 83.9 mm
Ia = 0.2 × S = 16.8 mm
Q  = (P−Ia)² / (P−Ia+S)   [runoff, mm]
Pe = P − Q                  [effective rainfall, mm]
```

---

## 📱 Dashboard Pages

| Page | Description |
|------|-------------|
| **Welcome** | Live KPI ring: AI Score, Soil Moisture, Pump Status, Weather |
| **Dashboard** | CSMI, SMV, SMA, TPR, ETo, Rain Prob, AI Decision Engine |
| **Sensors** | 12-node network, 3-depth VWC, environmental sensors |
| **Irrigation** | Pump control, event log, rainfall analytics, seasonal budget |
| **Analytics** | 7-day charts: soil moisture, irrigation, rainfall, ETo, AI Score |
| **Alerts** | Smart Alert Center, fault diagnostics, real-time categories |
| **Connectivity** | WiFi/LoRa/Firebase status, OWM vs local sensor comparison |
| **Pump Control** | Real-time motor params, scheduling, health index |
| **Soil Health** | DS18B20, EC, pH, NPK, soil health score |
| **Configuration** | Crop/area selection, soil calibration engine (ADC→VWC) |
| **AI Assistant** | Rule-based + Gemini AI chat, quick action buttons |

---

## 📴 Offline Operation

When WiFi is unavailable, the system switches seamlessly to autonomous offline mode:

```
WiFi Lost → LoRa SX1278 broadcast (every 30s)
         → EEPROM ring buffer (20 irrigation events)
         → Local WiFi AP: SSID "TSCRIC_AI" | Pass "12345678"
         → Dashboard at 192.168.4.1
         → Batch Firebase sync on reconnection
```

**EEPROM Memory Layout:**
| Addresses | Content | Size |
|-----------|---------|------|
| 0–59 | Soil Calibration (3 × SoilCalib structs) | 60 bytes |
| 60–69 | Config (crop, area, logCount) | 10 bytes |
| 103–118 | Water balance totals | 16 bytes |
| 119–122 | Tipping bucket accumulated mm | 4 bytes |
| 128–527 | Irrigation log ring buffer (20 records) | 400 bytes |

---

## 🏛️ Civil Engineering Relevance

This project implements standard irrigation engineering equations in real-time firmware:

| Parameter | Equation | Implementation |
|-----------|----------|----------------|
| Duty of Water | D = A/Q [ha/cumec] | YF-S201 measures Q live |
| Crop Delta | Δ = 8.64×B/D [m] | Pre-configured per crop |
| Net Irrigation Req. | NIR = (ETc−Pe)×A | ETc from HS + SCS CN |
| Gross Irrigation Req. | GIR = NIR/Ea | Flow meter efficiency |
| ETo (HS Method) | ETo = 0.0023×Ra×√ΔT×(T+17.8) | Every 10 seconds |
| Root-Zone Balance | ΔS = I+P−ETc−DP−R | Water budget panel |
| Runoff (SCS CN) | Q = (P−Ia)²/(P−Ia+S) | Tipping bucket data |

---

## 📊 Results

**7-Day Prototype Testing (OCT Bhopal Lab):**

- ✅ 10-second sensor cycle within ±50 ms consistently
- ✅ Firebase sync at ~380 ms average latency (4G)
- ✅ CSMI sequential depth response confirmed (SM1 → SM2+30s → SM3+60s)
- ✅ Offline mode activated within 60-second watchdog
- ✅ EEPROM batch sync confirmed (12 pending records uploaded correctly)
- ✅ millis() overflow guard verified at 49.7-day rollover
- ✅ LoRa: 99.2% uptime, −67 dBm avg RSSI, 98.7% packet delivery
- ✅ Water savings: 28% vs traditional flood irrigation benchmark

---

## 📁 Repository Structure

```
├── index.html          # Main dashboard (all pages, single-page app)
├── style.css           # Dark-theme SCADA CSS (custom properties)
├── app.js              # Core firmware bridge + dashboard logic
├── manifest.json       # PWA manifest (installable app)
├── sw.js               # Service Worker (offline caching)
├── TSCRIC_v5.ino       # ESP8266 Arduino firmware (complete)
└── README.md           # This file
```

---

## 🚀 Setup & Deployment

### Dashboard (GitHub Pages)
```
1. Fork this repository
2. Go to Settings → Pages → Source: main branch / root
3. Dashboard live at: https://<username>.github.io/<repo>/
```

### Firebase Setup
```
1. Create Firebase project at console.firebase.google.com
2. Enable Realtime Database (test mode for prototype)
3. Update FIREBASE_CONFIG in app.js with your credentials
4. Enable OpenWeatherMap API key (free tier) in OWM_DIRECT_KEY
```

### Firmware (Arduino IDE)
```
Board:   ESP8266 NodeMCU 1.0 (ESP-12E Module)
CPU:     80 MHz
Flash:   4MB (FS: 1MB, OTA: ~1019KB)

Required Libraries:
  - ESP8266WiFi (built-in)
  - FirebaseESP8266 by Mobizt
  - DHT sensor library by Adafruit
  - Adafruit BMP280
  - ArduinoJson
  - LoRa by Sandeep Mistry
  - OneWire + DallasTemperature (for DS18B20)

Upload:
  1. Open TSCRIC_v5.ino in Arduino IDE
  2. Fill WiFi credentials and Firebase config
  3. Upload at 115200 baud
```

### Local Offline Dashboard
```
1. Connect to WiFi: SSID "TSCRIC_AI" | Password "12345678"
2. Open browser: http://192.168.4.1
3. Full dashboard available without internet
```

---

## 👥 Team

| Name | Roll Number |
|------|-------------|
| **Aman Kumar** | 0126CE243D04 |
| Akash Kumar | 0126CE243D03 |
| Aditya Kumar | 0126CE243D01 |
| Akash Khargande | 0126CE243D02 |

**Faculty Guide:** Dr. Yogesh Iyer Murthy  
**Department:** Civil Engineering, OCT Bhopal  
**University:** RGPV, Bhopal  
**Academic Year:** 2024–2027

---

## 📚 References

Key references used in this project:
- FAO-56: Allen et al. (1998) — Crop Evapotranspiration Guidelines
- Hargreaves & Samani (1985) — Reference ETo from Temperature
- Hsiao (1990) — Root-zone weighted averaging theory
- A.M. Michael — Irrigation Theory and Practice
- S.K. Garg — Irrigation Engineering and Hydraulic Structures
- WMO-No. 8 (2018) — Guide to Meteorological Instruments

---

## 🔗 Links

- 🌐 **Live Dashboard**: [amanvyahut.github.io/Sitamarhi](https://amanvyahut.github.io/Sitamarhi/)
- 📄 **Project Report**: Available in repository

---

<div align="center">

*TSCRIC-LoRa v5.0 Enterprise | Oriental College of Technology, Bhopal*  
*"Make every drop count. Save water. Grow better."*

![Made with](https://img.shields.io/badge/Made%20with-ESP8266%20%2B%20Firebase%20%2B%20LoRa-00D4FF?style=flat-square)
![Civil Engineering](https://img.shields.io/badge/Civil%20Engineering-IoT%20Project-1DB954?style=flat-square)

</div>
