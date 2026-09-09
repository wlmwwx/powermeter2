#include "HT7017.h"
#include <HardwareSerial.h>

namespace ht7017 {

namespace {
  constexpr uint8_t CMD_HEAD = 0x16;
  constexpr uint8_t RX_HEAD  = 0x81;

  // Singleton map from uart_no (0..2) to HardwareSerial*
  HardwareSerial* serialFor(uint8_t uart_no) {
    switch (uart_no) {
      case 0: return &Serial;
      case 1: return &Serial1;
      case 2: return &Serial2;
      default: return nullptr;
    }
  }

  uint8_t checksumOk(const uint8_t* rx, uint8_t len) {
    uint8_t cs = 0;
    for (uint8_t i = 0; i < len - 1; i++) cs += rx[i];
    return cs == rx[len - 1];
  }
}  // anonymous namespace

bool HT7017::begin(uint8_t uart_no, int8_t rx_pin, int8_t tx_pin, uint32_t baud) {
  _uart = uart_no;
  HardwareSerial* ser = serialFor(uart_no);
  if (!ser) return false;

  if (rx_pin >= 0 && tx_pin >= 0) {
    ser->begin(baud, SERIAL_8E1, rx_pin, tx_pin);
  } else {
    ser->begin(baud, SERIAL_8E1);
  }
  ser->flush();
  _inited = true;
  return true;
}

bool HT7017::_txRx(const uint8_t* tx, uint8_t tx_len,
                    uint8_t* rx, uint8_t rx_len, uint32_t timeout_ms) {
  HardwareSerial* ser = serialFor(_uart);
  if (!ser || !_inited) return false;

  ser->flush();
  while (ser->available()) ser->read();  // drain

  ser->write(tx, tx_len);

  uint32_t deadline = millis() + timeout_ms;
  uint8_t pos = 0;
  while (pos < rx_len && millis() < deadline) {
    if (ser->available()) {
      rx[pos++] = ser->read();
    }
    yield();
  }

  if (pos != rx_len) return false;
  if (rx[0] != RX_HEAD) return false;
  if (!checksumOk(rx, rx_len)) return false;

  return true;
}

bool HT7017::read24(Reg r, uint32_t* out) {
  // TX: head(0x16) + read cmd (0x01 << 1 | 0x00).  See datasheet §5.2.
  // Protocol: TX 2 bytes, RX 6 bytes (head + cmd_echo + data2 + data1 + data0 + checksum).
  uint8_t tx[2] = { CMD_HEAD, static_cast<uint8_t>(static_cast<uint8_t>(r) << 1) };
  uint8_t rx[6];
  if (!_txRx(tx, sizeof(tx), rx, sizeof(rx), 500)) return false;

  // rx[2]=MSB, rx[3]=mid, rx[4]=LSB (big-endian 24-bit)
  *out = (static_cast<uint32_t>(rx[2]) << 16)
       | (static_cast<uint32_t>(rx[3]) << 8)
       |  static_cast<uint32_t>(rx[4]);
  return true;
}

bool HT7017::read16(Reg r, uint16_t* out) {
  // TX: head(0x16) + read cmd (0x01 << 1 | 0x00).  RX 5 bytes.
  uint8_t tx[2] = { CMD_HEAD, static_cast<uint8_t>(static_cast<uint8_t>(r) << 1) };
  uint8_t rx[5];
  if (!_txRx(tx, sizeof(tx), rx, sizeof(rx), 500)) return false;

  *out = (static_cast<uint16_t>(rx[2]) << 8) | static_cast<uint16_t>(rx[3]);
  return true;
}

bool HT7017::write16(Reg r, uint16_t value) {
  // TX: head(0x16) + write cmd (0x01 << 1 | 0x01) + data1 + data0.  RX 3 bytes.
  uint8_t tx[4] = {
    CMD_HEAD,
    static_cast<uint8_t>((static_cast<uint8_t>(r) << 1) | 0x01),
    static_cast<uint8_t>(value >> 8),
    static_cast<uint8_t>(value & 0xFF)
  };
  uint8_t rx[3];
  if (!_txRx(tx, sizeof(tx), rx, sizeof(rx), 500)) return false;
  return true;
}

bool HT7017::readAll(RawReadings* out) {
  if (!read24(Reg::RmsU,  &out->u))     return false;
  if (!read24(Reg::RmsI1, &out->i1))    return false;
  if (!read24(Reg::RmsI2, &out->i2))    return false;
  if (!read24(Reg::PowerP1, &out->p1))  return false;
  if (!read24(Reg::PowerP2, &out->p2))  return false;
  if (!read24(Reg::PowerQ1, &out->q1))  return false;
  if (!read24(Reg::PowerQ2, &out->q2))  return false;
  if (!read24(Reg::PowerS,  &out->s))   return false;
  if (!read24(Reg::FreqU,   &out->freq)) return false;
  if (!read24(Reg::EnergyP, &out->ep))  return false;
  if (!read24(Reg::EnergyQ, &out->eq))  return false;
  return true;
}

bool HT7017::probe(uint32_t* chip_id) {
  uint32_t id;
  if (!read24(Reg::ChipID, &id)) return false;
  // Expect 0x007053F0; LS byte of MSB page is 0x00.
  if (id != 0x007053F0) return false;
  if (chip_id) *chip_id = id;
  return true;
}

}  // namespace ht7017
