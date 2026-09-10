#pragma once
#include <Arduino.h>
#include <Adafruit_ST7735.h>

namespace meter { struct Data; }

namespace display {

class Display {
 public:
  enum class PageId : uint8_t { Ch1 = 0, Ch2 = 1, System = 2 };
  static constexpr int kPageCount = 3;

  static Display& instance();

  bool begin();                                                // init SPI + LCD; returns true on success
  void renderSplash();                                         // "Powermeter2 v0.6 / Initializing..."
  void renderHome(PageId page, const meter::Data& d, bool ch1Relay, bool ch2Relay,
                  int wifiRssi = 0, bool mqttConnected = false);  // live metering + "OK=menu" hint
  void invalidatePage();                                       // force full repaint on next renderHome
  void renderMenu(int selectedIdx, const char* const* items, int count); // menu list with highlight
  void setBacklight(bool on);
  bool backlight() const { return _bl; }

 private:
  Display();
  Adafruit_ST7735 _tft{7, 6, 3, 2, 10};  // cs, dc, mosi, sclk, rst
  bool _bl = true;
  bool _splashShown = false;

  // Dirty redraw state
  PageId _currentPage = PageId::Ch1;
  bool _pageDirty[kPageCount];
  char _lastText[kPageCount][4][24];

  // Helpers
  void drawTitle(PageId page);
  void drawDivider();
  void drawPageIndicator(PageId page);
  void drawOkHint();
  void drawDataRows(PageId page, const meter::Data& d, bool ch1Relay, bool ch2Relay,
                    int wifiRssi, bool mqttConnected);
  bool drawRow(int page, int rowIdx, int x, int y, uint16_t color,
                      const char* fmt, ...);
};

}  // namespace display
