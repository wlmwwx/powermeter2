# M8: 菜单脏区域重绘 + 立即刷新 Home Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate menu flicker (1-2 Hz visible artifact) by applying the same dirty-row redraw pattern as M7, and fix the perceived "can't exit Reset" bug by forcing an immediate Home render on the menu→home transition.

**Architecture:** Add a `renderMenu` API on `display::Display` that uses the same `_lastText` dirty-row pattern as `renderHome` — only repaint a menu row when its formatted text differs from the cached version. Move per-iteration menu rebuild from main.cpp's loop into a "dirty flag" — only call `renderMenu` when the menu state actually changed (UP/DOWN/OK/LEFT/relay toggle/backlight toggle/Reset). Also, on menu→home transition, force an immediate `renderHome` by resetting `lastMeterUpdateMs = 0` so the home redraw fires on the next tick (not waiting up to 2 s).

**Tech Stack:** ESP32-C3 + Arduino + PlatformIO. Adafruit ST7735 + Adafruit GFX (existing).

**Spec:** This plan. Originating user report: "按中心键显示wifi/mqtt等的状态时，一直闪烁，且无法回到其他页面".

## Global Constraints

Binding, copied from the user-reported symptoms:

1. **Menu flicker elimination** — apply the M7 dirty-row pattern to `renderMenu`. Each of the 6 menu rows is only repainted when its formatted text changes.
2. **Per-iteration rebuild removal** — main.cpp must NOT call `renderMenu` on every loop tick. Rebuild only on state change (selection change, OK action, LEFT exit).
3. **Immediate Home render on LEFT** — when `leftPressed()` in Menu state transitions `uiState = Home`, force `renderHome` to fire on the next iteration (not after the 2 s meter cadence). Simplest: reset `lastMeterUpdateMs = 0`.
4. **All 6 menu items preserved** — WiFi status, MQTT status, CH1 toggle, CH2 toggle, Backlight toggle, Reset.
5. **Reset behavior preserved** — OK on item 5 still calls `factoryReset(); ESP.restart();`.
6. **No regressions** — Home pages, page nav (LEFT/RIGHT), factory-reset combo, MQTT publish, energy snapshot all unchanged.
7. **Backward-compatible API** — existing `renderMenu(int, const char* const*, int)` consumers keep working. Internally switches to dirty mode automatically.

### Why "perceived can't exit Reset" happens today

When user presses LEFT in Menu, `uiState = Home`. The Home state is only re-rendered inside the 2 s meter cadence block (`if (millis() - lastMeterUpdateMs > 2000)`). If the user just entered Menu and presses LEFT within the 2 s window, the Home render doesn't fire for the remainder of the window — during which the LCD still shows the menu. To the user it looks like LEFT did nothing. Forcing `lastMeterUpdateMs = 0` makes the next `loop()` tick pass the timer check and redraw Home immediately.

This is a separate bug from the flicker (which is the dominant visual artifact). M8 fixes both.

### Display lib API change

`Display::renderMenu` stays with the same signature:
```cpp
void renderMenu(int selectedIdx, const char* const* items, int count);
```

Internally:
- Add private state `_menuDirty = true;` (initial: force full repaint on first call).
- Add private state `char _lastMenuText[6][32] = {};` (cache for 6 rows × 32 chars — covers "WiFi: disconnected" = 16 + slack).
- Add private state `int _lastMenuSel = -1;` (track last selected index; if it changed, the highlight color needs to be redrawn on TWO rows: the old row and the new row).
- Add private helper `static bool drawMenuRow(int rowIdx, int x, int y, uint16_t color, const char* text);` — returns true if text differs from cache and was repainted; updates cache. Pure dirty-row primitive.

### `renderMenu` body

