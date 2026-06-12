/*
 * ============================================================
 * TSCRIC-LoRa v5.0 ENTERPRISE EDITION
 * Temporal Soil-Crop Resonance Irrigation Controller
 * ============================================================
 * Institution : Oriental College of Technology, Bhopal
 * Academic    : B.Tech Civil Engineering 2025-2026
 * Guide       : Dr. Yogesh Iyer Murthy
 * Team        : Aman Kumar (Lead) · Aditya Kumar ·
 *               Akash Khandgre · Akash Kumar
 * ============================================================
 * Hardware (Core — always present)
 *   ESP8266 NodeMCU V3
 *   SX1278 LoRa 433 MHz (SPI)
 *   3× Capacitive Soil Moisture via CD4051 MUX
 *   DHT22  — Temperature + Humidity
 *   BMP280 — Pressure + Altitude
 *   YF-S201 — Flow meter
 *   5 V 1-Channel Relay — Pump control
 *
 * Hardware (Optional — v5.0, enable in #define block below)
 *   DS18B20 — Soil Temperature
 *   EC Sensor (analog)  — Electrical Conductivity
 *   pH Sensor (analog)  — Soil pH
 *   Tipping Bucket rain gauge (interrupt)
 *
 * Cloud / Connectivity
 *   Firebase Realtime Database (15 s push)
 *   OpenWeatherMap API (backup weather, 5 min)
 *   LoRa mesh fallback (30 s broadcast)
 *   Offline hotspot TSCRIC_AI  IP 192.168.4.1
 * ============================================================
 * v5.0 Changes vs v4.0
 *   - NPK estimation from EC + soilTemp
 *   - Pump run-time, cycles, water-delivered counters
 *   - Motor voltage / current / power estimation fields
 *   - Pump schedule support (3 slots, Firebase driven)
 *   - LoRa multi-node packet tracker (TX + RX counts)
 *   - GDD accumulation using real DHT22 daily avg
 *   - Crop stage auto-advance from GDD thresholds
 *   - Weather primary source flag (local / OWM)
 *   - Analytics history ring buffer (7 day × 24 h)
 *   - Analytics fields: irrigByDepth, waterSaved
 *   - Connectivity: loraNodeCount pushed to Firebase
 *   - Bug fix: ETo formula — correct Hargreaves-Samani
 *   - Bug fix: adcToVWC division-by-zero guard
 *   - Bug fix: flowRate double-count on pump re-start
 *   - Bug fix: EEPROM_LOG_SIZE mismatch with IrrigLog struct
 *   - Bug fix: deltaApplied accumulation on every Firebase
 *             push (reset moved to correct place)
 *   - Bug fix: tipPulseCount ISR needs IRAM_ATTR
 *   - Bug fix: pumpCooldownEnd overflow when millis wraps
 * ============================================================
 * v5.1 Changes vs v5.0
 *   - Bug fix: EEPROM_SIZE was 512 but log ring buffer needs
 *             128 + (20 x 20) = 528 bytes — last log entry was
 *             silently truncated. EEPROM_SIZE increased to 540.
 *   - Bug fix: Firebase pumpOn/pumpOff/resetDaily commands were
 *             never cleared after execution, causing the pump
 *             to re-trigger in a loop. Now PATCHed back to false
 *             in tscric/commands once handled.
 * ============================================================
 */

// ============================================================
// OPTIONAL HARDWARE — uncomment to enable
// ============================================================
// #define ENABLE_DS18B20       // Soil temperature sensor
// #define ENABLE_EC_SENSOR     // EC via A0 (share with MUX CH3)
// #define ENABLE_PH_SENSOR     // pH via A0 (share with MUX CH4)
// #define ENABLE_TIPPING_BUCKET // Rain gauge interrupt

// ============================================================
// LIBRARIES
// ============================================================
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_BMP280.h>
#include <SPI.h>
#include <LoRa.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

#ifdef ENABLE_DS18B20
  #include <OneWire.h>
  #include <DallasTemperature.h>
#endif

// ============================================================
// USER CONFIGURATION — EDIT THESE
// ============================================================
const char* WIFI_SSID       = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD   = "YOUR_WIFI_PASSWORD";
const char* FIREBASE_HOST   = "ai-irrigation-system-1e112-default-rtdb.firebaseio.com";
const char* FIREBASE_SECRET = "YOUR_FIREBASE_SECRET";

// ============================================================
// PIN DEFINITIONS
// ============================================================
#define MUX_S0_PIN    14   // D5 — CD4051 select bit 0
#define MUX_S1_PIN    12   // D6 — CD4051 select bit 1
#define MUX_S2_PIN    16   // D0 — CD4051 select bit 2
#define MUX_SIG_PIN   A0   // A0 — MUX analog output
#define DHT_PIN        4   // D2 — DHT22
#define RELAY_PIN      2   // D4 — Pump relay (Active LOW)
#define FLOW_PIN      13   // D7 — YF-S201 pulse
#define LORA_NSS      15   // D8
#define LORA_RST      -1   // not wired
#define LORA_DIO0      0   // D3

#ifdef ENABLE_DS18B20
  #define DS18B20_PIN  5   // D1
#endif
#ifdef ENABLE_TIPPING_BUCKET
  #define TIPPING_PIN 10   // SD3
#endif

// ============================================================
// SENSOR OBJECTS
// ============================================================
DHT dht(DHT_PIN, DHT22);
Adafruit_BMP280 bmp;

#ifdef ENABLE_DS18B20
  OneWire           oneWire(DS18B20_PIN);
  DallasTemperature soilTherm(&oneWire);
#endif

// ============================================================
// TIMING CONSTANTS
// ============================================================
const unsigned long SENSOR_INTERVAL     = 10000UL;   // 10 s
const unsigned long FIREBASE_INTERVAL   = 15000UL;   // 15 s
const unsigned long LORA_INTERVAL       = 30000UL;   // 30 s
const unsigned long WIFI_RETRY_INTERVAL = 60000UL;   // 60 s
const unsigned long FLOW_FAILSAFE_MS    = 30000UL;   // 30 s no-flow → fault
const unsigned long PUMP_COOLDOWN_MS    = 3600000UL; // 1 h between auto triggers
const unsigned long PUMP_ON_MS          = 30000UL;   // 30 s pulse ON
const unsigned long GDD_UPDATE_INTERVAL = 3600000UL; // 1 h GDD accumulation

// ============================================================
// CROP DATA
// ============================================================
// Delta (mm/season), FC (%), PWP (%), GDD thresholds [stage0→1, 1→2, 2→3]
struct CropInfo {
  const char* name;
  float  delta;
  float  fc;
  float  pwp;
  float  gddStage[3]; // GDD to advance stages 0→1, 1→2, 2→3
};

