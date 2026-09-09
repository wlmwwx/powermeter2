#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <HT7017.h>
#include <Meter.h>
#include <Config.h>
#include <EnergyStore.h>
#include <Provisioning.h>
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
  auto cur = cfg::Config::instance().device();
  if (cur.device_id[0] == 0) {
    uint64_t mac = ESP.getEfuseMac();
    snprintf(cur.device_id, sizeof(cur.device_id), "%06llX", (unsigned long long)(mac & 0xFFFFFF));
    cfg::Config::instance().setDevice(cur);
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
    Serial.printf("Restored energy EP1=%.3f EQ1=%.3f EP2=%.3f EQ2=%.3f\n",
                  es.ep1_kwh, es.eq1_kvarh, es.ep2_kwh, es.eq2_kvarh);
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
    s.ep2_kwh = lastData.ch2.ep_kwh;
    s.eq2_kvarh = lastData.ch2.eq_kvarh;
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
