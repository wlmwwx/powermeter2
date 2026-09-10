#include <Display.h>
#include <cstdarg>
#include <Adafruit_ST7735.h>
#include <Meter.h>

static const int8_t PIN_LCD_SCLK = 2;
static const int8_t PIN_LCD_MOSI = 3;
static const int8_t PIN_LCD_CS   = 7;
static const int8_t PIN_LCD_DC   = 6;
static const int8_t PIN_LCD_RST  = 10;
static const int8_t PIN_LCD_BL   = 11;

namespace display {

Display::Display() {
  for (int p = 0; p < kPageCount; p++) {
    _pageDirty[p] = true;
  }
}

Display& Display::instance() {
  static Display inst;
  return inst;
}

bool Display::begin() {
  _tft.initR(INITR_BLACKTAB);
  _tft.setRotation(1);  // landscape: 128 wide x 160 tall
  _tft.fillScreen(ST7735_BLACK);

  if (PIN_LCD_BL != -1) {
    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH);
  }

  // Probe write to verify SPI bus is responsive.
  // Adafruit_ST7735 does not expose an init error code, so this is
  // best-effort: we write a known pattern and accept that silent failure
  // (e.g. loose cable) cannot be caught without major refactor.
  _tft.setTextColor(ST7735_WHITE);
  _tft.setCursor(0, 0);
  _tft.print("OK");

  _currentPage = PageId::Ch1;
  for (int p = 0; p < kPageCount; p++) {
    _pageDirty[p] = true;
  }
  memset(_lastText, 0, sizeof(_lastText));

  return true;
}

void Display::renderSplash() {
  if (_splashShown) {
    return;
  }

  _tft.fillScreen(ST7735_BLACK);
  _tft.setTextSize(2);
  _tft.setTextColor(ST7735_WHITE);
  _tft.setCursor(10, 40);
  _tft.println("Powermeter2");
  _tft.setTextSize(1);
  _tft.setCursor(10, 80);
  _tft.println("v0.6  Initializing...");

  _splashShown = true;
}

void Display::invalidatePage() {
  int pi = (int)_currentPage;
  _pageDirty[pi] = true;
  memset(_lastText[pi], 0, sizeof(_lastText[pi]));
}

void Display::renderHome(PageId page, const meter::Data& d, bool ch1Relay, bool ch2Relay,
                         int wifiRssi, bool mqttConnected) {
  int pi = (int)page;
  if (page != _currentPage) {
    _currentPage = page;
    _pageDirty[pi] = true;
    memset(_lastText[pi], 0, sizeof(_lastText[pi]));
  }
  if (_pageDirty[pi]) {
    _tft.fillScreen(ST7735_BLACK);
    drawTitle(page);
    drawDivider();
    drawPageIndicator(page);
    drawOkHint();
    _pageDirty[pi] = false;
  }
  drawDataRows(page, d, ch1Relay, ch2Relay, wifiRssi, mqttConnected);
}

void Display::drawTitle(PageId /*page*/) {
  _tft.setTextSize(1);
  _tft.setTextColor(ST7735_WHITE);
  _tft.setCursor(0, 0);
  _tft.println("Powermeter2");
}

void Display::drawDivider() {
  _tft.drawLine(0, 80, 128, 80, ST7735_WHITE);
}

void Display::drawPageIndicator(PageId page) {
  _tft.setTextSize(1);
  int pi = (int)page;
  // Page indicator: "< ch1 | 2/3 | sys >"
  // Current page in GREEN, others in WHITE
  _tft.setCursor(0, 92);

  // "< "
  _tft.setTextColor(ST7735_WHITE);
  _tft.print("< ");

  // "ch1" — Ch1 page is current
  if (pi == (int)PageId::Ch1) {
    _tft.setTextColor(ST7735_GREEN);
  } else {
    _tft.setTextColor(ST7735_WHITE);
  }
  _tft.print("ch1");

  // " | "
  _tft.setTextColor(ST7735_WHITE);
  _tft.print(" | ");

  // "2/3" — Ch2 page is current
  if (pi == (int)PageId::Ch2) {
    _tft.setTextColor(ST7735_GREEN);
  } else {
    _tft.setTextColor(ST7735_WHITE);
  }
  _tft.print("2/3");

  // " | "
  _tft.setTextColor(ST7735_WHITE);
  _tft.print(" | ");

  // "sys" — System page is current
  if (pi == (int)PageId::System) {
    _tft.setTextColor(ST7735_GREEN);
  } else {
    _tft.setTextColor(ST7735_WHITE);
  }
  _tft.print("sys");

  // " >"
  _tft.setTextColor(ST7735_WHITE);
  _tft.print(" >");
}

