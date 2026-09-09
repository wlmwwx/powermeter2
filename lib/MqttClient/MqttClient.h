#pragma once
#include <Arduino.h>
#include <PubSubClient.h>
#include <functional>

namespace meter { struct Data; }

namespace mqtt {

using CmdHandler = std::function<void(const String& jsonPayload)>;

class Client {
 public:
  static Client& instance();

  void begin();
  void loop();

  // Publish current state. Called periodically by main.
  void publishState(const meter::Data& d, bool ch1Relay, bool ch2Relay, int rssi);

  // Publish availability "online"/"offline"
  void publishAvailability(bool online);

  // Set callback for incoming commands
  void onCmd(CmdHandler h) { _onCmd = h; }

  bool connected() const { return _pubsub.connected(); }

 private:
  Client() = default;
  void ensureConnected();
  void onMqttMessage(char* topic, byte* payload, unsigned int len);
  String topic(const char* suffix);

  PubSubClient _pubsub;
  WiFiClient   _wifi;
  CmdHandler   _onCmd;
  uint32_t     _last_publish_ms = 0;
  uint32_t     _last_retry_ms = 0;
  uint16_t    _backoff_s = 2;
};

}  // namespace mqtt