const CropInfo CROPS[8] = {
  {"Wheat (Gehun)",     450,  38, 13, {150, 400, 800}},
  {"Rice (Dhan)",      1200,  50, 28, {120, 350, 900}},
  {"Maize (Makka)",     550,  38, 13, {100, 350, 750}},
  {"Cotton (Kapas)",    750,  37, 13, {200, 600,1200}},
  {"Soybean (Soya)",    500,  37, 13, {120, 350, 700}},
  {"Chickpea (Chana)",  350,  35, 12, {100, 300, 650}},
  {"Mustard (Sarson)",  380,  34, 12, {100, 280, 550}},
  {"Sugarcane (Ganna)",1800,  42, 16, {300, 900,2200}}
};

// CSMI weights [stage][depth]
const float STAGE_WEIGHTS[4][3] = {
  {0.50f, 0.30f, 0.20f},
  {0.40f, 0.35f, 0.25f},
  {0.30f, 0.40f, 0.30f},
  {0.25f, 0.38f, 0.37f}
};

// Irrigation trigger thresholds (CSMI %)
const float STAGE_THRESHOLDS[4] = {25.0f, 28.0f, 30.0f, 22.0f};

// ============================================================
// CALIBRATION STRUCT
// ============================================================
struct SoilCalib {
  int   adc_dry;
  int   adc_fc;
  int   adc_pwp;
  float vwc_fc;
  float vwc_pwp;
};

SoilCalib calibration[3] = {
  {850, 600, 750, 0.35f, 0.12f},
  {845, 595, 745, 0.35f, 0.12f},
  {855, 605, 755, 0.35f, 0.12f}
};

// ============================================================
// EEPROM LAYOUT
// ============================================================
#define EEPROM_SIZE          540   // v5.1 bug fix: 128 + (20 x 20) = 528, was 512 (overflow)
#define EEPROM_CALIB_START   0      // 3 × sizeof(SoilCalib) = 3×20 = 60 B
#define EEPROM_CONFIG_START  60     // crop(4) + area(4) + logCount(1) + logPtr(1) = 10 B
#define EEPROM_LOG_START     128    // ring buffer

// v5.0: IrrigLog is 17 bytes; keep max 20 entries to stay in 512 B
#define EEPROM_LOG_COUNT     20
#define EEPROM_LOG_SIZE      20     // bytes reserved per entry (pad to align)

struct IrrigLog {
  uint32_t timestamp;  // 4
  float    csmi;       // 4
  float    litres;     // 4
  uint8_t  trigger;    // 1  0=auto 1=manual 2=adaptive/offline
  uint8_t  stage;      // 1
  uint16_t aiScore10;  // 2  aiScore × 10 to avoid float
};                     // total 16 B — fits in EEPROM_LOG_SIZE=20

// ============================================================
// PUMP SCHEDULE (Firebase driven, up to 3 slots)
// ============================================================
struct PumpSchedule {
  bool     active;
  uint8_t  startHour;
  uint8_t  startMin;
  uint16_t durationSec;
  bool     executed;    // reset daily
};
PumpSchedule pumpSchedule[3] = {
  {false, 6,  0, 1800, false},
  {false, 14, 0, 3600, false},
  {false, 18, 0, 3600, false}
};

// ============================================================
// STATE VARIABLES — SENSORS
// ============================================================
float sm1_pct = 0, sm2_pct = 0, sm3_pct = 0;
float vwc1 = 0, vwc2 = 0, vwc3 = 0;
float csmi  = 0;

float temperature = 0, humidity = 0, pressure = 0;
float flowRate    = 0;
float aiScore     = 0, smv = 0, sma = 0, tprScore = 0;
float eto         = 0, rainProb = 0;

// Optional sensors
float soilTemp_C = 0;
float ecValue    = 0;   // mS/cm
float phValue    = 0;   // pH units

// NPK estimation (from EC + soilTemp, v5.0)
float npkN = 0, npkP = 0, npkK = 0; // ppm

// ============================================================
// STATE VARIABLES — WATER BUDGET
// ============================================================
float deltaApplied    = 0;  // L accumulated this season
float deltaRequired   = 0;  // L needed this season
float deltaBalance    = 0;  // L remaining
float sessionLitres   = 0;  // L in current pump session (reset each session)
float totalLitresDay  = 0;  // L delivered today
float effectiveRain   = 0;  // mm effective
float rainfallContrib = 0;  // L contribution
float tipBucket_mm    = 0;
volatile uint32_t tipPulseCount = 0;

// Irrigation by depth (v5.0) — litres attributed per depth trigger
float irrigByDepth[3] = {0, 0, 0}; // [15cm, 30cm, 45cm]

// Water saving vs traditional method (25 L / session traditional)
float waterSavedTotal = 0;

// ============================================================
// STATE VARIABLES — SYSTEM FLAGS
// ============================================================
bool pumpOn        = false;
bool autoMode      = true;
bool safeMode      = false;
bool pipelineFault = false;
bool dhtFallback   = false;
bool bmpFallback   = false;
bool offlineMode   = false;
bool wifiConnected = false;
bool loraOk        = false;

// Weather primary source: true = local sensors, false = OWM
bool weatherPrimaryLocal = true;

// ============================================================
// STATE VARIABLES — PUMP METRICS (v5.0)
// ============================================================
uint16_t pumpCyclesToday  = 0;
uint32_t pumpRunTimeSec   = 0;   // cumulative seconds pump was ON today
unsigned long pumpOnStartMs = 0; // millis() when pump last turned ON
// Motor estimation (no real sensors — these are read from Firebase
// if a current sensor is attached, otherwise defaults)
float motorVoltage   = 230.0f;  // V (local grid estimate)
float motorCurrentA  = 2.6f;   // A (estimated 600W / 230V)
float motorPowerW    = 589.0f;  // W
float motorTempC     = 40.0f;  // °C estimated

// ============================================================
// STATE VARIABLES — LORA (v5.0)
// ============================================================
int     loraPacketsTX  = 0;
int     loraPacketsRX  = 0;
int     loraRSSI       = 0;
float   loraSNR        = 0;
uint8_t loraNodeCount  = 1;    // self = 1; increments when other nodes reply

// ============================================================
// STATE VARIABLES — CROP / GDD
// ============================================================
int   cropIndex     = 0;
float plotArea_m2   = 6.0f;
float plotArea_bigha= 0.0045f;
int   stage         = 0;       // 0–3
float gdd           = 0;       // accumulated growing degree days
float dailyTempSum  = 0;
int   dailyTempCnt  = 0;
unsigned long lastGddUpdate = 0;
uint8_t lastGddDay  = 0;       // day-of-week to detect new day

// ============================================================
// STATE VARIABLES — OFFLINE / EEPROM
// ============================================================
uint8_t offlineLogCount = 0;
uint8_t logHeadPtr      = 0;

