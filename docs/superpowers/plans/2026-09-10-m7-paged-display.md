# M7: 分页显示 + 脏区域重绘 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the single overflowing + flickering home screen with a 3-page paged UI (CH1 / CH2 / System) that uses dirty-region redraws to eliminate flicker.

**Architecture:** Add a `renderHomePage(PageId, ...)` API on `display::Display` that owns per-page state (last-rendered string per row) and only repaints the row whose content changed. Increase the home render cadence from 1 s to 2 s. Wire LEFT/RIGHT nav keys to switch pages; OK enters the menu as before. Default page is CH1.

**Tech Stack:** ESP32-C3 + Arduino + PlatformIO. Adafruit ST7735 + Adafruit GFX (existing).

**Spec:** This plan. Originating user request: "测试发现，屏幕显示不全，每隔一两秒刷新一次，显得有点闪烁，能否改进以下，显示不全可以分页显示，通过按键切换，两个通道按两页显示".

## Global Constraints

Binding, copied from the user-confirmed requirements:

- 3 home pages: **PageCh1** / **PageCh2** / **PageSystem**.
- Default page after splash: **PageCh1**.
- LEFT / RIGHT keys cycle pages: PageCh1 → PageSystem → PageCh2 → PageCh1 (wrap).
- OK from any page enters the menu as before.
- Home render cadence changes from **1000 ms** to **2000 ms** (less work, less flicker).
- Redraw strategy: **dirty-region** — only repaint a row whose formatted text differs from the last render of that row on the current page.
- The "OK=menu" hint stays on every page (bottom row, same Y).
- No regressions: menu FSM, MQTT publish, factory-reset combo, all preserved.

### Page layouts

**PageCh1 (default):**
```
Powermeter2
U=220.1V
I1=0.150A
P1=33.0W
EP1=1.23kWh
─────────
< ch1 | 2/3 | sys >
OK=menu
```

**PageCh2:**
```
Powermeter2
U=220.1V
I2=0.000A
P2=0.0W
EP2=0.00kWh
─────────
< ch1 | 2/3 | sys >
OK=menu
```

**PageSystem:**
```
Powermeter2
F=50.00Hz
CH1:ON  CH2:OFF
WiFi:-55dBm
MQTT:on
─────────
< ch1 | 2/3 | sys >
OK=menu
```

Y coordinates (textSize=1, 8 px line height, 128×160 ST7735 in landscape = 128 wide × 160 tall):
- Title: y=0
- Row 1 (U or F): y=18
- Row 2 (I or CH relay): y=32
- Row 3 (P or WiFi): y=46
- Row 4 (EP or MQTT): y=60
- Divider line: y=80
- Page indicator: y=92 (textSize=1)
- OK hint: y=152 (textSize=1)

Format strings (exactly these):
- PageCh1: `"U=%4.1fV"`, `"I1=%.3fA"`, `"P1=%5.1fW"`, `"EP1=%.2fkWh"`
- PageCh2: `"U=%4.1fV"`, `"I2=%.3fA"`, `"P2=%5.1fW"`, `"EP2=%.2fkWh"`
- PageSystem: `"F=%4.2fHz"`, `"CH1:%3s  CH2:%3s"` (ON/OFF), `"WiFi:%ddBm"`, `"MQTT:%s"` (on/off)

Page indicator: shows position in the 3-page cycle, e.g. `< ch1 | 2/3 | sys >`. Use angle brackets to hint that LEFT/RIGHT wraps.

---

## File Structure

```
lib/Display/Display.h          # MODIFY: add PageId enum, change renderHome signature, add page tracking
lib/Display/Display.cpp        # MODIFY: dirty-row redraw, page-aware layout
src/main.cpp                   # MODIFY: 3-page state, LEFT/RIGHT page nav, 2s cadence
docs/PRD.md                    # (no change)
README.md                      # MODIFY: M7 pin/feature updates (separate small commit at end)
```

The Display lib gains internal per-page state (last-rendered row strings) and an `enum class PageId`. The render API changes from `renderHome(const meter::Data&, bool, bool)` to `renderHome(PageId, const meter::Data&, bool, bool)` plus a separate `setBacklight(bool)`. main.cpp holds the current page in a single `int currentPage` variable and updates it on LEFT/RIGHT, then triggers a full redraw of the new page (because each page has its own last-rendered buffer, switching pages must clear those buffers so the new page redraws fully).

---

