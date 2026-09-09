#pragma once
#include <Arduino.h>
#include <WebServer.h>  // built-in

namespace web {

class Provisioning {
 public:
  static Provisioning& instance();

  // Starts in SoftAP mode if wifi not configured; otherwise tries STA.
  void begin();
  void loop();

  // Called by main loop when WiFi state transitions.
  void onWifiEvent();

  bool isApMode() const { return _ap_mode; }
  String apSsid() const;
  String apIp() const;

 private:
  Provisioning() = default;
  void startAp();
  void trySta();
  void handleRoot();
  void handleStatus();
  void handleWifiPost();
  void handleMqttPost();
  void handleReset();
  void handleNotFound();
  String indexHtml();

  WebServer _srv{80};
  bool _ap_mode = false;
  uint32_t _sta_attempt_at_ms = 0;
};

}  // namespace web
