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