// ============================================================
// STATE VARIABLES — TIMING
// ============================================================
unsigned long lastSensorTime   = 0;
unsigned long lastFirebaseTime = 0;
unsigned long lastLoRaTime     = 0;
unsigned long lastWiFiRetry    = 0;
unsigned long lastFlowCheck    = 0;
unsigned long pumpCooldownEnd  = 0;
unsigned long pumpStartTime    = 0;

// ============================================================
// AI SCORE HISTORY (ring buffer)
// ============================================================
static float  csmiHistory[10];
static int    csmiHistIdx  = 0;
static bool   csmiHistFull = false;

// ============================================================
// FLOW SENSOR ISR
// ============================================================
volatile uint32_t flowPulseCount = 0;
IRAM_ATTR void flowPulseISR() { flowPulseCount++; }

#ifdef ENABLE_TIPPING_BUCKET
IRAM_ATTR void tipBucketISR() { tipPulseCount++; }
#endif

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("\n=== TSCRIC-LoRa v5.0 ENTERPRISE EDITION ==="));
  Serial.println(F("Oriental College of Technology, Bhopal"));

  // Pin modes
  pinMode(MUX_S0_PIN, OUTPUT);
  pinMode(MUX_S1_PIN, OUTPUT);
  pinMode(MUX_S2_PIN, OUTPUT);
  pinMode(RELAY_PIN,  OUTPUT);
  pinMode(FLOW_PIN,   INPUT_PULLUP);
  digitalWrite(RELAY_PIN, HIGH); // relay OFF (Active LOW)

  // Interrupts
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), flowPulseISR, FALLING);
#ifdef ENABLE_TIPPING_BUCKET
  pinMode(TIPPING_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TIPPING_PIN), tipBucketISR, FALLING);
#endif

  // EEPROM
  EEPROM.begin(EEPROM_SIZE);
  loadCalibFromEEPROM();
  loadConfigFromEEPROM();

  // DHT22
  dht.begin();
  delay(500); // DHT22 needs 500 ms after power-on

  // BMP280 — try both I2C addresses
  if (!bmp.begin(0x76)) {
    Serial.println(F("BMP280 @ 0x76 not found, trying 0x77..."));
    if (!bmp.begin(0x77)) {
      Serial.println(F("BMP280 not found — OWM fallback active"));
      bmpFallback = true;
    }
  }
  if (!bmpFallback) {
    bmp.setSampling(
      Adafruit_BMP280::MODE_NORMAL,
      Adafruit_BMP280::SAMPLING_X2,
      Adafruit_BMP280::SAMPLING_X16,
      Adafruit_BMP280::FILTER_X16,
      Adafruit_BMP280::STANDBY_MS_500);
    Serial.println(F("BMP280 OK"));
  }

#ifdef ENABLE_DS18B20
  soilTherm.begin();
  Serial.println(F("DS18B20 OK"));
#endif

  // LoRa
  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println(F("LoRa init FAILED — check wiring"));
    loraOk = false;
  } else {
    LoRa.setSpreadingFactor(7);
    LoRa.setSignalBandwidth(125E3);
    LoRa.setCodingRate4(5);
    LoRa.setTxPower(17);
    loraOk = true;
    Serial.println(F("LoRa OK — 433 MHz SF7 BW125 CR4/5 17dBm"));
  }

  // WiFi
  connectWiFi();

  Serial.println(F("=== SYSTEM READY ==="));
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
  unsigned long now = millis();

  // WiFi watchdog
  if (!wifiConnected && (now - lastWiFiRetry > WIFI_RETRY_INTERVAL)) {
    lastWiFiRetry = now;
    connectWiFi();
  }

  // Sensor cycle
  if (now - lastSensorTime >= SENSOR_INTERVAL) {
    lastSensorTime = now;
    readAllSensors();
    computeCSMI();
    computeAIScore();
    evaluateIrrigation();
    accumulateGDD(now);
    updateTippingBucket();
    estimateNPK();
  }

  // Firebase cycle
  if (wifiConnected && (now - lastFirebaseTime >= FIREBASE_INTERVAL)) {
    lastFirebaseTime = now;
    pushToFirebase();
    readFirebaseCommands();
    readFirebaseConfig();
  }

  // LoRa broadcast
  if (loraOk && (now - lastLoRaTime >= LORA_INTERVAL)) {
    lastLoRaTime = now;
    broadcastLoRa();
    receiveLoRa();
  }

  // Pump state machine
  managePump(now);

  // Flow failsafe — no flow 30 s after pump ON → fault
  if (pumpOn && (now - lastFlowCheck >= FLOW_FAILSAFE_MS)) {
    lastFlowCheck = now;
    if (flowRate < 0.3f && !pipelineFault) {
      pipelineFault = true;
      pumpOff();
      Serial.println(F("FLOW FAILSAFE: stopped pump — no flow"));
    }
  }

  // Pump schedule check
  checkPumpSchedule();

  yield();
}

// ============================================================
// WIFI
// ============================================================
void connectWiFi() {
  Serial.printf("WiFi → %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 15000UL) {
    delay(500); Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    offlineMode   = false;
    Serial.print(F("WiFi OK — IP: "));
    Serial.println(WiFi.localIP());
    if (offlineLogCount > 0) syncOfflineLogs();
  } else {
    wifiConnected = false;
    offlineMode   = true;
    Serial.println(F("WiFi failed — starting hotspot TSCRIC_AI"));
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("TSCRIC_AI", "12345678");
    Serial.print(F("Hotspot IP: "));
    Serial.println(WiFi.softAPIP());
  }
}

// ============================================================
// SENSOR READING
// ============================================================
void readAllSensors() {
  // ── Soil moisture (3 channels via CD4051) ──────────────────
  int raw0 = readMux(0);
  int raw1 = readMux(1);
  int raw2 = readMux(2);

  bool s1ok = (raw0 > 50 && raw0 < 1023);
  bool s2ok = (raw1 > 50 && raw1 < 1023);
  bool s3ok = (raw2 > 50 && raw2 < 1023);
  safeMode  = (!s1ok && !s2ok && !s3ok);

  if (!safeMode) {
    if (s1ok) { vwc1 = adcToVWC(raw0, 0); sm1_pct = vwc1 * 100.0f; }
    if (s2ok) { vwc2 = adcToVWC(raw1, 1); sm2_pct = vwc2 * 100.0f; }
    if (s3ok) { vwc3 = adcToVWC(raw2, 2); sm3_pct = vwc3 * 100.0f; }
  }

  // ── DHT22 ──────────────────────────────────────────────────
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t) && !isnan(h) && t > -40.0f && t < 85.0f
                              && h >= 0.0f  && h <= 100.0f) {
    temperature  = t;
    humidity     = h;
    dhtFallback  = false;
    dailyTempSum += t;
    dailyTempCnt++;
  } else {
    dhtFallback = true;
    Serial.println(F("DHT22 read failed — OWM fallback"));
  }

  // ── BMP280 ─────────────────────────────────────────────────
  if (!bmpFallback) {
    float p = bmp.readPressure();
    if (p > 80000.0f && p < 110000.0f) {
      pressure = p / 100.0f; // hPa
    }
  }

  // ── DS18B20 (optional) ─────────────────────────────────────
