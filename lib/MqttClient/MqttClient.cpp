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
  doc["ch2"]["ep"] = d.ch2.ep_kwh;
  doc["ch2"]["eq"] = d.ch2.eq_kvarh;
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
