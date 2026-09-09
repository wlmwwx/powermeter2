#include "Provisioning.h"
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