```cpp
void Display::renderMenu(int selectedIdx, const char* const* items, int count) {
  const int rowHeight = 14;
  const int startY = 24;

  // Selection changed: invalidate both the old and new highlight rows
  if (_lastMenuSel != selectedIdx && _lastMenuSel >= 0) {
    _lastMenuText[_lastMenuSel][0] = '\0';  // force redraw of old highlighted row
  }
  if (selectedIdx != _lastMenuSel) {
    _lastMenuText[selectedIdx][0] = '\0';   // force redraw of new highlighted row
    _lastMenuSel = selectedIdx;
  }

  // First call: full clear + title
  if (_menuDirty) {
    _tft.fillScreen(ST7735_BLACK);
    _tft.setTextSize(1);
    _tft.setTextColor(ST7735_WHITE);
    _tft.setCursor(0, 4);
    _tft.println("Menu");
    _menuDirty = false;
  }

  for (int i = 0; i < count && i < 6; i++) {
    if (!items[i]) continue;  // safety
    char prefixed[36];
    if (i == selectedIdx) {
      snprintf(prefixed, sizeof(prefixed), "> %s", items[i]);
    } else {
      snprintf(prefixed, sizeof(prefixed), "  %s", items[i]);
    }
    // Dirty draw: only repaint if text differs
    uint16_t color = (i == selectedIdx) ? ST7735_GREEN : ST7735_WHITE;
    drawMenuRow(i, 0, startY + i * rowHeight, color, prefixed);
  }
}
```

`drawMenuRow` body:
```cpp
bool Display::drawMenuRow(int rowIdx, int x, int y, uint16_t color, const char* text) {
  if (strcmp(text, _lastMenuText[rowIdx]) == 0) return false;
  _tft.fillRect(x, y, 128, 14, ST7735_BLACK);  // clear row band (14 px tall)
  _tft.setCursor(x, y);
  _tft.setTextColor(color);
  _tft.print(text);
  strncpy(_lastMenuText[rowIdx], text, sizeof(_lastMenuText[rowIdx]) - 1);
  _lastMenuText[rowIdx][sizeof(_lastMenuText[rowIdx]) - 1] = '\0';
  return true;
}
```

