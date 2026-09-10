#pragma once
#include <Arduino.h>
#include <Adafruit_ST7735.h>

namespace meter { struct Data; }

namespace display {

class Display {
 public:
  static Display& instance();

  bool begin();                                                // init SPI + LCD; returns true on success
  void renderSplash();                                         // "Powermeter2 v0.6 / Initializing..."
  void renderHome(const meter::Data& d, bool ch1Relay, bool ch2Relay);  // live metering + "OK=menu" hint
  void renderMenu(int selectedIdx, const char* const* items, int count); // menu list with highlight
  void setBacklight(bool on);
  bool backlight() const { return _bl; }

 private:
  Display() = default;
  Adafruit_ST7735 _tft{7, 6, 3, 2, 10};  // cs, dc, mosi, sclk, rst
  bool _bl = true;
  bool _splashShown = false;
};

}  // namespace display