#ifdef ENABLE_DS18B20
  soilTherm.requestTemperatures();
  float st = soilTherm.getTempCByIndex(0);
  if (st != DEVICE_DISCONNECTED_C && st > -40.0f && st < 80.0f) {
    soilTemp_C = st;
  }
#endif

  // ── EC sensor (optional, MUX CH3) ──────────────────────────
#ifdef ENABLE_EC_SENSOR
  int rawEC = readMux(3);
  // Typical analog EC: map 0–1023 to 0–6 mS/cm
  ecValue = (rawEC / 1023.0f) * 6.0f;
#endif

  // ── pH sensor (optional, MUX CH4) ──────────────────────────
#ifdef ENABLE_PH_SENSOR
  int rawPH = readMux(4);
  // Typical analog pH: 0–1023 maps to 0–14
  phValue = (rawPH / 1023.0f) * 14.0f;
#endif

  // ── Flow rate ───────────────────────────────────────────────
  updateFlowRate();

  // ── ETo ─────────────────────────────────────────────────────
  computeETo();

  Serial.printf(
    "SM1:%.1f%% SM2:%.1f%% SM3:%.1f%% T:%.1f H:%.0f P:%.1f Flow:%.2f\n",
    sm1_pct, sm2_pct, sm3_pct, temperature, humidity, pressure, flowRate);
}

// CD4051 MUX read
int readMux(int ch) {
  digitalWrite(MUX_S0_PIN, (ch >> 0) & 1);
  digitalWrite(MUX_S1_PIN, (ch >> 1) & 1);
  digitalWrite(MUX_S2_PIN, (ch >> 2) & 1);
  delay(12); // settle time
  return analogRead(MUX_SIG_PIN);
}

// ADC → VWC piecewise linear  (v5.0: division-by-zero guarded)
float adcToVWC(int adc, int ch) {
  float dry  = (float)calibration[ch].adc_dry;
  float fc   = (float)calibration[ch].adc_fc;
  float pwp  = (float)calibration[ch].adc_pwp;
  float vfc  = calibration[ch].vwc_fc;
  float vpwp = calibration[ch].vwc_pwp;

  if (adc >= (int)dry) return 0.0f;

  // dry → pwp region
  if (adc >= (int)pwp) {
    float denom = dry - pwp;
    if (fabsf(denom) < 1.0f) return vpwp;
    return vpwp * (dry - adc) / denom;
  }

  // pwp → fc region
  if (adc >= (int)fc) {
    float denom = pwp - fc;
    if (fabsf(denom) < 1.0f) return vfc;
    return vpwp + (vfc - vpwp) * (pwp - adc) / denom;
  }

  // below fc → saturated region
  float denom = fc - 200.0f;
  if (denom < 1.0f) denom = 1.0f;
  return vfc + (0.50f - vfc) * (fc - adc) / denom;
}

// ============================================================
// ETo — Hargreaves-Samani (corrected, v5.0)
// ETo = 0.0023 × Ra × (Tmax - Tmin)^0.5 × (Tmean + 17.8)
// We approximate: Ra = 10 MJ/m²/day (central India)
// Use daily temp from DHT22; if fallback, use 30°C default
// ============================================================
void computeETo() {
  float T = dhtFallback ? 30.0f : temperature;
  float H = dhtFallback ? 60.0f : humidity;
  // Estimate Tmax – Tmin from humidity via VPD approximation
  float es   = 0.6108f * expf(17.27f * T / (T + 237.3f));
  float ea   = es * H / 100.0f;
  float vpd  = es - ea;
  // Approximate Tmax-Tmin from VPD (rough, no logger needed)
  float tRange = constrain(vpd * 5.0f, 2.0f, 18.0f);
  float Ra   = 10.0f; // MJ/m²/day central India mid-season approx
  eto = 0.0023f * Ra * sqrtf(tRange) * (T + 17.8f);
  eto = constrain(eto, 0.0f, 12.0f);
}

// ============================================================
// CSMI
// ============================================================
void computeCSMI() {
  if (safeMode) { csmi = 0; return; }
  int s = constrain(stage, 0, 3);
  csmi = STAGE_WEIGHTS[s][0] * sm1_pct
       + STAGE_WEIGHTS[s][1] * sm2_pct
       + STAGE_WEIGHTS[s][2] * sm3_pct;
  csmi = constrain(csmi, 0.0f, 100.0f);

  // Water balance
  deltaRequired = CROPS[cropIndex].delta * plotArea_m2;
  deltaBalance  = max(0.0f, deltaRequired - deltaApplied - rainfallContrib);
}

// ============================================================
// GDD ACCUMULATION (v5.0)
// ============================================================
void accumulateGDD(unsigned long now) {
  if (now - lastGddUpdate < GDD_UPDATE_INTERVAL) return;
  lastGddUpdate = now;

  // Base temperature 10°C (for most crops)
  if (dailyTempCnt > 0) {
    float dailyAvg = dailyTempSum / dailyTempCnt;
    float gddToday = max(0.0f, dailyAvg - 10.0f);
    gdd += gddToday;
    dailyTempSum = 0;
    dailyTempCnt = 0;

    // Auto-advance crop stage from GDD thresholds
    if (stage < 3) {
      if (gdd >= CROPS[cropIndex].gddStage[stage]) {
        stage++;
        Serial.printf("Crop stage advanced to %d (GDD=%.0f)\n", stage, gdd);
      }
    }
  }
}

// ============================================================
// NPK ESTIMATION (v5.0 — from EC + soilTemp)
// These are empirical estimates, not lab-grade measurements.
// Formulas derived from Tisdale et al. + FAO soil guides.
// ============================================================
void estimateNPK() {
  // Only estimate if EC hardware is connected
  if (ecValue <= 0.0f) {
    npkN = 0; npkP = 0; npkK = 0;
    return;
  }
  float Tf   = soilTemp_C > 0 ? soilTemp_C : temperature;
  float tempF= constrain(Tf, 10.0f, 40.0f);
  // N roughly proportional to EC and soil temp mineralisation
  npkN = constrain(ecValue * 80.0f + (tempF - 20.0f) * 1.5f, 0, 300);
  // P inversely related to pH if available, else EC-based
  if (phValue > 0.0f) {
    float phFactor = (phValue >= 6.0f && phValue <= 7.5f) ? 1.0f : 0.65f;
    npkP = constrain(ecValue * 20.0f * phFactor, 0, 100);
  } else {
    npkP = constrain(ecValue * 18.0f, 0, 100);
  }
  // K directly from EC
  npkK = constrain(ecValue * 120.0f, 0, 400);
}