## Task Structure

### Task 1: Display lib — PageId enum + dirty-row redraw

**Files:**
- Modify: `lib/Display/Display.h`
- Modify: `lib/Display/Display.cpp`

**Interfaces (changes):**

`Display.h` gains:
```cpp
enum class PageId : uint8_t { Ch1 = 0, Ch2 = 1, System = 2 };
static constexpr int kPageCount = 3;
```

`Display.h` `renderHome` signature changes from:
```cpp
void renderHome(const meter::Data& d, bool ch1Relay, bool ch2Relay);
```
to:
```cpp
void renderHome(PageId page, const meter::Data& d, bool ch1Relay, bool ch2Relay);
```

`Display.h` adds a new public method:
```cpp
void invalidatePage();  // force full repaint on next renderHome (call after page switch)
```

`Display.h` removes `void renderSplash()` only if T1 was the only consumer — leave it in place, it's still used at boot.

`Display.cpp` private state additions:
```cpp
// Per-page, per-row "last rendered text" cache so dirty-row redraw works.
// 3 pages × 4 rows × 24 chars covers the longest format string ("MQTT:disconnected" = 16 chars OK).
static char _lastText[3][4][24];
static bool _pageDirty[3] = { true, true, true };  // first render: full repaint
static PageId _lastPage = PageId::Ch1;
```

`Display.cpp` new private helper:
```cpp
// Returns true if the formatted text for this row differs from cached; updates cache.
static bool drawRow(int rowIdx, int x, int y, const char* fmt, ...);
// Implementation: snprintf into a tmp buffer, strcmp with _lastText[_lastPage][rowIdx].
// If different: fillRect(x, y, 128, 8, BLACK) then setCursor+print, then memcpy into cache.
// If same: no-op.
```

`Display.cpp` `renderHome(PageId page, ...)` new body:

