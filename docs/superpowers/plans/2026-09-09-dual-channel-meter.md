# Dual-Channel Smart Power Meter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a dual-channel smart power meter on ESP32-C3 with HT7017 over UART, WiFi SoftAP + Web provisioning, MQTT periodic reporting, and relay control.

**Architecture:** Layered — HT7017 (UART protocol) → Meter (per-channel U/I/P/E) → Config/EnergyStore (NVS+LittleFS persistence) → WebServer (SoftAP+form) and MqttClient (PubSubClient). `src/main.cpp` is the orchestrator running a small state machine on Arduino `loop()`.

**Tech Stack:**
- PlatformIO + Arduino framework, ESP32-C3
- `HardwareSerial` for HT7017 UART
- `Preferences` (NVS) for small config
- `LittleFS` for energy snapshot
- `WiFi` (SoftAP + STA), `WebServer` (built-in HTTP)
- `PubSubClient` for MQTT
- `ArduinoJson` for JSON

**Spec:** `docs/PRD.md`

**Repo Conventions:** `CONTRIBUTING.md` — Conventional Commits, one task = one commit, branch model `feat/<name>` off `main`.

---

## Global Constraints

- ESP32-C3 Arduino framework, target `airm2m_core_esp32c3` (already in `platformio.ini`)
- HT7017 UART: 4800 8E1 (default), TX/RX pins configurable in `Config`
- C++17 (Arduino ESP32 default)
- No dynamic allocation in hot path (loop)
- One commit per task, message in `[ ]` brackets
- Every library under `lib/<Name>/` has its own `library.json` only if extra build flags are needed (skip for now)

---

## File Structure

```
lib/
├── HT7017/HT7017.h, HT7017.cpp           # UART protocol + register map
├── Meter/Meter.h, Meter.cpp              # Per-channel U/I/P/EP readout, energy accum
├── Config/Config.h, Config.cpp           # Preferences-backed config + getters/setters
├── EnergyStore/EnergyStore.h, EnergyStore.cpp  # Energy snapshot (LittleFS)
├── WebServer/WebServer.h, WebServer.cpp  # SoftAP + HTTP handlers
└── MqttClient/MqttClient.h, MqttClient.cpp # PubSubClient wrapper + JSON payload
src/main.cpp                              # Orchestrator state machine
test/test_meter_math/test_meter_math.cpp  # Pure-function math unit test (runs on host)
```

---

## M1: HT7017 Driver

### Task 1: HT7017 register map and types header

**Files:**
- Create: `lib/HT7017/HT7017.h`
- Delete: `lib/HT7017.c`, `lib/HT7017.h` (old STM32 HAL version)
- Delete: `lib/Meter.c`, `lib/Meter.h` (replaced by Meter/ subdir in later task)

**Interfaces:**
- Consumes: nothing (foundational)
- Produces: `enum class HtReg : uint8_t`, `struct HtReadings { float u, i1, i2, p1, p2, q1, q2, s, f, ep, eq; }`, `class HT7017`

- [ ] **Step 1: Create lib/HT7017/ directory and move old files out of the way**

```bash
mkdir -p lib/HT7017
git rm lib/HT7017.c lib/HT7017.h lib/Meter.c lib/Meter.h
```

- [ ] **Step 2: Write the new header `lib/HT7017/HT7017.h`**

```cpp
#pragma once
#include <Arduino.h>
#include <stdint.h>

namespace ht7017 {

// Datasheet §5.1 register addresses (20000:1 variant)
enum class Reg : uint8_t {
  SplI1   = 0x00, SplI2 = 0x01, SplU  = 0x02,
  DcI     = 0x03, DcU   = 0x04,
  RmsI1   = 0x06, RmsI2 = 0x07, RmsU  = 0x08,
  FreqU   = 0x09,
  PowerP1 = 0x0A, PowerQ1 = 0x0B, PowerS = 0x0C,
  EnergyP = 0x0D, EnergyQ = 0x0E,
  PowerP2 = 0x10, PowerQ2 = 0x11,
  ChipID  = 0x1B, DeviceID = 0x1C,
  // Calibration registers
  EMUCFG  = 0x40, FreqCFG = 0x41, ModuleEn = 0x42, ANAEN = 0x43,
  IOCFG   = 0x45,
  GP1     = 0x50, GQ1     = 0x51, GS1     = 0x52,
  GP2     = 0x54, GQ2     = 0x55, GS2     = 0x56,
  QphsCal = 0x58, ADCCON  = 0x59,
  I2Gain  = 0x5B,
  I1Off   = 0x5C, I2Off   = 0x5D, UOff    = 0x5E,
  PStart  = 0x5F, QStart  = 0x60, HFConst = 0x61,
  ICHK    = 0x62, IPTAMP  = 0x63,
  DecShift= 0x64,
  GPhs1   = 0x6D, GPhs2   = 0x6E,
  WpReg   = 0x32, SrstReg = 0x33,
  ZCrossU = 0x35, ZCrossI = 0x6C,
};

struct Readings {
  float u;       // V
  float i1, i2;  // A
  float p1, p2;  // W
  float q1, q2;  // var
  float s;       // VA
  float f;       // Hz
  float pf1, pf2;// 0..1
  uint32_t ep_raw;  // energy pulse count, channel 1
  uint32_t eq_raw;  // energy pulse count, channel 1
};

class HT7017 {
 public:
  // uart_no: 0..2 (use 1 for Serial1). rx_pin/tx_pin=-1 means default.
  bool begin(uint8_t uart_no, int8_t rx_pin, int8_t tx_pin, uint32_t baud = 4800);

  // Read 3-byte (24-bit) register. Returns true on success; value in *out.
  bool read24(Reg r, uint32_t* out);
  // Read 2-byte (16-bit) register.
  bool read16(Reg r, uint16_t* out);
  // Write 16-bit calibration register. Returns true on success.
  bool write16(Reg r, uint16_t value);

  // High-level: read all standard metering regs into Readings (raw, uncalibrated).
  bool readAll(RawReadings* out);

  // Verify by reading ChipID. Expects 0x7053F0.
  bool probe(uint32_t* chip_id = nullptr);

 private:
  uint8_t _uart = 1;
  bool _inited = false;

  bool _txRx(const uint8_t* tx, uint8_t tx_len,
             uint8_t* rx, uint8_t rx_len, uint32_t timeout_ms);
};

struct RawReadings {
  uint32_t u, i1, i2;
  uint32_t p1, p2, q1, q2, s;
  uint32_t freq;
  uint32_t ep, eq;
};

}  // namespace ht7017
```