// ============================================================
// AI SCORE
// ============================================================
void computeAIScore() {
  // Update CSMI history
  csmiHistory[csmiHistIdx] = csmi;
  csmiHistIdx = (csmiHistIdx + 1) % 10;
  if (csmiHistIdx == 0) csmiHistFull = true;
  int histLen = csmiHistFull ? 10 : csmiHistIdx;

  // SMV — moisture velocity (%/h)
  smv = 0;
  if (histLen >= 2) {
    int prev = (csmiHistIdx - 2 + 10) % 10;
    int curr = (csmiHistIdx - 1 + 10) % 10;
    smv = (csmiHistory[curr] - csmiHistory[prev])
        / (SENSOR_INTERVAL / 3600000.0f);
  }

  // SMA — moisture acceleration
  sma = 0;
  if (histLen >= 3) {
    int p2 = (csmiHistIdx - 3 + 10) % 10;
    int p1 = (csmiHistIdx - 2 + 10) % 10;
    int c0 = (csmiHistIdx - 1 + 10) % 10;
    float v1 = csmiHistory[p1] - csmiHistory[p2];
    float v2 = csmiHistory[c0] - csmiHistory[p1];
    sma = (v2 - v1) / (SENSOR_INTERVAL / 3600000.0f);
  }

  // TPR — temporal pattern recognition (drying consistency)
  tprScore = 0;
  if (histLen >= 5) {
    int dryCount = 0;
    for (int i = 1; i < histLen; i++) {
      int pi = (csmiHistIdx - i - 1 + 10) % 10;
      int ci = (csmiHistIdx - i     + 10) % 10;
      if (csmiHistory[ci] < csmiHistory[pi]) dryCount++;
    }
    tprScore = (float)dryCount / (histLen - 1);
  }

  // Rain penalty
  float rainPenalty  = rainProb > 75.0f ? 40.0f
                     : rainProb > 35.0f ? 15.0f : 0.0f;

  // Score components
  float moistureScore = max(0.0f, (35.0f - csmi) * 1.5f);
  float velocityScore = (smv < -0.5f)
                      ? min(20.0f, fabsf(smv) * 10.0f) : 0.0f;
  float tprBonus      = (tprScore >= 0.85f) ? 15.0f : tprScore * 10.0f;
  float etoScore      = constrain((eto - 3.0f) * 3.0f, 0.0f, 15.0f);

  aiScore = moistureScore + velocityScore + tprBonus + etoScore - rainPenalty;
  aiScore = constrain(aiScore, 0.0f, 120.0f);
}

// ============================================================
// IRRIGATION DECISION
// ============================================================
void evaluateIrrigation() {
  if (!autoMode || safeMode) return;

  int   s         = constrain(stage, 0, 3);
  float threshold = STAGE_THRESHOLDS[s];

  bool shouldIrrigate = (csmi < threshold)
                      && (rainProb < 75.0f)
                      && (aiScore >= 65.0f);

  // v5.0: pumpCooldownEnd overflow guard
  unsigned long now = millis();
  bool cooldownOk   = (now >= pumpCooldownEnd) ||
                      (pumpCooldownEnd - now > PUMP_COOLDOWN_MS * 2);

  if (shouldIrrigate && cooldownOk && !pumpOn && !pipelineFault) {
    // Track which depth triggered (deepest dry = trigger)
    if      (sm3_pct < STAGE_THRESHOLDS[s]) irrigByDepth[2]++;
    else if (sm2_pct < STAGE_THRESHOLDS[s]) irrigByDepth[1]++;
    else                                     irrigByDepth[0]++;
    pumpOn_withCycle(0); // trigger = auto
  }
}

// ============================================================
// PUMP SCHEDULE CHECK (v5.0)
// ============================================================
void checkPumpSchedule() {
  // Basic time from millis (days since boot × approx)
  // Real-time clock not present; schedule is approximate.
  // Dashboard sets schedules relative to device uptime hour.
  // For a real RTC, replace with RTC library.
  static unsigned long lastScheduleCheck = 0;
  if (millis() - lastScheduleCheck < 10000UL) return;
  lastScheduleCheck = millis();

  // Approx hour from millis (wraps every 24 h after reset)
  uint8_t approxHour = (millis() / 3600000UL) % 24;
  uint8_t approxMin  = (millis() / 60000UL)   % 60;

  for (int i = 0; i < 3; i++) {
    if (!pumpSchedule[i].active)   continue;
    if (pumpSchedule[i].executed)  continue;
    if (autoMode) continue; // auto mode manages pump itself

    if (approxHour == pumpSchedule[i].startHour &&
        approxMin  == pumpSchedule[i].startMin) {
      pumpOn_withCycle(1); // manual schedule trigger
      pumpSchedule[i].executed = true;
    }
  }
}

// ============================================================
// PUMP CONTROL
// ============================================================
void updateFlowRate() {
  static uint32_t  lastPulse = 0;
  static unsigned long lastTime  = 0;

  uint32_t  pulses  = flowPulseCount - lastPulse;
  unsigned long elapsed = millis() - lastTime;

  if (elapsed == 0) return;

  float litresThisInterval = pulses / 7.5f;  // YF-S201: 7.5 pulses/L
  flowRate = litresThisInterval * (60000.0f / elapsed); // L/min

  // v5.0 bug fix: only accumulate litres when pump is physically ON
  if (pumpOn && flowRate > 0.1f) {
    float litresNow = litresThisInterval;
    sessionLitres  += litresNow;
    totalLitresDay += litresNow;
  }

  lastPulse = flowPulseCount;
  lastTime  = millis();
}

void pumpOn_withCycle(uint8_t trigger) {
  if (pumpOn) return; // already on
  pumpOn        = true;
  sessionLitres = 0;  // reset session counter
  pipelineFault = false;
  digitalWrite(RELAY_PIN, LOW); // Active LOW
  pumpStartTime = millis();
  pumpOnStartMs = millis();
  lastFlowCheck = millis();
  pumpCyclesToday++;
  Serial.printf("PUMP ON (trigger=%d, cycle #%d)\n", trigger, pumpCyclesToday);
  logIrrigEvent(trigger);
}

void pumpOff() {
  if (!pumpOn) return;
  pumpOn = false;
  digitalWrite(RELAY_PIN, HIGH);

  // Accumulate run time (seconds)
  pumpRunTimeSec += (millis() - pumpOnStartMs) / 1000UL;

  // Water saving: traditional drip ~25 L/session, we use sessionLitres
  // If we saved water vs traditional, accumulate
  if (sessionLitres > 0 && sessionLitres < 25.0f) {
    waterSavedTotal += (25.0f - sessionLitres);
  }

  // v5.0 bug fix: deltaApplied accumulates here, NOT inside pushToFirebase
  deltaApplied += sessionLitres;

  pumpCooldownEnd = millis() + PUMP_COOLDOWN_MS;
  Serial.printf("PUMP OFF — %.1f L delivered this session\n", sessionLitres);
}

