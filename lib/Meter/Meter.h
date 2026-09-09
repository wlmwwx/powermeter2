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
  void resetEnergy(Data* out);

 private:
  ht7017::HT7017* _chip = nullptr;
  CaliParams _cali;
  uint32_t _last_update_ms = 0;  // for ch2 watt-second integration

  // Helpers
  static float toSigned(int32_t v24);  // 24-bit two's complement
  static float convPower(float p_reg, float gain);
};

}  // namespace meter
