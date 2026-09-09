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