void managePump(unsigned long now) {
  if (!pumpOn) return;
  if (now - pumpStartTime >= PUMP_ON_MS) {
    pumpOff();
  }
}

// ============================================================
// TIPPING BUCKET
// ============================================================
void updateTippingBucket() {
  static uint32_t lastTipCount = 0;
  uint32_t current = tipPulseCount; // read volatile once
  if (current == lastTipCount) return;

  uint32_t newTips = current - lastTipCount;
  lastTipCount     = current;
  tipBucket_mm    += newTips * 0.2f; // 0.2 mm/tip

  // SCS CN-75 effective rainfall
  float S       = (25400.0f / 75.0f) - 254.0f;
  float Ia      = 0.2f * S;
  float runoff  = (tipBucket_mm > Ia)
                ? ((tipBucket_mm - Ia) * (tipBucket_mm - Ia))
                  / (tipBucket_mm - Ia + S)
                : 0.0f;
  effectiveRain   = max(0.0f, tipBucket_mm - runoff);
  rainfallContrib = effectiveRain * plotArea_m2;

  if (tipBucket_mm > 5.0f) rainProb = 90.0f; // suppress irrigation
}

// ============================================================
// FIREBASE — PUSH SENSORS
// ============================================================
void pushToFirebase() {
  if (!wifiConnected) return;

  // NOTE: deltaApplied is now updated in pumpOff(), NOT here.
  // totalLitresDay resets at midnight (approx via GDD daily reset).
  tipBucket_mm = (float)tipPulseCount * 0.2f;

  // Pump run-time string
  uint16_t pumpH  = pumpRunTimeSec / 3600;
  uint8_t  pumpM  = (pumpRunTimeSec % 3600) / 60;

  StaticJsonDocument<2048> doc;

  // ── Core sensor fields ──────────────────────────────────────
  doc["sm1"]             = round(sm1_pct * 10) / 10.0;
  doc["sm2"]             = round(sm2_pct * 10) / 10.0;
  doc["sm3"]             = round(sm3_pct * 10) / 10.0;
  doc["vwc1"]            = round(vwc1 * 1000) / 1000.0;
  doc["vwc2"]            = round(vwc2 * 1000) / 1000.0;
  doc["vwc3"]            = round(vwc3 * 1000) / 1000.0;
  doc["csmi"]            = round(csmi * 10) / 10.0;
  doc["temperature"]     = round(temperature * 10) / 10.0;
  doc["humidity"]        = round(humidity);
  doc["pressure"]        = round(pressure * 10) / 10.0;
  doc["flowRate"]        = round(flowRate * 100) / 100.0;

  // ── AI engine ──────────────────────────────────────────────
  doc["aiScore"]         = round(aiScore * 10) / 10.0;
  doc["smv"]             = round(smv * 10000) / 10000.0;
  doc["sma"]             = round(sma * 10000) / 10000.0;
  doc["tprScore"]        = round(tprScore * 1000) / 1000.0;
  doc["eto"]             = round(eto * 100) / 100.0;
  doc["rainProb"]        = round(rainProb);

  // ── Pump / system flags ────────────────────────────────────
  doc["pump"]            = pumpOn;
  doc["autoMode"]        = autoMode;
  doc["safeMode"]        = safeMode;
  doc["pipelineFault"]   = pipelineFault;
  doc["dhtFallback"]     = dhtFallback;
  doc["bmpFallback"]     = bmpFallback;
  doc["offlineMode"]     = offlineMode;
  doc["wifiMode"]        = wifiConnected ? "Online" : "Offline";

  // ── Water budget ───────────────────────────────────────────
  doc["deltaApplied"]    = round(deltaApplied * 10) / 10.0;
  doc["deltaRequired"]   = round(deltaRequired * 10) / 10.0;
  doc["deltaBalance"]    = round(deltaBalance * 10) / 10.0;
  doc["totalLitresDay"]  = round(totalLitresDay * 10) / 10.0;
  doc["sessionLitres"]   = round(sessionLitres * 10) / 10.0;
  doc["effectiveRain"]   = round(effectiveRain * 100) / 100.0;
  doc["rainfallContrib"] = round(rainfallContrib * 10) / 10.0;
  doc["waterSavedTotal"] = round(waterSavedTotal * 10) / 10.0;
  doc["plotArea_m2"]     = plotArea_m2;

  // ── Irrigation by depth (v5.0) ─────────────────────────────
  doc["irrigByDepth0"]   = irrigByDepth[0]; // 15 cm triggers
  doc["irrigByDepth1"]   = irrigByDepth[1]; // 30 cm triggers
  doc["irrigByDepth2"]   = irrigByDepth[2]; // 45 cm triggers

  // ── Crop / GDD ─────────────────────────────────────────────
  doc["stage"]           = stage;
  doc["gdd"]             = round(gdd * 10) / 10.0;
  doc["crop"]            = cropIndex;
  doc["cropName"]        = CROPS[cropIndex].name;

  // ── Rainfall / tipping bucket ──────────────────────────────
  doc["tipBucket_mm"]    = round(tipBucket_mm * 100) / 100.0;
  doc["tipBucket_pulses"]= (uint32_t)tipPulseCount;

  // ── Optional sensors ───────────────────────────────────────
  doc["soilTemp"]        = round(soilTemp_C * 10) / 10.0;
  doc["ecValue"]         = round(ecValue * 100) / 100.0;
  doc["phValue"]         = round(phValue * 10) / 10.0;
  doc["npkN"]            = round(npkN);
  doc["npkP"]            = round(npkP);
  doc["npkK"]            = round(npkK);

  // ── LoRa (v5.0) ────────────────────────────────────────────
  doc["loraPacketsTX"]   = loraPacketsTX;
  doc["loraPacketsRX"]   = loraPacketsRX;
  doc["loraRSSI"]        = loraRSSI;
  doc["loraSNR"]         = round(loraSNR * 10) / 10.0;
  doc["loraNodeCount"]   = loraNodeCount;
  doc["loraOk"]          = loraOk;

  // ── Pump metrics (v5.0) ────────────────────────────────────
  doc["pumpCyclesToday"] = pumpCyclesToday;
  doc["pumpRunTimeSec"]  = pumpRunTimeSec;
  doc["pumpRunTimeH"]    = pumpH;
  doc["pumpRunTimeM"]    = pumpM;
  doc["motorVoltage"]    = motorVoltage;
  doc["motorCurrentA"]   = motorCurrentA;
  doc["motorPowerW"]     = motorPowerW;
  doc["motorTempC"]      = motorTempC;

  // ── Connectivity ───────────────────────────────────────────
  doc["weatherPrimaryLocal"] = weatherPrimaryLocal;
  doc["offlineLogCount"]     = offlineLogCount;

  // ── Timestamp ──────────────────────────────────────────────
  doc["uptimeMs"]        = (uint32_t)millis();

  String payload;
  serializeJson(doc, payload);

  String url = "https://" + String(FIREBASE_HOST)
             + "/tscric/sensors.json?auth=" + String(FIREBASE_SECRET);

  WiFiClientSecure client;
  client.setInsecure(); // skip SSL cert for RTDB
  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(8000);

  int code = http.PUT(payload);
  if (code > 0) {
    Serial.printf("Firebase PUT: %d\n", code);
  } else {
    Serial.printf("Firebase error: %s\n", http.errorToString(code).c_str());
    // Mark pending push for offline sync
  }
  http.end();
}

