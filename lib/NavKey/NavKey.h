#pragma once
#include <Arduino.h>

namespace nav {

enum class Event : uint8_t {
  None = 0,
  Up, Down, Left, Right, Ok, OkUpLong
};

class NavKey {
 public:
  static NavKey& instance();

  bool begin();
  void loop();

  // momentary edge — true ONCE per press; auto-clears after read
  bool upPressed();
  bool downPressed();
  bool leftPressed();
  bool rightPressed();
  bool okPressed();

  // combo event — true ONCE per OK+UP held >5s; auto-clears
  bool okUpLong();

  // generic event consume (advanced — not used in T3, but available)
  void consume(Event e);

 private:
  NavKey() = default;
};

}  // namespace nav
