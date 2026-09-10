#include <Display.h>
#include <Adafruit_ST7735.h>
#include <Meter.h>

static const int8_t PIN_LCD_SCLK = 2;
static const int8_t PIN_LCD_MOSI = 3;
static const int8_t PIN_LCD_CS   = 7;
static const int8_t PIN_LCD_DC   = 6;
static const int8_t PIN_LCD_RST  = 10;
static const int8_t PIN_LCD_BL   = 11;

namespace display {

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

void Display::renderHome(const meter::Data& d, bool ch1Relay, bool ch2Relay) {
  _tft.fillScreen(ST7735_BLACK);

  char buf[32];

  // Title
  _tft.setTextSize(1);
  _tft.setTextColor(ST7735_WHITE);
  _tft.setCursor(0, 0);
  _tft.println("Powermeter2");

  // U
  snprintf(buf, sizeof(buf), "U=%4.1fV", d.ch1.u);
  _tft.setCursor(0, 20);
  _tft.println(buf);

  // I1
  snprintf(buf, sizeof(buf), "I1=%.3fA", d.ch1.i);
  _tft.setCursor(0, 34);
  _tft.setTextColor(ST7735_CYAN);
  _tft.println(buf);

  // P1 + EP1
  snprintf(buf, sizeof(buf), "P1=%5.1fW EP1=%.2f", d.ch1.p, d.ch1.ep_kwh);
  _tft.setCursor(0, 46);
  _tft.println(buf);

  // I2
  snprintf(buf, sizeof(buf), "I2=%.3fA", d.ch2.i);
  _tft.setCursor(0, 58);
  _tft.setTextColor(ST7735_YELLOW);
  _tft.println(buf);

  // P2 + EP2
  snprintf(buf, sizeof(buf), "P2=%5.1fW EP2=%.2f", d.ch2.p, d.ch2.ep_kwh);
  _tft.setCursor(0, 70);
  _tft.println(buf);

  // Frequency
  snprintf(buf, sizeof(buf), "F=%4.2fHz", d.freq_hz);
  _tft.setTextColor(ST7735_WHITE);
  _tft.setCursor(0, 86);
  _tft.println(buf);

  // Divider
  _tft.drawLine(0, 100, 128, 100, ST7735_WHITE);

  // Relay states
  snprintf(buf, sizeof(buf), "CH1:%3s  CH2:%3s", ch1Relay ? "ON" : "OFF", ch2Relay ? "ON" : "OFF");
  _tft.setCursor(0, 104);
  _tft.println(buf);

  // OK=menu hint
  _tft.setTextColor(ST7735_GREEN);
  _tft.setCursor(0, 145);
  _tft.println("OK=menu");
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
