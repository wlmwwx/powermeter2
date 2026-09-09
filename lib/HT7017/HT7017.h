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