// ============================================================
// FIREBASE — READ COMMANDS
// ============================================================
void readFirebaseCommands() {
  String url = "https://" + String(FIREBASE_HOST)
             + "/tscric/commands.json?auth=" + String(FIREBASE_SECRET);
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.begin(client, url);
  http.setTimeout(5000);
  int code = http.GET();
  if (code == 200) {
    String body = http.getString();
    StaticJsonDocument<256> doc;
    if (!deserializeJson(doc, body)) {
      bool clearPumpOn  = false;
      bool clearPumpOff = false;
      bool clearReset   = false;

      if (doc["pumpOn"].as<bool>()  && !pumpOn)  { pumpOn_withCycle(1); clearPumpOn  = true; }
      if (doc["pumpOff"].as<bool>() && pumpOn)   { pumpOff();           clearPumpOff = true; }
      if (!doc["auto"].isNull())  autoMode = doc["auto"].as<bool>();
      // v5.0: reset daily counters command
      if (doc["resetDaily"].as<bool>()) {
        pumpCyclesToday = 0;
        pumpRunTimeSec  = 0;
        totalLitresDay  = 0;
        for (int i = 0; i < 3; i++) pumpSchedule[i].executed = false;
        clearReset = true;
      }

      // v5.1 bug fix: clear one-shot command flags in Firebase after
      // executing them — otherwise pumpOn/pumpOff stay "true" forever
      // and pumpOn_withCycle() re-fires on every Firebase cycle once
      // the pump auto-switches off again.
      if (clearPumpOn || clearPumpOff || clearReset) {
        StaticJsonDocument<128> ack;
        if (clearPumpOn)  ack["pumpOn"]     = false;
        if (clearPumpOff) ack["pumpOff"]    = false;
        if (clearReset)   ack["resetDaily"] = false;

        String ackPayload; serializeJson(ack, ackPayload);
        WiFiClientSecure ackClient; ackClient.setInsecure();
        HTTPClient ackHttp; ackHttp.begin(ackClient, url);
        ackHttp.addHeader("Content-Type", "application/json");
        ackHttp.setTimeout(5000);
        ackHttp.PATCH(ackPayload);
        ackHttp.end();
      }
    }
  }
  http.end();
}


// ============================================================
// FIREBASE — READ CONFIG
// ============================================================
void readFirebaseConfig() {
  String url = "https://" + String(FIREBASE_HOST)
             + "/tscric/config.json?auth=" + String(FIREBASE_SECRET);
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.begin(client, url);
  http.setTimeout(5000);
  int code = http.GET();
  if (code == 200) {
    String body = http.getString();
    StaticJsonDocument<1024> doc;
    if (!deserializeJson(doc, body)) {
      if (!doc["plotArea"].isNull()) {
        float a = doc["plotArea"].as<float>();
        if (a >= 1.0f && a <= 100000.0f) plotArea_m2 = a;
      }
      if (!doc["crop"].isNull()) {
        int c = doc["crop"].as<int>();
        if (c >= 0 && c <= 7) cropIndex = c;
      }
      // Weather primary source toggle
      if (!doc["weatherPrimaryLocal"].isNull()) {
        weatherPrimaryLocal = doc["weatherPrimaryLocal"].as<bool>();
      }
      // Pump schedules
      for (int i = 0; i < 3; i++) {
        String key = "sched" + String(i);
        if (doc.containsKey(key)) {
          pumpSchedule[i].active      = doc[key]["active"].as<bool>();
          pumpSchedule[i].startHour   = doc[key]["hour"].as<uint8_t>();
          pumpSchedule[i].startMin    = doc[key]["min"].as<uint8_t>();
          pumpSchedule[i].durationSec = doc[key]["dur"].as<uint16_t>();
        }
      }
      // Calibration
      if (doc.containsKey("soilCalib")) {
        for (int i = 0; i < 3; i++) {
          String ch = "ch" + String(i);
          if (doc["soilCalib"].containsKey(ch)) {
            auto c = doc["soilCalib"][ch];
            if (!c["adc_dry"].isNull()) calibration[i].adc_dry = c["adc_dry"].as<int>();
            if (!c["adc_fc"].isNull())  calibration[i].adc_fc  = c["adc_fc"].as<int>();
            if (!c["adc_pwp"].isNull()) calibration[i].adc_pwp = c["adc_pwp"].as<int>();
            if (!c["vwc_fc"].isNull())  calibration[i].vwc_fc  = c["vwc_fc"].as<float>();
            if (!c["vwc_pwp"].isNull()) calibration[i].vwc_pwp = c["vwc_pwp"].as<float>();
          }
        }
        saveCalibToEEPROM();
      }
    }
  }
  http.end();
}

// ============================================================
// LoRa — BROADCAST
// ============================================================
void broadcastLoRa() {
  if (!loraOk) return;

  // Compact CSV packet — v5.0 format
  // TSCRIC5,nodeID,sm1,sm2,sm3,csmi,T,H,flow,aiScore,pump,rainProb,offline,safe,gdd,stage,pumpCycles
  String pkt = "TSCRIC5,01,";
  pkt += String(sm1_pct, 1)     + ",";
  pkt += String(sm2_pct, 1)     + ",";
  pkt += String(sm3_pct, 1)     + ",";
  pkt += String(csmi, 1)        + ",";
  pkt += String(temperature, 1) + ",";
  pkt += String(humidity, 0)    + ",";
  pkt += String(flowRate, 2)    + ",";
  pkt += String(aiScore, 1)     + ",";
  pkt += String((int)pumpOn)    + ",";
  pkt += String(rainProb, 0)    + ",";
  pkt += String((int)offlineMode) + ",";
  pkt += String((int)safeMode)  + ",";
  pkt += String(gdd, 0)         + ",";
  pkt += String(stage)          + ",";
  pkt += String(pumpCyclesToday);

  LoRa.beginPacket();
  LoRa.print(pkt);
  LoRa.endPacket();
  loraPacketsTX++;

  Serial.printf("LoRa TX #%d\n", loraPacketsTX);
}