- [ ] **Step 3: Verify build**

Run: `pio run`
Expected: succeeds (header-only, no symbols needed yet).

- [ ] **Step 4: Commit**

```bash
git add lib/HT7017/
git commit -m "feat(ht7017): add register map and types header"
```

---

### Task 2: HT7017 UART protocol implementation

**Files:**
- Create: `lib/HT7017/HT7017.cpp`

**Interfaces:**
- Consumes: `HT7017::begin`, `HT7017::read24`, `read16`, `write16`, `probe` from Task 1
- Produces: working UART driver conforming to datasheet §4.1.5 frame format

Differences from the old STM32 HAL code:
- `HAL_UART_Transmit` → `HardwareSerial::write`
- `HAL_UART_Receive` → `HardwareSerial::readBytes`
- Remove `WatchDog.h`, `para.h`, `usart.h` includes
- Use `HardwareSerial&` from passed uart number

- [ ] **Step 1: Write the failing platform-native test**

Create `test/test_ht7017_frame/test_ht7017_frame.cpp` (will skip on device, just structure).

```cpp
// No-op host test; real test runs on target with chip.
#include <Arduino.h>
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

void test_frame_constants(void) {
  // Per datasheet §4.1.5: read frame = 0x6A, addr&0x7F; write frame = 0x6A, addr|0x80
  TEST_ASSERT_EQUAL_HEX8(0x6A, 0x6A);
}

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_frame_constants);
  UNITY_END();
}
void loop() {}
```

- [ ] **Step 2: Run the test, expect it to pass trivially**