Note: row height is 14 px (slightly taller than home's 8 px rows) — keep existing `rowHeight` from M6.

### main.cpp menu state changes

Old behavior (M6/M7): every loop tick rebuilds 6 menu items + calls `renderMenu`. New behavior: only rebuild + render when state changes.

```cpp
} else if (uiState == UiState::Menu) {
  bool menuNeedsRedraw = false;

  if (nav::NavKey::instance().upPressed()) {
    menuSel--;
    if (menuSel < 0) menuSel = 0;
    menuNeedsRedraw = true;
  }
  if (nav::NavKey::instance().downPressed()) {
    menuSel++;
    if (menuSel >= 6) menuSel = 5;
    menuNeedsRedraw = true;
  }
  if (nav::NavKey::instance().leftPressed()) {
    uiState = UiState::Home;
    lastMeterUpdateMs = 0;  // force immediate Home redraw on next tick
    // No menu render needed; we just exited
  } else if (nav::NavKey::instance().okPressed()) {
    // Action depends on selected item
    switch (menuSel) {
      case 2:
        ch1Relay = !ch1Relay;
        digitalWrite(PIN_RELAY1, ch1Relay ? HIGH : LOW);
        mqtt::Client::instance().publishState(lastData, ch1Relay, ch2Relay, (int)WiFi.RSSI());
        menuNeedsRedraw = true;  // item 2 text changed
        break;
      case 3:
        ch2Relay = !ch2Relay;
        digitalWrite(PIN_RELAY2, ch2Relay ? HIGH : LOW);
        mqtt::Client::instance().publishState(lastData, ch1Relay, ch2Relay, (int)WiFi.RSSI());
        menuNeedsRedraw = true;
        break;
      case 4:
        display::Display::instance().setBacklight(!display::Display::instance().backlight());
        menuNeedsRedraw = true;
        break;
      case 5:
        cfg::Config::instance().factoryReset();
        ESP.restart();
        return;  // never returns; restart kicks in
      default:
        // items 0 (WiFi status) and 1 (MQTT status) are display-only; no redraw needed
        break;
    }
    if (menuNeedsRedraw) {
      // Rebuild and render the menu
      char menuItemBufs[6][32];
      const char* menuItems[6];
      for (int i = 0; i < 6; i++) menuItems[i] = menuItemBufs[i];
      bool wifiOk = WiFi.status() == WL_CONNECTED;
      bool mqttOk = mqtt::Client::instance().connected();
      snprintf(menuItemBufs[0], 32, "WiFi: %s", wifiOk ? "connected" : "offline");
      snprintf(menuItemBufs[1], 32, "MQTT: %s", mqttOk ? "connected" : "offline");
      snprintf(menuItemBufs[2], 32, "%s", ch1Relay ? "Channel 1: ON" : "Channel 1: OFF");
      snprintf(menuItemBufs[3], 32, "%s", ch2Relay ? "Channel 2: OFF" : "Channel 2: OFF");
      snprintf(menuItemBufs[4], 32, "%s", display::Display::instance().backlight() ? "Backlight: ON" : "Backlight: OFF");
      snprintf(menuItemBufs[5], 32, "%s", "Reset");
      display::Display::instance().renderMenu(menuSel, menuItems, 6);
    }
  }
}
```

**Important:** the helper-rebuild block must ONLY run on `menuNeedsRedraw == true`. The 6 lines of `snprintf` should be wrapped in `if (menuNeedsRedraw) { ... }`.

The `else if` between LEFT and OK is critical — if the user pressed both at once (impossible physically but defensive), LEFT wins (return to Home).

`menuNeedsRedraw` does NOT need to be true for items 0/1 (WiFi/MQTT status) — they're display-only and the user isn't expected to refresh them. If the user wants fresh status, they can press UP then DOWN to force a redraw. (Or scroll past item 0/1 and back.) This is an acceptable trade-off — they don't change often.

If we want to be more user-friendly: after the menu sits idle for >5 s, refresh WiFi/MQTT status. Deferred to v0.9 unless trivial to add.

---

## File Structure

```
lib/Display/Display.h          # MODIFY: add private state + drawMenuRow helper
lib/Display/Display.cpp        # MODIFY: rewrite renderMenu with dirty-row pattern
src/main.cpp                   # MODIFY: event-driven menu redraw + immediate Home on LEFT
docs/PRD.md                    # (no change)
README.md                      # (no change; behavior change is internal)
```

---

## Task Structure

### Task 1: Display lib — dirty-row `renderMenu`

**Files:**
- Modify: `lib/Display/Display.h`
- Modify: `lib/Display/Display.cpp`

**Required Display.h additions to private section:**

```cpp
// Menu dirty-row state
bool _menuDirty = true;
int _lastMenuSel = -1;
char _lastMenuText[6][32] = {};

// Helper
static bool drawMenuRow(int rowIdx, int x, int y, uint16_t color, const char* text);
```

**Required Display.cpp changes:**

1. Replace the existing `renderMenu(int, const char* const*, int)` body with the new dirty-row version (see "renderMenu body" above).
2. Add `drawMenuRow` static member implementation (see "drawMenuRow body" above).
3. In `begin()`: also reset `_menuDirty = true; _lastMenuSel = -1; memset(_lastMenuText, 0, sizeof(_lastMenuText));`. (Re-init safety.)
4. Do NOT change `renderHome`, `renderSplash`, `setBacklight`, `backlight`, or the PageId enum.

**Constraints:**
- The `> ` prefix for the highlighted row and `  ` prefix for others must match exactly. The old code used `_tft.print("> ")` followed by `_tft.println(items[i])`. New code composes the prefixed string into `prefixed[]` and passes the whole thing to `drawMenuRow`.
- The rowHeight constant (14 px) and startY (24) match the M6 values — don't change.
- Title "Menu" at y=4 in ST7735_WHITE — only drawn when `_menuDirty == true` (first call).

**Build verification (T1):** `pio run` will fail until T2 updates main.cpp to NOT call renderMenu per-iteration. This is intentional — the same M7 T1 pattern. The Display library itself should compile in isolation (verifiable by `pio run` failure being ONLY at main.cpp link/edit time, not at Display.cpp compile).

**Conventional Commits:**
- One commit: `feat(display): dirty-row redraw for menu (eliminates flicker)`
- Stage: `lib/Display/Display.h` and `lib/Display/Display.cpp`

**Report file:** `.superpowers/sdd/2026-09-10-m8-menu-dirty-redraw/task-1-report.md`

### Task 2: main.cpp — event-driven menu + immediate Home on LEFT

**Files:**
- Modify: `src/main.cpp`

**Changes:**

1. Replace the menu branch (lines 156-225 in current main.cpp) with the new event-driven version above. Specifically:
   - Drop the bottom per-iteration rebuild-and-render block (lines 211-225).
   - Wrap OK-handler rebuild in `if (menuNeedsRedraw)`.
   - On LEFT in Menu: `uiState = UiState::Home; lastMeterUpdateMs = 0;` (no menu render).
   - Use `else if` between LEFT and OK.
   - For case 5 (Reset): keep `cfg::Config::instance().factoryReset(); ESP.restart(); return;`.

2. No other changes — HOME branch, factory-reset combo, MQTT publish, energy snapshot, setup() all preserved.

**Build verification:** `pio run` must succeed.

**Conventional Commits:**
- One commit: `fix(main): event-driven menu redraw + immediate Home on LEFT`
- Stage: `src/main.cpp` only

**Report file:** `.superpowers/sdd/2026-09-10-m8-menu-dirty-redraw/task-2-report.md`

### Task 3: README + tag v0.8.0 (no separate code change)

**Files:**
- Modify: `README.md`

**README changes:** Add to features list (after the M7 bullet, line 15):
- "Flicker-free menu with dirty-row redraw"

Update "Display & Controls" section to mention menu redraw is event-driven (no flicker).

**Conventional Commits:**
- One commit: `docs(readme): document M8 menu flicker fix`
- Stage: `README.md` only

Then:
```bash
git tag -a v0.8.0 -m "M8: flicker-free menu + immediate Home on LEFT"
```

**Report file:** `.superpowers/sdd/2026-09-10-m8-menu-dirty-redraw/task-3-report.md`

---

## Verification (how to test end-to-end)

**Pre-merge:**
1. `pio run` succeeds.
2. `grep -n "PIN_" src/main.cpp lib/Display/* lib/NavKey/*` — no duplicates (still clean from M6).
3. Verify only expected files changed (Display.h, Display.cpp, main.cpp, README.md).
4. Conventional Commits: 3 commits + tag.

**On-device bring-up (user, manual):**
1. `pio run --target upload`.
2. Power on → splash → PageCh1 (default).
3. RIGHT twice → PageSystem. Wait 5 s — WiFi/MQTT status should NOT flicker on the home page (M7 already proved this).
4. Press OK → menu appears.
5. **Flicker test:** leave menu untouched for 30 s. The display should be COMPLETELY STATIC. Only when WiFi/MQTT status actually changes would items 0/1 refresh (and they shouldn't in 30 s of normal operation).
6. Press DOWN 3 times → cursor moves to item 3 (CH2 toggle). The previously-highlighted row's color and the new row's color should both update. Other rows unchanged.
7. Press LEFT → menu exits to home. **The home page should appear within ~50 ms** (next loop tick, not after 2 s).
8. Press OK again → menu. DOWN to item 4 (Backlight). OK → backlight toggles, item 4 text updates ("Backlight: OFF" instead of "ON"). No other row flickers.
9. DOWN to item 5 (Reset). Press LEFT → menu exits to home (no restart). Verify this — this is the bug the user reported.
10. Press OK on item 5 → device reboots into SoftAP (factory reset still works).

**Edge cases:**
- Press UP/DOWN at boundary (item 0 or item 5): selection clamps, no flicker.
- Press LEFT then immediately press OK: should not double-fire (LEFT is `else if` to OK; once LEFT transitions to Home, we're no longer in Menu state for OK to fire).
- Long-press UP for >1 s to scroll fast: each release-edge increments by 1 (per NavKey behavior). Acceptable.

---

## Open Decisions (resolved)

- [x] **WiFi/MQTT refresh inside menu** — deferred. Display-only items 0/1 don't auto-refresh. User can scroll UP-then-DOWN to force redraw. If we want >5 s idle auto-refresh, defer to v0.9.
- [x] **Per-iteration rebuild removal scope** — complete removal; main.cpp must NOT call renderMenu without a `menuNeedsRedraw == true` condition.
- [x] **lastMeterUpdateMs reset semantics** — only when exiting Menu → Home. Not when entering Menu (don't matter; home was just rendered).

## Tag & Release

```bash
git tag -a v0.8.0 -m "M8: flicker-free menu + immediate Home on LEFT"
```