void Display::drawOkHint() {
  _tft.setTextSize(1);
  _tft.setTextColor(ST7735_GREEN);
  _tft.setCursor(0, 152);
  _tft.print("OK=menu");
}

void Display::drawDataRows(PageId page, const meter::Data& d, bool ch1Relay, bool ch2Relay,
                           int wifiRssi, bool mqttConnected) {
  int pi = (int)page;
  if (page == PageId::Ch1) {
    drawRow(pi, 0, 0, 18, ST7735_WHITE, "U=%4.1fV", d.ch1.u);
    drawRow(pi, 1, 0, 32, ST7735_CYAN, "I1=%.3fA", d.ch1.i);
    drawRow(pi, 2, 0, 46, ST7735_CYAN, "P1=%5.1fW", d.ch1.p);
    drawRow(pi, 3, 0, 60, ST7735_CYAN, "EP1=%.2fkWh", d.ch1.ep_kwh);
  } else if (page == PageId::Ch2) {
    drawRow(pi, 0, 0, 18, ST7735_WHITE, "U=%4.1fV", d.ch2.u);
    drawRow(pi, 1, 0, 32, ST7735_YELLOW, "I2=%.3fA", d.ch2.i);
    drawRow(pi, 2, 0, 46, ST7735_YELLOW, "P2=%5.1fW", d.ch2.p);
    drawRow(pi, 3, 0, 60, ST7735_YELLOW, "EP2=%.2fkWh", d.ch2.ep_kwh);
  } else {  // PageId::System
    drawRow(pi, 0, 0, 18, ST7735_WHITE, "F=%4.2fHz", d.freq_hz);
    drawRow(pi, 1, 0, 32, ST7735_WHITE, "CH1:%3s  CH2:%3s",
            ch1Relay ? "ON " : "OFF", ch2Relay ? "ON " : "OFF");
    drawRow(pi, 2, 0, 46, ST7735_WHITE, "WiFi:%ddBm", wifiRssi);
    drawRow(pi, 3, 0, 60, ST7735_WHITE, "MQTT:%s", mqttConnected ? "on" : "off");
  }
}

bool Display::drawRow(int page, int rowIdx, int x, int y, uint16_t color,
                      const char* fmt, ...) {
  char buf[24];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  if (strcmp(buf, _lastText[page][rowIdx]) == 0) {
    return false;
  }

  _tft.fillRect(x, y, 128, 8, ST7735_BLACK);
  _tft.setCursor(x, y);
  _tft.setTextColor(color);
  _tft.print(buf);
  strncpy(_lastText[page][rowIdx], buf, sizeof(_lastText[page][rowIdx]) - 1);
  _lastText[page][rowIdx][sizeof(_lastText[page][rowIdx]) - 1] = '\0';
  return true;
}

void Display::renderMenu(int selectedIdx, const char* const* items, int count) {
  _tft.fillScreen(ST7735_BLACK);

  // Title
  _tft.setTextSize(1);
  _tft.setTextColor(ST7735_WHITE);
  _tft.setCursor(0, 4);
  _tft.println("Menu");

  const int rowHeight = 14;
  const int startY = 24;

  for (int i = 0; i < count && i < 8; i++) {
    _tft.setCursor(0, startY + i * rowHeight);
    if (i == selectedIdx) {
      _tft.setTextColor(ST7735_GREEN);
      _tft.print("> ");
    } else {
      _tft.setTextColor(ST7735_WHITE);
      _tft.print("  ");
    }
    _tft.println(items[i]);
  }
}

void Display::setBacklight(bool on) {
  _bl = on;
  if (PIN_LCD_BL != -1) {
    digitalWrite(PIN_LCD_BL, on ? HIGH : LOW);
  }
}

}  // namespace display