For each page, draw 4 rows:
- PageCh1: rows are U, I1, P1, EP1 (from `d.ch1.u`, `d.ch1.i`, `d.ch1.p`, `d.ch1.ep_kwh`).
- PageCh2: rows are U, I2, P2, EP2 (from `d.ch2.u`, `d.ch2.i`, `d.ch2.p`, `d.ch2.ep_kwh`).
- PageSystem: rows are F (`d.freq_hz`), relay line (`"CH1:%3s  CH2:%3s"`, ch1Relay/ch2Relay as ON/OFF), WiFi (`WiFi.RSSI()` — but Display can't include WiFi.h cleanly; pass it in or change signature).

**Important design call:** PageSystem needs WiFi RSSI and MQTT connected status. The Display lib stays dependency-light. Extend the signature:

```cpp
void renderHome(PageId page, const meter::Data& d, bool ch1Relay, bool ch2Relay,
                int wifiRssi, bool mqttConnected);
```

This is a breaking signature change. main.cpp is the only caller; one place to update. Use default arg `wifiRssi = 0, mqttConnected = false` to keep header-friendly for future mock/test usage (still useful even if no tests exist).

Title + divider + page indicator + OK hint: always redrawn at full cadence OR — for full flicker elimination — also dirty. Title never changes, divider never changes, page indicator only changes when currentPage changes (handled in main by calling `invalidatePage()` after page switch). OK hint never changes. So:

- Title: redrawn ONCE per page on first render (when `_pageDirty[page]` is true), then never again on that page.
- Divider: same as title.
- Page indicator: redrawn only on page switch (handled via invalidatePage).
- OK hint: redrawn only on page switch.

`_pageDirty[page]` semantics: when true, force a full repaint of the static parts (title + divider + indicator + OK hint) plus the 4 data rows. When false, only the 4 data rows run through `drawRow` dirty check.

`invalidatePage()` implementation:
```cpp
void Display::invalidatePage() {
  for (int p = 0; p < kPageCount; p++) _pageDirty[p] = true;
  // Wipe per-page text cache so the new page redraws fully even if it has different row strings.
  memset(_lastText, 0, sizeof(_lastText));
}
```

Wait — invalidating ALL pages on a page switch is wasteful. Better:
- Track `_currentPage` in Display (separate from `_lastPage` which records what was last drawn).
- On `renderHome(page, ...)`: if `page != _currentPage`, set `_pageDirty[(int)page] = true` and wipe `_lastText[(int)page]` so the new page redraws from scratch. Then set `_currentPage = page`.

Cleaner. Implementation:
```cpp
void Display::renderHome(PageId page, ...) {
  int pi = (int)page;
  if (page != _currentPage) {
    _currentPage = page;
    _pageDirty[pi] = true;
    memset(_lastText[pi], 0, sizeof(_lastText[pi]));
  }
  if (_pageDirty[pi]) {
    _tft.fillScreen(ST7735_BLACK);          // full clear (cheap; happens once per page entry)
    drawTitle(page);                         // "Powermeter2"
    drawDivider();
    drawPageIndicator(page);                 // "< ch1 | 2/3 | sys >"
    drawOkHint();                            // "OK=menu"
    _pageDirty[pi] = false;
  }
  // Always run dirty check on the 4 data rows (cheap; 4 strcmps + maybe 4 short SPI writes).
  drawDataRows(page, d, ch1Relay, ch2Relay, wifiRssi, mqttConnected);
}
```

The title/dividend/indicator/OK hint are NOT dirty-tracked — they only redraw on page entry. This is correct because they are static per page.

`drawDataRows` implementation: 4 calls to `drawRow(0..3, x, y_row, fmt, ...)`.

`drawRow` implementation (the dirty primitive):
```cpp
static bool drawRow(int rowIdx, int x, int y, const char* fmt, ...) {
  char buf[24];
  va_list ap; va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  int pi = (int)Display::_currentPage;  // need access; expose or make friend
  if (strcmp(buf, _lastText[pi][rowIdx]) == 0) return false;
  _tft.fillRect(x, y, 128, 8, ST7735_BLACK);
  _tft.setCursor(x, y);
  _tft.setTextColor(<color per row/page>);
  _tft.print(buf);
  strncpy(_lastText[pi][rowIdx], buf, sizeof(_lastText[pi][rowIdx]) - 1);
  _lastText[pi][rowIdx][sizeof(_lastText[pi][rowIdx]) - 1] = '\0';
  return true;
}
```

For static helpers, we need a way to get the current page. Either:
- Pass `page` explicitly to `drawRow`. Cleaner.
- Or make them private member functions of `Display`. Even cleaner.

Recommendation: make `drawRow`, `drawTitle`, `drawDivider`, `drawPageIndicator`, `drawOkHint`, `drawDataRows` private member functions of `Display` (declared in the header private section). They have access to `_currentPage` and `_tft`.

But `drawRow` is parameterized by row index and y coordinate, not page — `drawDataRows` knows the row→y mapping. Cleaner to keep `drawRow` static and pass `page` in. Hybrid:
- `static bool drawRow(int page, int rowIdx, int x, int y, uint16_t color, const char* fmt, va_list ap)` — does the strcmp/fillRect/print work.
- `void drawDataRows(PageId page, const meter::Data& d, bool ch1Relay, bool ch2Relay, int wifiRssi, bool mqttConnected)` — member that calls drawRow with the right format strings per page.

The `static` helpers need access to `_tft` and `_lastText` — so they live as static member functions of `Display` (with internal linkage if needed). C++ allows `static` member functions to access private static members.

Layout final:
```cpp
class Display {
 public:
  enum class PageId : uint8_t { Ch1 = 0, Ch2 = 1, System = 2 };
  static constexpr int kPageCount = 3;

  static Display& instance();
  bool begin();
  void renderSplash();
  void renderHome(PageId page, const meter::Data& d, bool ch1Relay, bool ch2Relay,
                  int wifiRssi = 0, bool mqttConnected = false);
  void renderMenu(int selectedIdx, const char* const* items, int count);
  void setBacklight(bool on);
  bool backlight() const { return _bl; }

 private:
  Display() = default;
  Adafruit_ST7735 _tft{7, 6, 3, 2, 10};
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
  static bool drawRow(int page, int rowIdx, int x, int y, uint16_t color,
                      const char* fmt, ...);
};
```

In the .cpp:
- Initialize `_pageDirty` to `{ true, true, true }` in the constructor body (since the in-class initializer for a C-style array isn't supported in C++11/14 in all toolchains — use `for` loop in ctor).
- `_lastText` zero-init via `{}` is fine.
- In `begin()`: also reset `_pageDirty` and `_lastText` so a re-init (if any) starts clean.

### Task 2: main.cpp — 3-page state + 2s cadence + LEFT/RIGHT nav

**Files:**
- Modify: `src/main.cpp`

**Changes:**

1. Add `int currentPage = 0;` global (or static). The page id is just an int 0..2 mapping to `(PageId)currentPage`.

2. `renderHome` call signature updates to pass `wifiRssi` and `mqttConnected`:
   ```cpp
   display::Display::instance().renderHome(
     (display::Display::PageId)currentPage,
     lastData, ch1Relay, ch2Relay,
     (int)WiFi.RSSI(),
     mqtt::Client::instance().connected());
   ```

3. Cadence change: `if (millis() - lastMeterUpdateMs > 1000)` → `> 2000`.

4. In HOME state branch, add LEFT/RIGHT page handling:
   ```cpp
   if (uiState == UiState::Home) {
     if (nav::NavKey::instance().leftPressed()) {
       currentPage--;
       if (currentPage < 0) currentPage = 2;  // wrap
     }
     if (nav::NavKey::instance().rightPressed()) {
       currentPage++;
       if (currentPage > 2) currentPage = 0;  // wrap
     }
     if (nav::NavKey::instance().okPressed()) {
       uiState = UiState::Menu;
       menuSel = 0;
     }
   }
   ```

5. Drop the splash-then-jump-to-home delay: the 2-second render cadence gives enough time for splash visibility, but if the implementer wants splash longer, they can hold `firstRender` for one extra tick. Optional; default behavior is fine.

6. Meter update: still at 2 s, display render at 2 s — they coincide (acceptable, single timer).

### Task 3: README update + tag v0.7.0

**Files:**
- Modify: `README.md`
- Create tag `v0.7.0`

After all 3 tasks land and pass review:

```bash
git tag -a v0.7.0 -m "M7: paged display + dirty-region redraw"
```

README changes:
- Top one-liner: add "with 3-page LCD display"
- Add to features: "3-page LCD (CH1 / CH2 / System) with dirty-region redraw — no flicker"
- Add to Display & Controls: page navigation (LEFT/RIGHT cycles), per-page content description.

---

## Verification (how to test end-to-end)

**Pre-merge:**
1. `pio run` succeeds.
2. `grep -n "PIN_" src/main.cpp lib/Display/* lib/NavKey/*` — no duplicates (should still be clean from M6).
3. Static check: `_lastText` and `_pageDirty` arrays are sized for 3 pages × 4 rows.
4. Verify with `git diff --stat` that ONLY the expected files changed (Display.h, Display.cpp, main.cpp, README.md).
5. Conventional Commits: 3 commits (T1: feat(display), T2: feat(main), T3: docs(readme) + tag).

**On-device bring-up (user, manual):**
1. `pio run --target upload`.
2. Power on → splash → PageCh1 (default).
3. Press RIGHT → PageCh2.
4. Press RIGHT → PageSystem (F / relays / WiFi / MQTT).
5. Press RIGHT → PageCh1 (wrap).
6. Press LEFT → PageSystem (wrap backward).
7. Watch values for 30 s — verify NO flicker (the data rows only repaint when the formatted text changes, which for stable values should be ~never).
8. Change a load → current/power values update within 2 s, only the changed row redraws.
9. Press OK → menu still works (no regression).
10. Hold OK + UP 6 s → factory reset still works (no regression).

**Edge cases the implementer must consider:**
- First render of each page: title + divider + indicator + OK hint all drawn; 4 data rows drawn.
- Subsequent renders: 4 data rows run dirty check; static parts untouched.
- Page switch: force `fillScreen` + static parts + data rows of new page.
- Backlight toggle from menu: doesn't reset dirty state (good — flicker-free persists).

---

## Open Decisions (resolved)

- [x] **Page indicator format** — `"< ch1 | 2/3 | sys >"`. Wrap-around hinted by the angle brackets at the ends.
- [x] **Wrap behavior** — both LEFT (3→2→1→0→3... actually no: 0→3→2→1→0) and RIGHT (0→1→2→3→0) wrap.
- [x] **Cadence** — 2 s (was 1 s). Less work, less perceived flicker.
- [x] **Meter update cadence** — also 2 s (was 1 s). Meter and display share a timer.
- [x] **MQTT publish cadence** — UNCHANGED. The 5-second periodic publish in main.cpp uses its own timer (`publishState` is called every loop iteration, internally throttled by PubSubClient wrapper).

## Tag & Release

After all 3 tasks land and pass review:

```bash
git tag -a v0.7.0 -m "M7: paged display + dirty-region redraw"
```

README gets a small commit.
