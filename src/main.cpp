#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <HT7017.h>
#include <Meter.h>
#include <Config.h>
#include <EnergyStore.h>
#include <Provisioning.h>
#include <MqttClient.h>
#include <Display.h>
#include <NavKey.h>

// Pin map
static const int8_t PIN_HT7017_RX = 20;  // ESP32-C3 RX
static const int8_t PIN_HT7017_TX = 21;
static const int8_t PIN_RELAY1    = 12;
static const int8_t PIN_RELAY2    = 18;

ht7017::HT7017 chip;
meter::Meter meterObj;
meter::CaliParams cali;
meter::Data lastData;
bool ch1Relay = true, ch2Relay = true;

uint32_t lastMeterUpdateMs = 0;
uint32_t lastEnergySaveMs  = 0;

enum class UiState { Home, Menu };
UiState uiState = UiState::Home;
int menuSel = 0;
bool firstRender = true;

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
  pinMode(PIN_RELAY1, OUTPUT); digitalWrite(PIN_RELAY1, HIGH);
  pinMode(PIN_RELAY2, OUTPUT); digitalWrite(PIN_RELAY2, HIGH);

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

  display::Display::instance().begin();
  display::Display::instance().setBacklight(true);
  nav::NavKey::instance().begin();
  display::Display::instance().renderSplash();
}

void loop() {
  web::Provisioning::instance().loop();
  mqtt::Client::instance().loop();
  nav::NavKey::instance().loop();

  // OkUpLong: factory reset from any state
  if (nav::NavKey::instance().okUpLong()) {
    cfg::Config::instance().factoryReset();
    ESP.restart();
  }

  // Meter + display update once per second
  if (millis() - lastMeterUpdateMs > 1000) {
    lastMeterUpdateMs = millis();

    if (meterObj.update(&lastData)) {
      Serial.printf("U=%.1fV I1=%.3fA P1=%.2fW PF1=%.3f EP1=%.3fkWh\n",
                    lastData.ch1.u, lastData.ch1.i, lastData.ch1.p, lastData.ch1.pf, lastData.ch1.ep_kwh);
    }

    if (firstRender) {
      firstRender = false;
      uiState = UiState::Home;
    }

    if (uiState == UiState::Home) {
      display::Display::instance().renderHome(lastData, ch1Relay, ch2Relay);
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

  // UI state machine
  if (uiState == UiState::Home) {
    if (nav::NavKey::instance().okPressed()) {
      uiState = UiState::Menu;
      menuSel = 0;
    }
  } else if (uiState == UiState::Menu) {
    if (nav::NavKey::instance().upPressed()) {
      menuSel--;
      if (menuSel < 0) menuSel = 0;
    }
    if (nav::NavKey::instance().downPressed()) {
      menuSel++;
      if (menuSel >= 6) menuSel = 5;
    }
    if (nav::NavKey::instance().leftPressed()) {
      uiState = UiState::Home;
    }
    if (nav::NavKey::instance().okPressed()) {
      // Rebuild dynamic menu items each time
      char menuItemBufs[6][32];
      const char* menuItems[6];
      for (int i = 0; i < 6; i++) menuItems[i] = menuItemBufs[i];
      bool wifiOk = WiFi.status() == WL_CONNECTED;
      bool mqttOk = mqtt::Client::instance().connected();
      snprintf(menuItemBufs[0], 32, "WiFi: %s", wifiOk ? "connected" : "offline");
      snprintf(menuItemBufs[1], 32, "MQTT: %s", mqttOk ? "connected" : "offline");
      snprintf(menuItemBufs[2], 32, "%s", ch1Relay ? "Channel 1: ON" : "Channel 1: OFF");
      snprintf(menuItemBufs[3], 32, "%s", ch2Relay ? "Channel 2: ON" : "Channel 2: OFF");
      snprintf(menuItemBufs[4], 32, "%s", display::Display::instance().backlight() ? "Backlight: ON" : "Backlight: OFF");
      snprintf(menuItemBufs[5], 32, "%s", "Reset");

      switch (menuSel) {
        case 0:
        case 1:
          // display-only, re-render current menu with refreshed status
          break;
        case 2:
          ch1Relay = !ch1Relay;
          digitalWrite(PIN_RELAY1, ch1Relay ? HIGH : LOW);
          mqtt::Client::instance().publishState(lastData, ch1Relay, ch2Relay, (int)WiFi.RSSI());
          snprintf(menuItemBufs[2], 32, "%s", ch1Relay ? "Channel 1: ON" : "Channel 1: OFF");
          break;
        case 3:
          ch2Relay = !ch2Relay;
          digitalWrite(PIN_RELAY2, ch2Relay ? HIGH : LOW);
          mqtt::Client::instance().publishState(lastData, ch1Relay, ch2Relay, (int)WiFi.RSSI());
          snprintf(menuItemBufs[3], 32, "%s", ch2Relay ? "Channel 2: ON" : "Channel 2: OFF");
          break;
        case 4:
          display::Display::instance().setBacklight(!display::Display::instance().backlight());
          snprintf(menuItemBufs[4], 32, "%s", display::Display::instance().backlight() ? "Backlight: ON" : "Backlight: OFF");
          break;
        case 5:
          cfg::Config::instance().factoryReset();
          ESP.restart();
          break;
      }
      display::Display::instance().renderMenu(menuSel, menuItems, 6);
      return;  // menu rendered, skip below
    }
    // Render menu on each loop iteration while in menu state
    {
      char menuItemBufs[6][32];
      const char* menuItems[6];
      for (int i = 0; i < 6; i++) menuItems[i] = menuItemBufs[i];
      bool wifiOk = WiFi.status() == WL_CONNECTED;
      bool mqttOk = mqtt::Client::instance().connected();
      snprintf(menuItemBufs[0], 32, "WiFi: %s", wifiOk ? "connected" : "offline");
      snprintf(menuItemBufs[1], 32, "MQTT: %s", mqttOk ? "connected" : "offline");
      snprintf(menuItemBufs[2], 32, "%s", ch1Relay ? "Channel 1: ON" : "Channel 1: OFF");
      snprintf(menuItemBufs[3], 32, "%s", ch2Relay ? "Channel 2: ON" : "Channel 2: OFF");
      snprintf(menuItemBufs[4], 32, "%s", display::Display::instance().backlight() ? "Backlight: ON" : "Backlight: OFF");
      snprintf(menuItemBufs[5], 32, "%s", "Reset");
      display::Display::instance().renderMenu(menuSel, menuItems, 6);
    }
  }
}