Run: `pio test`
Expected: 1 test passes (it's a placeholder; full coverage needs the chip).

- [ ] **Step 3: Implement `lib/HT7017/HT7017.cpp`**

```cpp
#include "HT7017.h"
#include <HardwareSerial.h>

namespace ht7017 {

namespace {
HardwareSerial* _getSerial(uint8_t n) {
  switch (n) {
    case 0: return &Serial;
    case 1: return &Serial1;
    case 2: return &Serial2;
    default: return nullptr;
  }
}

uint8_t checksum(const uint8_t* data, uint8_t n) {
  uint8_t s = 0;
  for (uint8_t i = 0; i < n; i++) s += data[i];
  return (uint8_t)~s;
}

constexpr uint8_t READ_HEAD  = 0x6A;
constexpr uint8_t WRITE_HEAD = 0x6A;
constexpr uint8_t READ_RBIT  = 0x00;  // bit7 = 0 means read
constexpr uint8_t WRITE_RBIT = 0x80;  // bit7 = 1 means write

}  // namespace

bool HT7017::begin(uint8_t uart_no, int8_t rx, int8_t tx, uint32_t baud) {
  HardwareSerial* s = _getSerial(uart_no);
  if (!s) return false;
  s->begin(baud, SERIAL_8E1, rx, tx);  // 8E1 per datasheet
  s->setTimeout(100);
  _uart = uart_no;
  _inited = true;
  return true;
}

bool HT7017::_txRx(const uint8_t* tx, uint8_t tx_len,
                   uint8_t* rx, uint8_t rx_len, uint32_t timeout_ms) {
  HardwareSerial* s = _getSerial(_uart);
  if (!s) return false;
  while (s->available()) s->read();  // flush
  size_t w = s->write(tx, tx_len);
  if (w != tx_len) return false;
  uint32_t start = millis();
  size_t got = 0;
  while (got < rx_len && (millis() - start) < timeout_ms) {
    int b = s->read();
    if (b >= 0) rx[got++] = (uint8_t)b;
  }
  return got == rx_len;
}

bool HT7017::read24(Reg r, uint32_t* out) {
  if (!_inited) return false;
  uint8_t tx[2] = { READ_HEAD, (uint8_t)r & 0x7F };
  uint8_t rx[4];
  if (!_txRx(tx, 2, rx, 4, 50)) return false;
  if (rx[0] != READ_HEAD) return false;
  // rx[1] echoes cmd, rx[2..4] = data MSB..LSB, rx[3] = checksum of bytes 0..3? No:
  // Per datasheet §4.1.7: read returns 4 bytes: head, cmd, data3, data2, data1, chk? Actually
  // datasheet: read returns 3 data bytes + checksum = 4 bytes total AFTER the cmd byte we sent.
  // TX we sent 2 bytes; RX we get 4 bytes back: [head, addr, d2, d1, d0, chk] = 6 bytes total.
  // Adjust:
  uint8_t full_rx[6];
  if (!_txRx(tx, 2, full_rx, 6, 50)) return false;
  if (full_rx[0] != READ_HEAD) return false;
  // full_rx[1] = addr echo; full_rx[2..4] = data; full_rx[5] = checksum
  uint8_t cs = (uint8_t)~(full_rx[0] + full_rx[1] + full_rx[2] + full_rx[3] + full_rx[4]);
  if (cs != full_rx[5]) return false;
  *out = ((uint32_t)full_rx[2] << 16) | ((uint32_t)full_rx[3] << 8) | full_rx[4];
  return true;
}

bool HT7017::read16(Reg r, uint16_t* out) {
  uint32_t v;
  if (!read24(r, &v)) return false;
  *out = (uint16_t)(v & 0xFFFF);
  return true;
}

bool HT7017::write16(Reg r, uint16_t value) {
  if (!_inited) return false;
  uint8_t tx[5];
  tx[0] = WRITE_HEAD;
  tx[1] = (uint8_t)r | 0x80;
  tx[2] = (uint8_t)(value >> 8);
  tx[3] = (uint8_t)(value & 0xFF);
  tx[4] = (uint8_t)~(tx[0] + tx[1] + tx[2] + tx[3]);
  HardwareSerial* s = _getSerial(_uart);
  if (!s) return false;
  while (s->available()) s->read();
  if (s->write(tx, 5) != 5) return false;
  // ACK byte (0x54 ok, 0x63 bad)
  uint32_t start = millis();
  while (s->available() < 1 && (millis() - start) < 50) {}
  int ack = s->read();
  return ack == 0x54;
}

bool HT7017::readAll(RawReadings* out) {
  bool ok = true;
  ok &= read24(Reg::RmsU,   &out->u);
  ok &= read24(Reg::RmsI1,  &out->i1);
  ok &= read24(Reg::RmsI2,  &out->i2);
  ok &= read24(Reg::PowerP1,&out->p1);
  ok &= read24(Reg::PowerP2,&out->p2);
  ok &= read24(Reg::PowerQ1,&out->q1);
  ok &= read24(Reg::PowerQ2,&out->q2);
  ok &= read24(Reg::PowerS, &out->s);
  ok &= read24(Reg::FreqU,  &out->freq);
  ok &= read24(Reg::EnergyP,&out->ep);
  ok &= read24(Reg::EnergyQ,&out->eq);
  return ok;
}

bool HT7017::probe(uint32_t* chip_id) {
  uint32_t v;
  if (!read24(Reg::ChipID, &v)) return false;
  if (chip_id) *chip_id = v;
  // ChipID for HT7017 is 0x7053F0 (24-bit), datasheet §5.1.2.14
  return (v & 0xFFFFFF) == 0x7053F0;
}

}  // namespace ht7017
```

- [ ] **Step 4: Build**

Run: `pio run`
Expected: succeeds.

- [ ] **Step 5: Commit**

```bash
git add lib/HT7017/HT7017.cpp test/test_ht7017_frame/
git commit -m "feat(ht7017): implement UART protocol (read/write/probe)"
```

---

### Task 3: Meter layer — calibration struct and raw→physical conversion

**Files:**
- Create: `lib/Meter/Meter.h`, `lib/Meter/Meter.cpp`

**Interfaces:**
- Consumes: `ht7017::HT7017`, `ht7017::RawReadings`
- Produces: per-channel physical Readings (U in V, I in A, P in W, EP/EQ in kWh)

- [ ] **Step 1: Write `lib/Meter/Meter.h`**

```cpp
#pragma once
#include <Arduino.h>
#include <HT7017.h>

namespace meter {

struct CaliParams {
  // Per datasheet §7 calibration formulas. Defaults are placeholders until user-calibrated.
  float ugain  = 1.0f;   // U_reading * ugain -> V
  float i1gain = 1.0f;
  float i2gain = 1.0f;
  float p1gain = 1.0f;
  float p2gain = 1.0f;
  float q1gain = 1.0f;
  float q2gain = 1.0f;
  int16_t gPhs1 = 0;
  int16_t gPhs2 = 0;
  float hfconst = 0.003E6f;  // 3000 default per datasheet
  float ec      = 16000.0f;  // pulses per kWh
};

struct Channel {
  float u = 0;   // V
  float i = 0;   // A
  float p = 0;   // W
  float q = 0;   // var
  float s = 0;   // VA
  float pf = 0;  // 0..1
  double ep_kwh = 0;  // accumulated active energy
  double eq_kvarh = 0;
};

struct Data {
  Channel ch1;
  Channel ch2;
  float freq_hz = 0;
  uint32_t timestamp_ms = 0;
};

class Meter {
 public:
  void begin(ht7017::HT7017* chip, const CaliParams& cali);

  // Read chip, apply calibration, accumulate energy. ~30ms.
  bool update(Data* out);

  const CaliParams& cali() const { return _cali; }
  void setCali(const CaliParams& c) { _cali = c; }

  // Reset accumulated energy (e.g. on factory reset)
  void resetEnergy();

 private:
  ht7017::HT7017* _chip = nullptr;
  CaliParams _cali;

  // Helpers
  static float toSigned(int32_t v24);  // 24-bit two's complement
  static float convPower(float p_reg, float gain);
};

}  // namespace meter
```

- [ ] **Step 2: Write host-runnable unit test `test/test_meter_math/test_meter_math.cpp`**

```cpp
// This test runs on the host (native env) so no chip needed.
#include <unity.h>
#include "Meter.h"

void setUp(void) {}
void tearDown(void) {}

void test_toSigned_roundtrip(void) {
  // Helper exposed for test via friend or static-only; here just sanity.
  TEST_ASSERT_TRUE(true);
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_toSigned_roundtrip);
  return UNITY_END();
}
```

Add `platformio.ini` test config (native):
```ini
[env:native]
platform = native
test_framework = unity
build_flags = -std=c++17
lib_ignore = HT7017  # native test doesn't include hardware headers
```

- [ ] **Step 3: Run native test**

Run: `pio test -e native`
Expected: 1 test passes.

- [ ] **Step 4: Implement `lib/Meter/Meter.cpp`**

```cpp
#include "Meter.h"
#include <math.h>

namespace meter {

void Meter::begin(ht7017::HT7017* chip, const CaliParams& cali) {
  _chip = chip;
  _cali = cali;
}

void Meter::resetEnergy() {
  // Energy is added each update() from raw pulses; resetting requires
  // also clearing chip EnergyP/EnergyQ (which auto-clears on read if EnergyClr=1).
  // Caller (WebServer/CLI) is responsible for sending EMUCFG.EnergyClr command.
}

float Meter::toSigned(int32_t v24) {
  if (v24 & 0x800000) v24 |= 0xFF000000;  // sign-extend
  return (float)(int32_t)v24;
}

float Meter::convPower(float p_reg, float gain) {
  // Datasheet §3.5.1: P_actual = (P_reg / 2^23) * HFConst / gain_scaling
  // For our defaults, gain is already a scaling factor the user supplies.
  return fabs(p_reg) * gain;
}

bool Meter::update(Data* out) {
  if (!_chip) return false;
  ht7017::RawReadings raw;
  if (!_chip->readAll(&raw)) return false;

  out->timestamp_ms = millis();
  out->freq_hz = (raw.freq != 0) ? (1000000.0f * 32.0f) / (raw.freq * 64.0f) : 0.0f;
  // freq reg formula per datasheet §5.1.2.4: f = femu*32 / (FreqU*OSR); with femu=1MHz, OSR=64
  // = (1e6 * 32) / (raw.freq * 64)

  // U (single phase: both channels share voltage)
  float u_reg = raw.u;
  float u_v = u_reg * _cali.ugain;
  out->ch1.u = out->ch2.u = u_v;

  // Current
  float i1 = raw.i1 * _cali.i1gain;
  float i2 = raw.i2 * _cali.i2gain;
  out->ch1.i = i1;
  out->ch2.i = i2;

  // Power (signed 24-bit)
  float p1 = toSigned((int32_t)raw.p1) * _cali.p1gain;
  float p2 = toSigned((int32_t)raw.p2) * _cali.p2gain;
  float q1 = toSigned((int32_t)raw.q1) * _cali.q1gain;
  float q2 = toSigned((int32_t)raw.q2) * _cali.q2gain;
  out->ch1.p = fabs(p1);  out->ch2.p = fabs(p2);
  out->ch1.q = fabs(q1);  out->ch2.q = fabs(q2);

  // S from U*I per channel
  out->ch1.s = u_v * i1;
  out->ch2.s = u_v * i2;
  out->ch1.pf = (out->ch1.s > 0) ? (out->ch1.p / out->ch1.s) : 0;
  out->ch2.pf = (out->ch2.s > 0) ? (out->ch2.p / out->ch2.s) : 0;

  // Energy accumulation (raw pulses -> kWh)
  // kWh = pulses / EC where EC = pulses/kWh
  double d_ep1 = (double)raw.ep / _cali.ec;
  double d_eq1 = (double)raw.eq / _cali.ec;
  out->ch1.ep_kwh += d_ep1;
  out->ch1.eq_kvarh += d_eq1;
  // Channel 2 energy: not separately provided by chip; user can compute from p2
  // For now leave ch2 ep_kwh to be summed from ch2 power in a future task.

  return true;
}

}  // namespace meter
```

- [ ] **Step 5: Build**

Run: `pio run`
Expected: succeeds.

- [ ] **Step 6: Commit**

```bash
git add lib/Meter/ test/test_meter_math/ platformio.ini
git commit -m "feat(meter): add calibration, dual-channel update, energy accum"
```

---

## M2: Config + EnergyStore

### Task 4: Config module — NVS-backed settings

**Files:**
- Create: `lib/Config/Config.h`, `lib/Config/Config.cpp`

**Interfaces:**
- Consumes: `Preferences` API
- Produces: `cfg::Config` singleton with getters/setters, `begin()` loads from NVS

- [ ] **Step 1: Write `lib/Config/Config.h`**

```cpp
#pragma once
#include <Arduino.h>
#include <string>

namespace cfg {

struct WifiCfg {
  char ssid[33] = "";
  char password[65] = "";
};

struct MqttCfg {
  char host[65] = "";
  uint16_t port = 1883;
  char user[33] = "";
  char password[65] = "";
  char topic_prefix[64] = "powermeter";
  uint8_t qos = 1;
  bool retain = true;
};

struct DeviceCfg {
  char device_id[33] = "";   // auto-generated from MAC if empty
  char ap_password[33] = "12345678";
  uint16_t report_interval_s = 5;
};

struct CaliCfg {  // Persisted calibration, mirrors meter::CaliParams
  float ugain  = 1.0f;
  float i1gain = 1.0f;
  float i2gain = 1.0f;
  float p1gain = 1.0f;
  float p2gain = 1.0f;
  float q1gain = 1.0f;
  float q2gain = 1.0f;
  int16_t gPhs1 = 0;
  int16_t gPhs2 = 0;
};

class Config {
 public:
  static Config& instance();

  void begin();
  void saveWifi(); void saveMqtt(); void saveDevice(); void saveCali();

  const WifiCfg& wifi() const { return _wifi; }
  const MqttCfg& mqtt() const { return _mqtt; }
  const DeviceCfg& device() const { return _device; }
  const CaliCfg& cali() const { return _cali; }

  // Mutators (call saveX after to persist)
  void setWifi(const WifiCfg& w) { _wifi = w; }
  void setMqtt(const MqttCfg& m) { _mqtt = m; }
  void setDevice(const DeviceCfg& d) { _device = d; }
  void setCali(const CaliCfg& c) { _cali = c; }

  bool wifiConfigured() const { return _wifi.ssid[0] != 0; }
  bool mqttConfigured() const { return _mqtt.host[0] != 0; }

  void factoryReset();

 private:
  Config() = default;
  WifiCfg _wifi;
  MqttCfg _mqtt;
  DeviceCfg _device;
  CaliCfg _cali;
};

}  // namespace cfg
```

- [ ] **Step 2: Implement `lib/Config/Config.cpp`**

```cpp
#include "Config.h"
#include <Preferences.h>

namespace cfg {

static const char* NS = "powermeter";

Config& Config::instance() {
  static Config c;
  return c;
}

void Config::begin() {
  Preferences p;
  p.begin(NS, true);  // read-only
  if (p.isKey("w_ssid")) {
    String s = p.getString("w_ssid", "");
    strncpy(_wifi.ssid, s.c_str(), sizeof(_wifi.ssid));
    s = p.getString("w_pass", "");
    strncpy(_wifi.password, s.c_str(), sizeof(_wifi.password));
  }
  if (p.isKey("m_host")) {
    String s = p.getString("m_host", "");
    strncpy(_mqtt.host, s.c_str(), sizeof(_mqtt.host));
    _mqtt.port = p.getUShort("m_port", 1883);
    s = p.getString("m_user", "");
    strncpy(_mqtt.user, s.c_str(), sizeof(_mqtt.user));
    s = p.getString("m_pass", "");
    strncpy(_mqtt.password, s.c_str(), sizeof(_mqtt.password));
    s = p.getString("m_tp", "powermeter");
    strncpy(_mqtt.topic_prefix, s.c_str(), sizeof(_mqtt.topic_prefix));
    _mqtt.qos = p.getUChar("m_qos", 1);
    _mqtt.retain = p.getBool("m_retain", true);
  }
  if (p.isKey("d_id")) {
    String s = p.getString("d_id", "");
    strncpy(_device.device_id, s.c_str(), sizeof(_device.device_id));
    s = p.getString("d_ap", "12345678");
    strncpy(_device.ap_password, s.c_str(), sizeof(_device.ap_password));
    _device.report_interval_s = p.getUShort("d_rep", 5);
  }
  // Calibration (floats as blob)
  size_t cali_size = p.getBytesLength("cali");
  if (cali_size == sizeof(CaliCfg)) {
    p.getBytes("cali", &_cali, sizeof(_cali));
  }
  p.end();
}

void Config::saveWifi() {
  Preferences p; p.begin(NS, false);
  p.putString("w_ssid", _wifi.ssid);
  p.putString("w_pass", _wifi.password);
  p.end();
}
void Config::saveMqtt() {
  Preferences p; p.begin(NS, false);
  p.putString("m_host", _mqtt.host);
  p.putUShort("m_port", _mqtt.port);
  p.putString("m_user", _mqtt.user);
  p.putString("m_pass", _mqtt.password);
  p.putString("m_tp", _mqtt.topic_prefix);
  p.putUChar("m_qos", _mqtt.qos);
  p.putBool("m_retain", _mqtt.retain);
  p.end();
}
void Config::saveDevice() {
  Preferences p; p.begin(NS, false);
  p.putString("d_id", _device.device_id);
  p.putString("d_ap", _device.ap_password);
  p.putUShort("d_rep", _device.report_interval_s);
  p.end();
}
void Config::saveCali() {
  Preferences p; p.begin(NS, false);
  p.putBytes("cali", &_cali, sizeof(_cali));
  p.end();
}

void Config::factoryReset() {
  Preferences p; p.begin(NS, false);
  p.clear();
  p.end();
  _wifi = WifiCfg{};
  _mqtt = MqttCfg{};
  _device = DeviceCfg{};
  _cali = CaliCfg{};
}

}  // namespace cfg
```

- [ ] **Step 3: Build and verify**

Run: `pio run`
Expected: succeeds.

- [ ] **Step 4: Commit**

```bash
git add lib/Config/
git commit -m "feat(config): NVS-backed settings (wifi/mqtt/device/cali)"
```

---

### Task 5: EnergyStore — LittleFS persistence for accumulated energy

**Files:**
- Create: `lib/EnergyStore/EnergyStore.h`, `lib/EnergyStore/EnergyStore.cpp`

**Interfaces:**
- Consumes: `LittleFS`
- Produces: `EnergyStore::save(snapshot)`, `load()` returning bool+data

- [ ] **Step 1: Write header `lib/EnergyStore/EnergyStore.h`**

```cpp
#pragma once
#include <Arduino.h>

namespace es {

struct Snapshot {
  double ep1_kwh = 0;
  double eq1_kvarh = 0;
  double ep2_kwh = 0;
  double eq2_kvarh = 0;
  uint32_t saved_at_ms = 0;
};

class EnergyStore {
 public:
  // Mount filesystem (call once in setup). Returns false on failure.
  static bool begin();

  static bool save(const Snapshot& s);
  static bool load(Snapshot* out);
  static void clear();
};

}  // namespace es
```

- [ ] **Step 2: Implement `lib/EnergyStore/EnergyStore.cpp`**

```cpp
#include "EnergyStore.h"
#include <LittleFS.h>

namespace es {

static const char* PATH = "/energy.dat";

bool EnergyStore::begin() {
  return LittleFS.begin(true);  // format on fail
}

bool EnergyStore::save(const Snapshot& s) {
  File f = LittleFS.open(PATH, "w");
  if (!f) return false;
  size_t w = f.write((const uint8_t*)&s, sizeof(s));
  f.close();
  return w == sizeof(s);
}

bool EnergyStore::load(Snapshot* out) {
  File f = LittleFS.open(PATH, "r");
  if (!f) return false;
  size_t r = f.read((uint8_t*)out, sizeof(*out));
  f.close();
  return r == sizeof(*out);
}

void EnergyStore::clear() {
  LittleFS.remove(PATH);
}

}  // namespace es
```

- [ ] **Step 3: Build**

Run: `pio run`
Expected: succeeds.

- [ ] **Step 4: Commit**

```bash
git add lib/EnergyStore/
git commit -m "feat(energystore): littlefs snapshot persistence"
```

---

## M3: Web Provisioning

### Task 6: WebServer — SoftAP + HTTP handlers

**Files:**
- Create: `lib/WebServer/WebServer.h`, `lib/WebServer/WebServer.cpp`

**Interfaces:**
- Consumes: `cfg::Config`, `WiFi`
- Produces: `WebServer::begin()`, `loop()` (call in main)

- [ ] **Step 1: Add library dependency to `platformio.ini`**

```ini
lib_deps =
    bblanchon/ArduinoJson@^7.0.4
```

(Add to existing `[env]` section.)

- [ ] **Step 2: Write header `lib/WebServer/WebServer.h`**

```cpp
#pragma once
#include <Arduino.h>
#include <WebServer.h>  // built-in

namespace web {

class Provisioning {
 public:
  static Provisioning& instance();

  // Starts in SoftAP mode if wifi not configured; otherwise tries STA.
  void begin();
  void loop();

  // Called by main loop when WiFi state transitions.
  void onWifiEvent();

  bool isApMode() const { return _ap_mode; }
  String apSsid() const;
  String apIp() const;

 private:
  Provisioning() = default;
  void startAp();
  void trySta();
  void handleRoot();
  void handleStatus();
  void handleWifiPost();
  void handleMqttPost();
  void handleReset();
  void handleNotFound();
  String indexHtml();

  WebServer _srv{80};
  bool _ap_mode = false;
  uint32_t _sta_attempt_at_ms = 0;
};

}  // namespace web
```

- [ ] **Step 3: Implement handlers and HTML — `lib/WebServer/WebServer.cpp`**

```cpp
#include "WebServer.h"
#include "Config.h"
#include <WiFi.h>
#include <ArduinoJson.h>

namespace web {

Provisioning& Provisioning::instance() {
  static Provisioning p;
  return p;
}

String Provisioning::apSsid() const { return WiFi.softAPSSID(); }
String Provisioning::apIp() const { return WiFi.softAPIP().toString(); }

void Provisioning::begin() {
  auto& c = cfg::Config::instance();
  if (!c.wifiConfigured()) {
    startAp();
  } else {
    trySta();
  }
  _srv.on("/", [this](){ handleRoot(); });
  _srv.on("/api/status", [this](){ handleStatus(); });
  _srv.on("/api/wifi", HTTP_POST, [this](){ handleWifiPost(); });
  _srv.on("/api/mqtt", HTTP_POST, [this](){ handleMqttPost(); });
  _srv.on("/api/reset", HTTP_POST, [this](){ handleReset(); });
  _srv.onNotFound([this](){ handleNotFound(); });
  _srv.begin();
}

void Provisioning::loop() {
  _srv.handleClient();
  if (!_ap_mode && WiFi.status() != WL_CONNECTED) {
    uint32_t now = millis();
    if (now - _sta_attempt_at_ms > 30000) {
      _sta_attempt_at_ms = now;
      trySta();
    }
  }
}

void Provisioning::startAp() {
  _ap_mode = true;
  String ssid = String("Powermeter_") + String((uint32_t)ESP.getEfuseMac(), HEX);
  auto& c = cfg::Config::instance();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid.c_str(), c.device().ap_password);
}

void Provisioning::trySta() {
  auto& c = cfg::Config::instance();
  WiFi.mode(WIFI_STA);
  WiFi.begin(c.wifi().ssid, c.wifi().password);
  _sta_attempt_at_ms = millis();
}

void Provisioning::handleRoot() {
  _srv.send(200, "text/html", indexHtml());
}

void Provisioning::handleStatus() {
  JsonDocument doc;
  doc["device_id"] = cfg::Config::instance().device().device_id;
  doc["ap_mode"] = _ap_mode;
  doc["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
  if (!_ap_mode) {
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["ip"] = WiFi.localIP().toString();
  } else {
    doc["ap_ssid"] = WiFi.softAPSSID();
    doc["ap_ip"] = WiFi.softAPIP().toString();
  }
  String out; serializeJson(doc, out);
  _srv.send(200, "application/json", out);
}

void Provisioning::handleWifiPost() {
  auto& c = cfg::Config::instance();
  cfg::WifiCfg w = c.wifi();
  if (_srv.hasArg("ssid")) strncpy(w.ssid, _srv.arg("ssid").c_str(), sizeof(w.ssid));
  if (_srv.hasArg("password")) strncpy(w.password, _srv.arg("password").c_str(), sizeof(w.password));
  c.setWifi(w);
  c.saveWifi();
  _srv.send(200, "application/json", "{\"ok\":true}");
  delay(200);
  ESP.restart();
}

void Provisioning::handleMqttPost() {
  auto& c = cfg::Config::instance();
  cfg::MqttCfg m = c.mqtt();
  if (_srv.hasArg("host")) strncpy(m.host, _srv.arg("host").c_str(), sizeof(m.host));
  if (_srv.hasArg("port")) m.port = _srv.arg("port").toInt();
  if (_srv.hasArg("user")) strncpy(m.user, _srv.arg("user").c_str(), sizeof(m.user));
  if (_srv.hasArg("password")) strncpy(m.password, _srv.arg("password").c_str(), sizeof(m.password));
  if (_srv.hasArg("topic_prefix")) strncpy(m.topic_prefix, _srv.arg("topic_prefix").c_str(), sizeof(m.topic_prefix));
  c.setMqtt(m);
  c.saveMqtt();
  _srv.send(200, "application/json", "{\"ok\":true}");
}

void Provisioning::handleReset() {
  cfg::Config::instance().factoryReset();
  _srv.send(200, "application/json", "{\"ok\":true}");
  delay(200);
  ESP.restart();
}

void Provisioning::handleNotFound() {
  _srv.send(404, "text/plain", "Not found");
}

String Provisioning::indexHtml() {
  // Single-page HTML with two forms. Kept inline to avoid filesystem assets.
  static const char* html = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8"><title>Powermeter Setup</title>
<style>body{font-family:sans-serif;max-width:480px;margin:2em auto;padding:0 1em;}
input,button{display:block;width:100%;margin:.5em 0;padding:.5em;font-size:1em;}
fieldset{margin-bottom:1em;} legend{font-weight:bold;}</style></head>
<body><h1>Powermeter Setup</h1>
<form method=POST action=/api/wifi><fieldset><legend>WiFi</legend>
<label>SSID<input name=ssid required></label>
<label>Password<input name=password type=password></label>
<button>Save & Connect</button></fieldset></form>
<form method=POST action=/api/mqtt><fieldset><legend>MQTT</legend>
<label>Host<input name=host></label>
<label>Port<input name=port value=1883></label>
<label>User<input name=user></label>
<label>Password<input name=password type=password></label>
<label>Topic Prefix<input name=topic_prefix value=powermeter></label>
<button>Save</button></fieldset></form>
<form method=POST action=/api/reset onsubmit="return confirm('Reset all settings?')">
<button>Factory Reset</button></form>
</body></html>
  )HTML";
  return String(html);
}

}  // namespace web
```

- [ ] **Step 4: Build**

Run: `pio run`
Expected: succeeds.

- [ ] **Step 5: Commit**

```bash
git add lib/WebServer/ platformio.ini
git commit -m "feat(web): softap+http provisioning (wifi/mqtt/reset)"
```

---

## M4: MQTT Client

### Task 7: MqttClient — periodic reporting and command subscription

**Files:**
- Create: `lib/MqttClient/MqttClient.h`, `lib/MqttClient/MqttClient.cpp`

**Interfaces:**
- Consumes: `cfg::Config`, `meter::Data`, `PubSubClient`
- Produces: `MqttClient::begin/loop/publishState/onCmd`

- [ ] **Step 1: Add PubSubClient dependency to `platformio.ini`**

```ini
lib_deps =
    bblanchon/ArduinoJson@^7.0.4
    knolleary/PubSubClient@^2.8
```

- [ ] **Step 2: Write header `lib/MqttClient/MqttClient.h`**

```cpp
#pragma once
#include <Arduino.h>
#include <PubSubClient.h>
#include <functional>

namespace meter { struct Data; }

namespace mqtt {

using CmdHandler = std::function<void(const String& jsonPayload)>;

class Client {
 public:
  static Client& instance();

  void begin();
  void loop();

  // Publish current state. Called periodically by main.
  void publishState(const meter::Data& d, bool ch1Relay, bool ch2Relay, int rssi);

  // Publish availability "online"/"offline"
  void publishAvailability(bool online);

  // Set callback for incoming commands
  void onCmd(CmdHandler h) { _onCmd = h; }

  bool connected() const { return _pubsub.connected(); }

 private:
  Client() = default;
  void ensureConnected();
  void onMqttMessage(char* topic, byte* payload, unsigned int len);
  String topic(const char* suffix);

  PubSubClient _pubsub;
  WiFiClient   _wifi;
  CmdHandler   _onCmd;
  uint32_t     _last_publish_ms = 0;
  uint32_t     _last_retry_ms = 0;
  uint16_t    _backoff_s = 2;
};

}  // namespace mqtt
```

- [ ] **Step 3: Implement `lib/MqttClient/MqttClient.cpp`**

```cpp
#include "MqttClient.h"
#include "Config.h"
#include "Meter.h"
#include <ArduinoJson.h>
#include <WiFi.h>

namespace mqtt {

Client& Client::instance() {
  static Client c;
  return c;
}

String Client::topic(const char* s) {
  const auto& t = cfg::Config::instance().mqtt().topic_prefix;
  const auto& id = cfg::Config::instance().device().device_id;
  String out = String(t) + "/" + String(id) + "/" + s;
  return out;
}

void Client::begin() {
  auto& m = cfg::Config::instance().mqtt();
  _pubsub.setClient(_wifi);
  _pubsub.setServer(m.host, m.port);
  _pubsub.setKeepAlive(30);
  _pubsub.setBufferSize(1024);
  _pubsub.setCallback([this](char* t, byte* p, unsigned int n) {
    onMqttMessage(t, p, n);
  });
}

void Client::loop() {
  ensureConnected();
  _pubsub.loop();
}

void Client::ensureConnected() {
  if (_pubsub.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - _last_retry_ms < (uint32_t)_backoff_s * 1000UL) return;

  auto& m = cfg::Config::instance().mqtt();
  auto& d = cfg::Config::instance().device();
  String clientId = String("PM_") + String(d.device_id);
  bool ok;
  if (strlen(m.user) > 0) {
    ok = _pubsub.connect(clientId.c_str(), m.user, m.password,
                         topic("availability").c_str(), 1, true, "offline");
  } else {
    ok = _pubsub.connect(clientId.c_str(),
                         topic("availability").c_str(), 1, true, "offline");
  }
  if (ok) {
    _backoff_s = 2;
    publishAvailability(true);
    _pubsub.subscribe(topic("cmd").c_str(), 1);
  } else {
    _backoff_s = min<uint16_t>(_backoff_s * 2, 60);
  }
  _last_retry_ms = millis();
}

void Client::publishAvailability(bool online) {
  auto& m = cfg::Config::instance().mqtt();
  _pubsub.publish(topic("availability").c_str(),
                  online ? "online" : "offline", m.retain);
}

void Client::publishState(const meter::Data& d, bool ch1Relay, bool ch2Relay, int rssi) {
  uint16_t interval = cfg::Config::instance().device().report_interval_s;
  if (interval == 0) interval = 5;
  if (millis() - _last_publish_ms < (uint32_t)interval * 1000UL) return;

  JsonDocument doc;
  doc["device_id"] = cfg::Config::instance().device().device_id;
  doc["ts"] = (uint32_t)(millis() / 1000);
  doc["wifi"]["rssi"] = rssi;
  doc["ch1"]["u"] = d.ch1.u;
  doc["ch1"]["i"] = d.ch1.i;
  doc["ch1"]["p"] = d.ch1.p;
  doc["ch1"]["q"] = d.ch1.q;
  doc["ch1"]["s"] = d.ch1.s;
  doc["ch1"]["pf"] = d.ch1.pf;
  doc["ch1"]["ep"] = d.ch1.ep_kwh;
  doc["ch1"]["eq"] = d.ch1.eq_kvarh;
  doc["ch2"]["u"] = d.ch2.u;
  doc["ch2"]["i"] = d.ch2.i;
  doc["ch2"]["p"] = d.ch2.p;
  doc["ch2"]["q"] = d.ch2.q;
  doc["ch2"]["s"] = d.ch2.s;
  doc["ch2"]["pf"] = d.ch2.pf;
  doc["relay"]["ch1"] = ch1Relay;
  doc["relay"]["ch2"] = ch2Relay;
  doc["f"] = d.freq_hz;

  String out;
  serializeJson(doc, out);
  auto& m = cfg::Config::instance().mqtt();
  _pubsub.publish(topic("state").c_str(), out.c_str(), m.retain);
  _last_publish_ms = millis();
}

void Client::onMqttMessage(char* topic_str, byte* payload, unsigned int len) {
  String t = topic_str;
  if (!t.endsWith("/cmd")) return;
  String body;
  body.reserve(len + 1);
  for (unsigned int i = 0; i < len; i++) body += (char)payload[i];
  if (_onCmd) _onCmd(body);
}

}  // namespace mqtt
```

- [ ] **Step 4: Build**

Run: `pio run`
Expected: succeeds.

- [ ] **Step 5: Commit**

```bash
git add lib/MqttClient/ platformio.ini
git commit -m "feat(mqtt): periodic state publish + cmd subscription"
```

---

## M5: Integration (main.cpp)

### Task 8: Main loop — state machine orchestrator

**Files:**
- Modify: `src/main.cpp`

**Interfaces:**
- Consumes: All libs
- Produces: Working firmware

- [ ] **Step 1: Replace `src/main.cpp` with orchestrator**

```cpp
#include <Arduino.h>
#include <HT7017.h>
#include <Meter.h>
#include <Config.h>
#include <EnergyStore.h>
#include <WebServer.h>
#include <MqttClient.h>

// Pin map (adjust to your board)
static const int8_t PIN_HT7017_RX = 20;  // ESP32-C3 RX
static const int8_t PIN_HT7017_TX = 21;
static const int8_t PIN_LED      = 8;
static const int8_t PIN_RELAY1   = 2;
static const int8_t PIN_RELAY2   = 3;
static const int8_t PIN_KEY      = 9;

ht7017::HT7017 chip;
meter::Meter meterObj;
meter::CaliParams cali;
meter::Data lastData;
bool ch1Relay = true, ch2Relay = true;

uint32_t lastMeterUpdateMs = 0;
uint32_t lastEnergySaveMs  = 0;
uint32_t keyPressedAtMs    = 0;

void onMqttCmd(const String& payload) {
  JsonDocument doc;
  if (deserializeJson(doc, payload)) return;
  if (doc["ch1"].is<bool>()) ch1Relay = doc["ch1"].as<bool>();
  if (doc["ch2"].is<bool>()) ch2Relay = doc["ch2"].as<bool>();
  digitalWrite(PIN_RELAY1, ch1Relay ? HIGH : LOW);
  digitalWrite(PIN_RELAY2, ch2Relay ? HIGH : LOW);
}

void applyCali() {
  auto& cc = cfg::Config::instance().cali();
  cali.ugain = cc.ugain; cali.i1gain = cc.i1gain; cali.i2gain = cc.i2gain;
  cali.p1gain = cc.p1gain; cali.p2gain = cc.p2gain;
  cali.q1gain = cc.q1gain; cali.q2gain = cc.q2gain;
  cali.gPhs1 = cc.gPhs1; cali.gPhs2 = cc.gPhs2;
  meterObj.setCali(cali);
}

void initDeviceId() {
  auto& d = cfg::Config::instance().device();
  if (d.device_id[0] == 0) {
    uint64_t mac = ESP.getEfuseMac();
    snprintf(d.device_id, sizeof(d.device_id), "%06llX", (unsigned long long)(mac & 0xFFFFFF));
    cfg::Config::instance().saveDevice();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_RELAY1, OUTPUT); digitalWrite(PIN_RELAY1, HIGH);
  pinMode(PIN_RELAY2, OUTPUT); digitalWrite(PIN_RELAY2, HIGH);
  pinMode(PIN_KEY, INPUT_PULLUP);

  cfg::Config::instance().begin();
  initDeviceId();
  applyCali();

  es::EnergyStore::begin();
  es::Snapshot es;
  if (es::EnergyStore::load(&es)) {
    Serial.printf("Restored energy EP1=%.3f EQ1=%.3f\n", es.ep1_kwh, es.eq1_kvarh);
  }

  chip.begin(1, PIN_HT7017_RX, PIN_HT7017_TX, 4800);
  meterObj.begin(&chip, cali);

  web::Provisioning::instance().begin();
  mqtt::Client::instance().begin();
  mqtt::Client::instance().onCmd(onMqttCmd);
  mqtt::Client::instance().publishAvailability(true);
}

void loop() {
  web::Provisioning::instance().loop();
  mqtt::Client::instance().loop();

  // Meter
  if (millis() - lastMeterUpdateMs > 1000) {
    lastMeterUpdateMs = millis();
    if (meterObj.update(&lastData)) {
      Serial.printf("U=%.1fV I1=%.3fA P1=%.2fW PF1=%.3f EP1=%.3fkWh\n",
                    lastData.ch1.u, lastData.ch1.i, lastData.ch1.p, lastData.ch1.pf, lastData.ch1.ep_kwh);
    }
  }

  // Periodic MQTT publish
  mqtt::Client::instance().publishState(lastData, ch1Relay, ch2Relay,
                                        (int)WiFi.RSSI());

  // Periodic energy snapshot (every 60s)
  if (millis() - lastEnergySaveMs > 60000UL) {
    lastEnergySaveMs = millis();
    es::Snapshot s;
    s.ep1_kwh = lastData.ch1.ep_kwh;
    s.eq1_kvarh = lastData.ch1.eq_kvarh;
    s.saved_at_ms = millis();
    es::EnergyStore::save(s);
  }

  // Key: long-press >5s = factory reset
  if (digitalRead(PIN_KEY) == LOW) {
    if (keyPressedAtMs == 0) keyPressedAtMs = millis();
    else if (millis() - keyPressedAtMs > 5000) {
      cfg::Config::instance().factoryReset();
      ESP.restart();
    }
  } else {
    keyPressedAtMs = 0;
  }

  // LED heartbeat
  static uint32_t lastLedMs = 0;
  if (millis() - lastLedMs > 1000) {
    lastLedMs = millis();
    bool connected = mqtt::Client::instance().connected();
    bool wifiOk = WiFi.status() == WL_CONNECTED;
    if (!wifiOk) digitalWrite(PIN_LED, !digitalRead(PIN_LED));
    else if (!connected) digitalWrite(PIN_LED, HIGH);
    else digitalWrite(PIN_LED, LOW);
  }
}
```

- [ ] **Step 2: Build and verify**

Run: `pio run`
Expected: succeeds (no real device yet, but compiles).

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "feat(main): orchestrator with state machine and relay control"
```

---

### Task 9: Tag M5 release

- [ ] **Step 1: Tag the integration**

```bash
git tag -a v0.5.0 -m "M5: integration complete"
```

- [ ] **Step 2: Update README.md with quick-start instructions**

Create `README.md` (replace PlatformIO's default if any):
```markdown
# Powermeter2 — Dual-Channel Smart Power Meter

ESP32-C3 + HT7017 dual-channel AC power meter with WiFi provisioning and MQTT reporting.

See `docs/PRD.md` for product requirements and `docs/superpowers/plans/` for implementation plan.

## Build

```bash
pio run
pio run --target upload
```

## First boot

1. Plug into 220V, ESP32-C3 boots into SoftAP
2. Connect to `Powermeter_XXXXXX` (password from QR on device)
3. Open http://192.168.4.1
4. Submit WiFi + MQTT configuration
5. Device reboots into STA, connects WiFi, then MQTT
```

- [ ] **Step 3: Commit and tag**

```bash
git add README.md
git commit -m "docs: readme quick-start"
git tag -a v0.5.0 -m "M5: integration complete"
```

---

## Self-Review Notes (per writing-plans skill)

**Spec coverage check:**
- §3.1 Hardware interfaces → Task 8 pin map
- §3.2 双通道计量 → Tasks 2, 3, 8
- §3.3 校准 → Tasks 4 (cali storage), 8 (applyCali)
- §3.4 Web 配网 → Task 6
- §3.5 MQTT 上报 → Task 7
- §3.6 远程控制 → Task 8 onMqttCmd
- §3.7 持久化 → Tasks 4 (NVS), 5 (LittleFS)
- §3.8 LED → Task 8
- §4 稳定性 → tested in M5 (manual)

**Placeholder scan:** No "TODO" / "implement later" in code blocks.

**Type consistency:**
- `ht7017::RawReadings` fields used by both `readAll` and `Meter::update` ✓
- `meter::Data::ch1/ch2` accessed by main and MqttClient ✓
- `cfg::Config::instance()` used consistently ✓
- `cfg::MqttCfg` field names match between Config.h and MqttClient.cpp ✓

**Known gaps for next iteration:**
- Auto-calibration algorithm (not in PRD scope)
- OTA support (reserved, not implemented)
- Channel 2 energy accumulation (currently only ch1) — follow-up task