#include "Meter.h"
#include <math.h>

namespace meter {

void Meter::begin(ht7017::HT7017* chip, const CaliParams& cali) {
  _chip = chip;
  _cali = cali;
}

void Meter::resetEnergy(Data* out) {
  // Energy is added each update() from raw pulses; resetting requires
  // also clearing chip EnergyP/EnergyQ (which auto-clears on read if EnergyClr=1).
  // Caller (WebServer/CLI) is responsible for sending EMUCFG.EnergyClr command.
  _last_update_ms = 0;
  if (out) {
    out->ch1.ep_kwh = 0;
    out->ch1.eq_kvarh = 0;
    out->ch2.ep_kwh = 0;
    out->ch2.eq_kvarh = 0;
  }
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

  // Energy accumulation
  // Ch1: raw pulses from chip -> kWh
  double d_ep1 = (double)raw.ep / _cali.ec;
  double d_eq1 = (double)raw.eq / _cali.ec;
  out->ch1.ep_kwh += d_ep1;
  out->ch1.eq_kvarh += d_eq1;

  // Ch2: watt-second integration over time
  uint32_t now_ms = millis();
  uint32_t dt_ms = (_last_update_ms != 0) ? (now_ms - _last_update_ms) : 0;
  _last_update_ms = now_ms;
  if (dt_ms > 0 && dt_ms < 10000) {  // ignore first call + huge gaps
    out->ch2.ep_kwh   += (double)out->ch2.p * (double)dt_ms / 3600000.0;
    out->ch2.eq_kvarh += (double)out->ch2.q * (double)dt_ms / 3600000.0;
  }

  return true;
}

}  // namespace meter