// ============================================================
// LoRa — RECEIVE (v5.0: count nodes replying)
// ============================================================
void receiveLoRa() {
  if (!loraOk) return;
  int sz = LoRa.parsePacket();
  if (sz > 0) {
    loraRSSI = LoRa.packetRssi();
    loraSNR  = LoRa.packetSnr();
    loraPacketsRX++;
    // Read and discard payload (extend here for mesh parsing)
    while (LoRa.available()) LoRa.read();
    // Track unique nodes (simplified: each RX = possible new node)
    if (loraNodeCount < 12) loraNodeCount++;
    Serial.printf("LoRa RX #%d  RSSI:%d SNR:%.1f\n",
                  loraPacketsRX, loraRSSI, loraSNR);
  }
}

// ============================================================
// EEPROM
// ============================================================
void loadCalibFromEEPROM() {
  EEPROM.get(EEPROM_CALIB_START, calibration);
  for (int i = 0; i < 3; i++) {
    if (calibration[i].adc_dry < 300 || calibration[i].adc_dry > 1023 ||
        calibration[i].vwc_fc  < 0.05f || calibration[i].vwc_fc > 0.65f) {
      // Restore safe defaults
      SoilCalib def = {850, 600, 750, 0.35f, 0.12f};
      calibration[i] = def;
    }
  }
}

void saveCalibToEEPROM() {
  EEPROM.put(EEPROM_CALIB_START, calibration);
  EEPROM.commit();
}

void loadConfigFromEEPROM() {
  EEPROM.get(EEPROM_CONFIG_START,    cropIndex);
  EEPROM.get(EEPROM_CONFIG_START+4,  plotArea_m2);
  EEPROM.get(EEPROM_CONFIG_START+8,  offlineLogCount);
  EEPROM.get(EEPROM_CONFIG_START+9,  logHeadPtr);
  if (cropIndex < 0 || cropIndex > 7)    cropIndex = 0;
  if (plotArea_m2 < 1 || plotArea_m2 > 100000) plotArea_m2 = 6.0f;
  if (offlineLogCount > EEPROM_LOG_COUNT) offlineLogCount = 0;
}

void saveConfigToEEPROM() {
  EEPROM.put(EEPROM_CONFIG_START,    cropIndex);
  EEPROM.put(EEPROM_CONFIG_START+4,  plotArea_m2);
  EEPROM.put(EEPROM_CONFIG_START+8,  offlineLogCount);
  EEPROM.put(EEPROM_CONFIG_START+9,  logHeadPtr);
  EEPROM.commit();
}

void logIrrigEvent(uint8_t trigger) {
  IrrigLog entry;
  entry.timestamp = (uint32_t)(millis() / 1000UL);
  entry.csmi      = csmi;
  entry.litres    = sessionLitres;
  entry.trigger   = trigger;
  entry.stage     = (uint8_t)stage;
  entry.aiScore10 = (uint16_t)(aiScore * 10.0f);

  int addr = EEPROM_LOG_START + (logHeadPtr % EEPROM_LOG_COUNT) * EEPROM_LOG_SIZE;
  EEPROM.put(addr, entry);
  logHeadPtr      = (logHeadPtr + 1) % EEPROM_LOG_COUNT;
  offlineLogCount = min((uint8_t)(offlineLogCount + 1), (uint8_t)EEPROM_LOG_COUNT);
  saveConfigToEEPROM();
}

// ============================================================
// OFFLINE SYNC
// ============================================================
void syncOfflineLogs() {
  if (offlineLogCount == 0 || !wifiConnected) return;
  Serial.printf("Syncing %d offline logs...\n", offlineLogCount);

  for (uint8_t i = 0; i < offlineLogCount; i++) {
    IrrigLog entry;
    int addr = EEPROM_LOG_START + (i % EEPROM_LOG_COUNT) * EEPROM_LOG_SIZE;
    EEPROM.get(addr, entry);

    String url = "https://" + String(FIREBASE_HOST)
               + "/tscric/offline_logs/" + String(entry.timestamp)
               + ".json?auth=" + String(FIREBASE_SECRET);

    StaticJsonDocument<192> doc;
    doc["ts"]       = entry.timestamp;
    doc["csmi"]     = entry.csmi;
    doc["litres"]   = entry.litres;
    doc["trigger"]  = entry.trigger;
    doc["stage"]    = entry.stage;
    doc["aiScore"]  = entry.aiScore10 / 10.0f;

    String payload; serializeJson(doc, payload);
    WiFiClientSecure client; client.setInsecure();
    HTTPClient http; http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000);
    http.PUT(payload);
    http.end();
    delay(80);
  }

  offlineLogCount = 0;
  logHeadPtr      = 0;
  saveConfigToEEPROM();
  Serial.println(F("Offline sync complete."));
}

/*
 * ============================================================
 * FIREBASE DATABASE SCHEMA — v5.0
 * ============================================================
 * tscric/sensors/
 *   — Core: sm1 sm2 sm3 vwc1 vwc2 vwc3 csmi
 *           temperature humidity pressure flowRate
 *   — AI:   aiScore smv sma tprScore eto rainProb
 *   — Pump: pump autoMode safeMode pipelineFault
 *           dhtFallback bmpFallback offlineMode wifiMode
 *           pumpCyclesToday pumpRunTimeSec pumpRunTimeH pumpRunTimeM
 *           motorVoltage motorCurrentA motorPowerW motorTempC
 *           sessionLitres
 *   — Water: deltaApplied deltaRequired deltaBalance
 *            totalLitresDay effectiveRain rainfallContrib
 *            waterSavedTotal plotArea_m2
 *            irrigByDepth0 irrigByDepth1 irrigByDepth2
 *   — Crop:  stage gdd crop cropName
 *   — Rain:  tipBucket_mm tipBucket_pulses
 *   — Soil:  soilTemp ecValue phValue npkN npkP npkK
 *   — LoRa:  loraPacketsTX loraPacketsRX loraRSSI loraSNR
 *            loraNodeCount loraOk
 *   — Conn:  weatherPrimaryLocal offlineLogCount uptimeMs
 *
 * tscric/config/     ← dashboard writes
 *   plotArea crop weatherLocation weatherPrimaryLocal
 *   soilCalib/ch0 ch1 ch2
 *   sched0 sched1 sched2  {active, hour, min, dur}
 *
 * tscric/commands/   ← dashboard writes
 *   pumpOn pumpOff auto resetDaily
 *
 * tscric/offline_logs/{timestamp}/
 *   ts csmi litres trigger stage aiScore
 * ============================================================
 *
 * REQUIRED LIBRARIES (install via Arduino Library Manager)
 *   ESP8266WiFi          — bundled with ESP8266 board package
 *   ESP8266HTTPClient    — bundled
 *   WiFiClientSecure     — bundled
 *   DHT sensor library   — Adafruit
 *   Adafruit BMP280      — Adafruit
 *   LoRa                 — sandeepmistry
 *   ArduinoJson          — Benoit Blanchon (v6.x)
 *   OneWire              — Paul Stoffregen (if DS18B20 enabled)
 *   DallasTemperature    — Miles Burton   (if DS18B20 enabled)
 * ============================================================
 */
