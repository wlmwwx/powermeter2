#include "NavKey.h"

static const int8_t PIN_NAV_UP    = 8;
static const int8_t PIN_NAV_DOWN  = 13;
static const int8_t PIN_NAV_LEFT  = 5;
static const int8_t PIN_NAV_RIGHT = 9;
static const int8_t PIN_NAV_OK    = 4;

namespace nav {

constexpr uint8_t DEBOUNCE_MS = 30;
constexpr uint16_t COMBO_MS    = 5000;

struct KeyState {
  uint8_t pin;
  bool raw = false;
  bool stable = false;
  bool prevStable = false;
  uint32_t changeMs = 0;
  uint32_t pressStartMs = 0;

  KeyState() = default;
  explicit KeyState(uint8_t p) : pin(p) {}
};

static KeyState kUp   ( PIN_NAV_UP );
static KeyState kDown ( PIN_NAV_DOWN );
static KeyState kLeft ( PIN_NAV_LEFT );
static KeyState kRight( PIN_NAV_RIGHT );
static KeyState kOk   ( PIN_NAV_OK );

static bool edgeUp = false, edgeDown = false, edgeLeft = false, edgeRight = false, edgeOk = false;
static bool comboOkUpLongFired = false;
static uint32_t comboStartMs = 0;

NavKey& NavKey::instance() {
  static NavKey inst;
  return inst;
}

bool NavKey::begin() {
  pinMode(kUp.pin, INPUT_PULLUP);
  pinMode(kDown.pin, INPUT_PULLUP);
  pinMode(kLeft.pin, INPUT_PULLUP);
  pinMode(kRight.pin, INPUT_PULLUP);
  pinMode(kOk.pin, INPUT_PULLUP);

  kUp.raw = digitalRead(kUp.pin) == LOW;
  kDown.raw = digitalRead(kDown.pin) == LOW;
  kLeft.raw = digitalRead(kLeft.pin) == LOW;
  kRight.raw = digitalRead(kRight.pin) == LOW;
  kOk.raw = digitalRead(kOk.pin) == LOW;

  kUp.stable = kUp.raw;
  kDown.stable = kDown.raw;
  kLeft.stable = kLeft.raw;
  kRight.stable = kRight.raw;
  kOk.stable = kOk.raw;

  kUp.prevStable = kUp.stable;
  kDown.prevStable = kDown.stable;
  kLeft.prevStable = kLeft.stable;
  kRight.prevStable = kRight.stable;
  kOk.prevStable = kOk.stable;

  uint32_t now = millis();
  kUp.changeMs = now;
  kDown.changeMs = now;
  kLeft.changeMs = now;
  kRight.changeMs = now;
  kOk.changeMs = now;

  kUp.pressStartMs = kUp.raw ? now : 0;
  kDown.pressStartMs = kDown.raw ? now : 0;
  kLeft.pressStartMs = kLeft.raw ? now : 0;
  kRight.pressStartMs = kRight.raw ? now : 0;
  kOk.pressStartMs = kOk.raw ? now : 0;

  edgeUp = edgeDown = edgeLeft = edgeRight = edgeOk = false;
  comboOkUpLongFired = false;
  comboStartMs = 0;
  return true;
}

static void pollKey(KeyState& k, bool& edgeFlag) {
  bool reading = (digitalRead(k.pin) == LOW);
  uint32_t now = millis();
  if (reading != k.raw) {
    k.raw = reading;
    k.changeMs = now;
  }
  if ((now - k.changeMs) >= DEBOUNCE_MS && k.stable != k.raw) {
    k.stable = k.raw;
    if (k.stable && !k.prevStable) {
      edgeFlag = true;
      k.pressStartMs = now;
    } else if (!k.stable && k.prevStable) {
      k.pressStartMs = 0;
    }
  }
  k.prevStable = k.stable;
}

void NavKey::loop() {
  pollKey(kUp,    edgeUp);
  pollKey(kDown,  edgeDown);
  pollKey(kLeft,  edgeLeft);
  pollKey(kRight, edgeRight);
  pollKey(kOk,    edgeOk);

  bool bothHeld = kOk.stable && kUp.stable;
  if (bothHeld) {
    if (comboStartMs == 0) comboStartMs = millis();
  } else {
    comboStartMs = 0;
  }

  if (bothHeld && comboStartMs != 0 && (millis() - comboStartMs) >= COMBO_MS) {
    comboOkUpLongFired = true;
  }
}

bool NavKey::upPressed()    { bool v = edgeUp;    edgeUp = false;    return v; }
bool NavKey::downPressed()  { bool v = edgeDown;  edgeDown = false;  return v; }
bool NavKey::leftPressed()  { bool v = edgeLeft;  edgeLeft = false;  return v; }
bool NavKey::rightPressed() { bool v = edgeRight; edgeRight = false; return v; }
bool NavKey::okPressed()    { bool v = edgeOk;    edgeOk = false;    return v; }
bool NavKey::okUpLong()     { bool v = comboOkUpLongFired; comboOkUpLongFired = false; return v; }

void NavKey::consume(Event /*e*/) { /* not implemented in T3 path; reserved */ }

}  // namespace nav
