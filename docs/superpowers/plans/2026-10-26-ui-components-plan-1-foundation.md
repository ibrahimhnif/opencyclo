# UI Components & Theme Engine — Plan 1: Foundation, Components, Ride & Power Menus

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the firmware a component library (`button`, `label`, `value_tile`, `hero_value`, `page_header`) that draws only from a theme style sheet, rebuild the dashboard, ride menu, power menu, and map buttons on it, and route every UI side effect through one `actions.cpp` — with the default look unchanged.

**Architecture:** `uitheme::StyleSheet` (pure data) + `UiTheme` colors resolve into a `ui::ResolvedTheme` (RGB565 + font pointers). Components take `(Gfx&, rect, props, StyleId, const ResolvedTheme&)` and touch no globals. Screens compute a pure view (`ride_view.h`, new `power_view.h`, `widget_model`), draw with components, and send taps to `dispatchAction()`. Parity is proven by recorded draw-command goldens from the existing host fake canvas.

**Tech Stack:** C++ (Arduino/ESP32-S3, LovyanGFX), host tests compiled by `tests/run_*.py` with `c++`, PlatformIO for the firmware build.

**Spec:** `docs/superpowers/specs/2026-10-26-ui-components-and-theme-engine-design.md` — this plan covers spec build-order phases 0–3. Plan 2 = phase 4 (settings, camera, sensor manager, charging screen; `list`/`toggle`/`chart`/`battery` components). Plan 3 = phases 5–6 (page documents, theme v2 JSON, protocol, web editor).

## Global Constraints

- Default look must not change: the dashboard, header and ride-menu golden draw streams must stay byte-identical. The only golden allowed to change is the new power-menu golden. Any other change needs before/after previews in the PR for the owner to approve.
- No heap allocation in render/touch paths; fixed-size structs only.
- Components never read `g_settings`, `g_theme`, telemetry, or BLE. Everything arrives as arguments.
- Code compiled by `tests/run_ride_tests.py` builds with `-std=c++11 -Wall -Wextra -Werror` (C++11: no generic lambdas, no C++14 `constexpr` bodies). Code compiled by `tests/run_native_tests.py` / `[env:native]` builds with C++14.
- Host test runs on this Mac need `SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk` (without it, the link fails with `tapi error: malformed file`). Every `python3 tests/run_*.py` command below assumes it is exported.
- Firmware build gate: `pio run -e esp32-s3-devkitc-1` succeeds.
- Match the surrounding code style. `src/ui/` uses dense one-line style in `ride_*`/`layout_manager`, and normal style in `engine/theme*`/`widget_model`. New files under `src/ui/components/` use the normal style of `theme.cpp`.
- Commit messages follow the repo's `type(scope): summary` style and end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. **Theme pushed mid-frame (Web Serial task writes `g_theme` while the UI task draws).** Expected: the UI never resolves a half-written theme. It re-resolves only when the version is stable before and after the copy. Owned by Task 2 (`test_resolved_theme`: version unchanged → no re-resolve; version bumped → new colors).
2. **Imperial units on every tile.** Expected: altitude/ascent labels read `alt (ft)` / `asc (ft)` with feet values; avg/max read mph. Owned by Task 4 (`test_widget_model` imperial cases).
3. **Missing sensor values (HR/power = -1, `altitude_valid=false`, NaN distance).** Expected: today's panel text, `0` / `--` / `0.0`, not garbage. Owned by Task 4 (widget model) and Task 6 (golden `long_values` plus a NaN case).
4. **The screen state changes while a finger is down** (ride saves mid-press, a power button press under the finger). Expected: the release fires nothing, or only the action that was pressed. Owned by Task 5 (`test_press_tracker`).
5. **Long text.** Expected: a page title longer than 144 px stays clipped to 48..192, and an oversize battery/satellite count is clamped (`100%`, `GPS 99`). Owned by Task 3 (`test_components`: header clip commands, status clamping).

---

## File Structure

| File | Responsibility |
|---|---|
| `src/ui/gfx.h` (new) | `Gfx` / `GfxFont` aliases. Device: LovyanGFX. Host: substituted by fakes. |
| `src/ui/engine/theme.h/.cpp` (modify) | Adds `ColorToken`, `FontId`, `StyleId`, `ComponentStyle`, `StyleSheet`, default sheet, validation, `g_theme_version`. |
| `src/ui/engine/resolved_theme.h/.cpp` (new) | Resolves theme + sheet into RGB565/fonts; active-theme refresh with version check. |
| `src/ui/components/label.*`, `button.*`, `value_tile.*`, `hero_value.*`, `page_header.*` (new) | One component each. Pure drawing. |
| `src/ui/engine/widget_model.h/.cpp` (modify) | All dashboard tile formatters + status-bar model. |
| `src/ui/actions.h/.cpp` (new) | `ActionId`, `dispatchAction()`, the only UI→logic calls. |
| `src/ui/press_tracker.h`, `src/ui/gesture.h` (new) | Shared press/release and swipe/tap rules. |
| `src/ui/power_view.h`, `src/ui/power_screen.h/.cpp` (new) | Pure power-menu view and drawing. |
| `tests/golden/*.txt` (new) | Recorded draw streams for parity. |

---

### Task 0: Commit the working-tree base

**Files:** the currently modified and untracked firmware/web/test/doc files from `git status`, **excluding** `Android-SDK-2.1.5.zip` and `work/`.

- [ ] **Step 1: Run the suites on the current tree**

Run: `python3 tests/run_native_tests.py && python3 tests/run_ride_tests.py && python3 tests/run_navigation_tests.py && pio run -e esp32-s3-devkitc-1`
Expected: every runner prints its "passed" lines and the build ends with `SUCCESS`. If anything fails, stop and report it. Don't fix it under this plan.

- [ ] **Step 2: Commit**

```bash
git add README.md platformio.ini src tests tools docs/browser-companion.md docs/insta360-camera-control.md web
git status --short   # confirm Android-SDK-2.1.5.zip and work/ are still untracked
git commit -m "feat(ui): theme store, widget models, web companion and camera control base"
```

---

### Task 1: Golden draw-stream parity harness

**Files:**
- Modify: `tests/run_ride_tests.py`
- Create: `tests/golden/ride.txt`, `tests/golden/pages.txt`, `tests/golden/sensors.txt`, `tests/golden/camera.txt`

**Interfaces:**
- Produces: `run_ride_tests.py` always runs the four preview binaries (`test_ride_preview`, `page_ui preview`, `sensor_ui preview`, `camera_ui preview`) and compares stdout byte-for-byte to `tests/golden/<name>.txt`. With `UPDATE_GOLDEN=1` it writes the files instead. The existing `argv[1]` PNG-preview behavior is unchanged. Later tasks add goldens by appending `(name, binary, args)` to one list in this file.

- [ ] **Step 1: Add the comparison**

One helper, `check_golden(name, binary, args)`. On mismatch it prints a unified diff (`difflib`) of the first 40 differing lines and exits non-zero. Build the ride preview binary unconditionally, not only when `argv[1]` is given. The final line printed on success is `golden: ride, pages, sensors, camera match`.

- [ ] **Step 2: Record the goldens from the untouched code**

Run: `UPDATE_GOLDEN=1 python3 tests/run_ride_tests.py`, then `python3 tests/run_ride_tests.py`
Expected: four files are created, and the second run prints `golden: ride, pages, sensors, camera match`.

- [ ] **Step 3: Prove the check bites**

Temporarily change `"Ready to ride"` in `src/ui/ride_view.h` to `"Ready to ridE"` and run `python3 tests/run_ride_tests.py`.
Expected: a non-zero exit, with a diff showing the `T|...|Ready to ride` line. Revert the change.

- [ ] **Step 4: Commit** — `test(ui): golden draw-stream parity for ride, pages, sensors, camera`

---

### Task 2: Style sheet, `Gfx` alias, and resolved theme

**Files:**
- Create: `src/ui/gfx.h`, `src/ui/engine/resolved_theme.h`, `src/ui/engine/resolved_theme.cpp`, `tests/test_resolved_theme.cpp`
- Modify: `src/ui/engine/theme.h`, `src/ui/engine/theme.cpp`, `src/storage/theme_config.cpp`, `tests/test_theme.cpp`, `tests/ride_fakes.h`, `tests/navigation_fakes.h`, `tests/run_ride_tests.py`, `tests/run_navigation_tests.py`

**Interfaces:**
- Produces (`src/ui/gfx.h`, device side):
  ```cpp
  #include "hardware/display.h"
  using Gfx = lgfx::LovyanGFX;
  using GfxFont = const lgfx::IFont*;
  ```
  The host runners substitute `ui/gfx.h` with their fake header. `ride_fakes.h` and `navigation_fakes.h` gain `using Gfx = Canvas;` / `using Gfx = FakeCanvas;` and `using GfxFont = const int*;`. Add any draw methods the components call that a fake lacks: `drawRoundRect` and `setClipRect` on `FakeCanvas`, recording nothing new.
- Produces (`theme.h`, namespace `uitheme`):
  ```cpp
  enum ColorToken : uint8_t { TOK_BG, TOK_PANEL, TOK_TEXT, TOK_LABEL, TOK_HAIRLINE, TOK_ACCENT,
    TOK_SUCCESS, TOK_WARNING, TOK_DANGER, TOK_PRESSED, TOK_ACTIVE, TOK_COUNT };
  enum FontId : uint8_t { FONT_GLCD, FONT_SANS_BOLD_9, FONT_SANS_BOLD_12, FONT_SANS_BOLD_24, FONT_ID_COUNT };
  enum StyleId : uint8_t { STYLE_TEXT, STYLE_TEXT_MUTED, STYLE_CAPTION, STYLE_VALUE_XL,
    STYLE_TILE_LABEL, STYLE_TILE_VALUE, STYLE_HERO_LABEL, STYLE_HERO_VALUE, STYLE_HEADER,
    STYLE_PANEL_TITLE, STYLE_PANEL_TEXT, STYLE_SCREEN_TITLE, STYLE_SCREEN_TEXT,
    STYLE_BUTTON_PRIMARY, STYLE_BUTTON_SECONDARY, STYLE_BUTTON_PAGE_PRIMARY,
    STYLE_BUTTON_PAGE_SECONDARY, STYLE_BUTTON_POWER, STYLE_BUTTON_MAP, STYLE_BUTTON_MAP_ACTIVE,
    STYLE_COUNT };
  constexpr int8_t ICON_CENTER = -1;   // icon_dx: center horizontally when the button has no text
  struct ComponentStyle { ColorToken bg, fg, icon, border; uint8_t border_w, radius;
    FontId font; uint8_t text_size; int8_t inset, icon_dx, icon_dy, text_dx, text_dy; };
  #define UI_STYLE_SCHEMA_VERSION 1u
  struct StyleSheet { uint32_t schema_version; Rgb888 pressed, active; ComponentStyle styles[STYLE_COUNT]; };
  const StyleSheet& defaultStyleSheet();
  bool isStyleSheetValid(const StyleSheet& s);  // schema, every token < TOK_COUNT, font < FONT_ID_COUNT, text_size 1..4
  ```
  `UiTheme`, its JSON, and `UI_THEME_SCHEMA_VERSION` stay unchanged, so stored NVS themes survive. `font_scale_pct` stays (unused) until Plan 3.
  `g_theme_version` lives in `theme.cpp` next to `g_theme`: `std::atomic<uint32_t> g_theme_version{0};`, declared in `theme_config.h`. `importThemeFromString` and `resetThemeToDefault` increment it after writing `g_theme`.
- Produces (`resolved_theme.h`, namespace `ui`):
  ```cpp
  struct ResolvedStyle { uint16_t bg, fg, icon, border; uint8_t border_w, radius, text_size;
    GfxFont font; int8_t inset, icon_dx, icon_dy, text_dx, text_dy; };
  struct ResolvedTheme { uint16_t colors[uitheme::TOK_COUNT]; ResolvedStyle styles[uitheme::STYLE_COUNT];
    uint16_t toneColor(wmodel::Tone tone, uitheme::StyleId fallback) const; };
  void resolveTheme(const uitheme::UiTheme& t, const uitheme::StyleSheet& s, ResolvedTheme& out);
  // Re-resolves only when `version` differs from the last resolved one AND
  // equals versionAfter (read again after copying `t`). Returns true if it re-resolved.
  bool refreshActiveTheme(const uitheme::UiTheme& t, uint32_t version, uint32_t versionAfter);
  const ResolvedTheme& activeTheme();   // resolved from defaults before the first refresh
  GfxFont fontFor(uitheme::FontId id);  // GLCD→&fonts::Font0, 9→FreeSansBold9pt7b, 12→…12pt7b, 24→…24pt7b
  ```
  `toneColor` maps `TONE_TEXT→TOK_TEXT, ACCENT→ACCENT, SUCCESS→SUCCESS, WARNING→WARNING, DANGER→DANGER, TONE_MUTED→TOK_LABEL`. `TONE_STYLE` returns `styles[fallback].fg`. **Task 4 adds `TONE_MUTED` and `TONE_STYLE = 255` to `wmodel::Tone`. Add them here instead (append after `TONE_DANGER`) so this task compiles.**

Default sheet values (these reproduce today's literals; `{bg, fg, icon, border, border_w, radius, font, size, inset, icon_dx, icon_dy, text_dx, text_dy}`). `pressed = {128,128,128}` (=`0x8410`), `active = {0,68,128}` (=`0x0230`):

| Style | Values |
|---|---|
| TEXT | BG, TEXT, TEXT, BG, 0,0, SANS_BOLD_9, 1, 0,0,0,0,0 |
| TEXT_MUTED | BG, LABEL, LABEL, BG, 0,0, SANS_BOLD_9, 1, 0… |
| CAPTION | BG, LABEL, LABEL, BG, 0,0, GLCD, 1, 0… |
| VALUE_XL | BG, TEXT, TEXT, BG, 0,0, SANS_BOLD_24, 1, 0… |
| TILE_LABEL | BG, LABEL, LABEL, HAIRLINE, 1,0, SANS_BOLD_9, 1, 0,0,0, 4,4 |
| TILE_VALUE | BG, TEXT, TEXT, HAIRLINE, 1,0, SANS_BOLD_12, 1, 0,0,0, 4,24 |
| HERO_LABEL | BG, LABEL, LABEL, BG, 0,0, SANS_BOLD_9, 1, 0,0,0, 4,4 |
| HERO_VALUE | BG, TEXT, TEXT, BG, 0,0, SANS_BOLD_24, 1, 0,0,0, 4,24 |
| HEADER | BG, TEXT, ACCENT, BG, 0,0, SANS_BOLD_9, 1, 0,0,0, 48,12 |
| PANEL_TITLE | PANEL, TEXT, TEXT, PANEL, 0,8, SANS_BOLD_12, 1, 0… |
| PANEL_TEXT | PANEL, TEXT, TEXT, PANEL, 0,8, SANS_BOLD_9, 1, 0… |
| SCREEN_TITLE | BG, TEXT, TEXT, BG, 0,0, GLCD, 2, 0… |
| SCREEN_TEXT | BG, TEXT, TEXT, BG, 0,0, GLCD, 1, 0… |
| BUTTON_PRIMARY | ACCENT, BG, BG, ACCENT, 0,8, SANS_BOLD_12, 1, 0, 12,12, 48,13 |
| BUTTON_SECONDARY | PANEL, TEXT, TEXT, PANEL, 0,8, SANS_BOLD_12, 1, 0, 12,12, 48,13 |
| BUTTON_PAGE_PRIMARY | ACCENT, BG, BG, ACCENT, 0,8, SANS_BOLD_9, 1, 0, 8,13, 40,16 |
| BUTTON_PAGE_SECONDARY | PANEL, TEXT, TEXT, PANEL, 0,8, SANS_BOLD_9, 1, 0, 8,13, 40,16 |
| BUTTON_POWER | PANEL, TEXT, TEXT, PANEL, 0,8, GLCD, 2, 0, 12,10, 48,14 |
| BUTTON_MAP | PANEL, TEXT, ACCENT, PANEL, 0,8, GLCD, 1, 2, 5,10, 34,18 |
| BUTTON_MAP_ACTIVE | ACTIVE, TEXT, ACCENT, ACTIVE, 0,8, GLCD, 1, 2, 5,10, 34,18 |

(Map controls center the icon when the button has no text. `icon_dx`, `text_dx`, and so on are relative to the inset rect.)

- [ ] **Step 1: Write failing tests**

`tests/test_theme.cpp` (native runner) appends:
```cpp
assert(isStyleSheetValid(defaultStyleSheet()));
StyleSheet bad = defaultStyleSheet(); bad.styles[STYLE_TEXT].fg = ColorToken(TOK_COUNT); assert(!isStyleSheetValid(bad));
bad = defaultStyleSheet(); bad.styles[STYLE_TEXT].font = FontId(FONT_ID_COUNT); assert(!isStyleSheetValid(bad));
bad = defaultStyleSheet(); bad.styles[STYLE_TEXT].text_size = 0; assert(!isStyleSheetValid(bad));
bad = defaultStyleSheet(); bad.schema_version = 0; assert(!isStyleSheetValid(bad));
assert(toRgb565(defaultStyleSheet().pressed) == 0x8410 && toRgb565(defaultStyleSheet().active) == 0x0230);
```
`tests/test_resolved_theme.cpp` (new binary in `run_ride_tests.py`, compiling `theme.cpp` + `resolved_theme.cpp`):
```cpp
ResolvedTheme r; resolveTheme(kDefaultDarkTheme, defaultStyleSheet(), r);
assert(r.colors[TOK_ACCENT] == ui::accent && r.colors[TOK_PANEL] == ui::panel && r.colors[TOK_TEXT] == 0xffff && r.colors[TOK_BG] == 0);
assert(r.styles[STYLE_BUTTON_PRIMARY].bg == ui::accent && r.styles[STYLE_BUTTON_PRIMARY].fg == 0);
assert(*r.styles[STYLE_TILE_VALUE].font == fonts::FreeSansBold12pt7b);
assert(r.toneColor(wmodel::TONE_DANGER, STYLE_TEXT) == ui::danger && r.toneColor(wmodel::TONE_STYLE, STYLE_TEXT_MUTED) == ui::muted);
// version rules
assert(refreshActiveTheme(kDefaultDarkTheme, 1, 1));
assert(!refreshActiveTheme(kDefaultDarkTheme, 1, 1));            // unchanged → no work
UiTheme red = kDefaultDarkTheme; red.accent = {255,0,0};
assert(!refreshActiveTheme(red, 2, 3));                            // torn: version moved during copy
assert(activeTheme().colors[TOK_ACCENT] == ui::accent);
assert(refreshActiveTheme(red, 3, 3) && activeTheme().colors[TOK_ACCENT] == 0xf800);
puts("resolved theme tests passed");
```

- [ ] **Step 2: Run and see them fail**

Run: `python3 tests/run_native_tests.py; python3 tests/run_ride_tests.py`
Expected: compile errors naming `isStyleSheetValid` / `resolved_theme.h`.

- [ ] **Step 3: Implement** the declarations above in the listed files. Add `resolved_theme.cpp` to every ride-runner build that compiles `widget_registry.cpp` (camera, sensor, page). Leave it out of `[env:native]`, which has no `Gfx` fake.

- [ ] **Step 4: Run all gates**

Run: `python3 tests/run_native_tests.py && python3 tests/run_ride_tests.py && python3 tests/run_navigation_tests.py && pio run -e esp32-s3-devkitc-1`
Expected: `theme tests passed`, `resolved theme tests passed`, `golden: … match`, build `SUCCESS`.

- [ ] **Step 5: Commit** — `feat(ui): style sheet and resolved theme with versioned refresh`

---

### Task 3: Components

**Files:**
- Create: `src/ui/components/label.h/.cpp`, `button.h/.cpp`, `value_tile.h/.cpp`, `hero_value.h/.cpp`, `page_header.h/.cpp`, `tests/test_components.cpp`
- Modify: `src/ui/engine/widget_model.h/.cpp` (status model), `tests/run_ride_tests.py`

**Interfaces:**
- Consumes: Task 2's `Gfx`, `ResolvedTheme`, `StyleId`, and `ui::HitRect` (from `control_layout.h`).
- Produces (namespace `ui`):
  ```cpp
  void drawLabel(Gfx& g, int x, int y, const char* text, uitheme::StyleId style,
                 const ResolvedTheme& t, wmodel::Tone tone = wmodel::TONE_STYLE, int padding = 0);
  struct ButtonProps { const char* text; Icon icon; bool has_icon; bool pressed; };
  void drawButton(Gfx& g, const HitRect& r, const ButtonProps& p, uitheme::StyleId style, const ResolvedTheme& t);
  void drawValueTile(Gfx& g, const HitRect& b, const wmodel::TileModel& m, bool force, const ResolvedTheme& t);
  void drawHeroValue(Gfx& g, const HitRect& b, const wmodel::HeroModel& m, bool force, const ResolvedTheme& t);
  void drawPageHeader(Gfx& g, const char* title, const ResolvedTheme& t);      // clears 0..44, map+ride icons, clipped title
  void drawStatusBar(Gfx& g, const wmodel::StatusModel& m, const ResolvedTheme& t);
  ```
  ```cpp
  // widget_model.h
  enum RideStateHint : uint8_t { RIDE_HINT_IDLE, RIDE_HINT_ACTIVE, RIDE_HINT_PAUSED };
  struct StatusModel { char gps[12]; Tone gps_tone; char ride[8]; Tone ride_tone; char battery[8]; char page[8]; };
  void modelStatus(bool fix, uint8_t fixQuality, uint8_t sats, RideStateHint ride,
                   uint8_t batteryPct, uint8_t pageIdx, uint8_t totalPages, StatusModel& out);
  ```
- Behavior each component must reproduce exactly, call for call, from the code it will replace in Tasks 4 and 6:
  - `drawButton`: `renderPage` action buttons, `rideui` `button()`, power and map buttons. Fill `fillRoundRect(r.x+inset, r.y, r.w-2*inset, r.h, radius, pressed ? colors[TOK_PRESSED] : bg)`. Draw the icon at `+icon_dx,+icon_dy` in `icon` color, or centered when `icon_dx==ICON_CENTER` or the text is empty. Draw the text at `+text_dx,+text_dy` with `setFont`, `setTextSize(text_size)`, `setTextColor(fg,bg)`, `drawString`. When `text_size != 1`, reset `setTextSize(1)` afterwards.
  - `drawValueTile` = `renderTile`. `drawHeroValue` = `renderWidgetSpeed`, including `setCursor`+`print` for the unit at `(b.x+b.w-44, b.y+b.h-18)` and paddings `b.w-8` / `b.w-70`.
  - `drawPageHeader` / `drawStatusBar` = `layout_manager.cpp` `header()`, split at the status line, with `statusY=302`. `modelStatus` text is `GPS %u` (sats clamped to 99) / `GPS weak` / `GPS --`; `REC`/`PAUSE`/`IDLE`; `%u%%` (clamped to 100); `%u/%u`. Tones: GPS fix → SUCCESS, otherwise WARNING. Ride ACTIVE → SUCCESS, PAUSED → WARNING, IDLE → MUTED. The battery and page counter use `STYLE_TEXT_MUTED` and `STYLE_CAPTION` (page counter at `216, statusY+5`).

- [ ] **Step 1: Write failing tests** (`tests/test_components.cpp`, new binary in `run_ride_tests.py`. Assertions are on `canvas.commands`; `A`=`ui::accent`.)
```cpp
const auto& t = ui::activeTheme();
canvas.fillScreen(0); ui::drawButton(canvas, {12,198,216,48}, {"Pause", ui::Icon::Pause, true, false}, STYLE_BUTTON_PRIMARY, t);
assert(canvas.commands.front() == "R|12|198|216|48|8|" + std::to_string(ui::accent));
assert(canvas.commands.back() == "T|2|60|211|0|Pause");
canvas.fillScreen(0); ui::drawButton(canvas, {12,198,216,48}, {"Pause", ui::Icon::Pause, true, true}, STYLE_BUTTON_PRIMARY, t);
assert(canvas.commands.front() == "R|12|198|216|48|8|33808");           // 0x8410 pressed
canvas.fillScreen(0); ui::drawButton(canvas, {0,266,48,44}, {"", ui::Icon::Minus, true, false}, STYLE_BUTTON_MAP, t);
assert(canvas.commands.front() == "R|2|266|44|44|8|" + std::to_string(ui::panel));
assert(canvas.commands.back().rfind("L|", 0) == 0);                     // no text drawn
canvas.fillScreen(0); ui::drawPageHeader(canvas, "A very long custom page title here", t);
assert(std::find(canvas.commands.begin(), canvas.commands.end(), "C|48|0|144|44") != canvas.commands.end());
wmodel::StatusModel s; wmodel::modelStatus(true, 2, 140, wmodel::RIDE_HINT_ACTIVE, 250, 0, 7, s);
assert(!strcmp(s.gps, "GPS 99") && !strcmp(s.battery, "100%") && !strcmp(s.page, "1/7") && s.ride_tone == wmodel::TONE_SUCCESS);
wmodel::modelStatus(false, 1, 0, wmodel::RIDE_HINT_IDLE, 5, 1, 2, s);
assert(!strcmp(s.gps, "GPS weak") && s.gps_tone == wmodel::TONE_WARNING && !strcmp(s.ride, "IDLE") && s.ride_tone == wmodel::TONE_MUTED);
puts("component tests passed");
```

- [ ] **Step 2: Run** `python3 tests/run_ride_tests.py` and expect compile failure on `ui/components/button.h`.
- [ ] **Step 3: Implement** the five components and `modelStatus`.
- [ ] **Step 4: Run** `python3 tests/run_native_tests.py && python3 tests/run_ride_tests.py`. Expect `component tests passed`, and the goldens still match because nothing uses the components yet.
- [ ] **Step 5: Commit** — `feat(ui): button, label, value tile, hero value and page header components`

---

### Task 4: Dashboard on components

**Files:**
- Modify: `src/ui/engine/widget_registry.cpp:1-258` (tiles, hero, battery, theme statics), `src/ui/engine/widget_registry.h` (drop `applyActiveTheme`), `src/ui/engine/layout_manager.cpp` (header/status/action buttons), `src/ui/ui_task.cpp:151` (theme refresh), `src/ui/engine/widget_model.h/.cpp`, `tests/test_widget_model.cpp`, `web/models.js`

**Interfaces:**
- Consumes: Task 3 components, Task 2 `refreshActiveTheme`/`activeTheme`.
- Produces (`widget_model.h`): `modelAvgSpeed(float kmh, bool imperial, TileModel&)`, `modelMaxSpeed(float kmh, bool imperial, TileModel&)`, `modelAltitude(float m, bool valid, bool imperial, TileModel&)`, `modelGrade(float pct, TileModel&)`, `modelTotalAscent(float m, bool imperial, TileModel&)`, `modelBattery(uint8_t pct, TileModel&)`.
  Text and tones are copied from today's renderers (`widget_registry.cpp:128-258`): `avg`/`max` `%.1f` (×0.621371 imperial), TEXT. `alt (m)`/`alt (ft)` `%.0f` (×3.28084), `--` when invalid, TEXT. `grade` `%+.1f%%`, WARNING if >3, ACCENT if <-2, otherwise SUCCESS. `asc (m)`/`asc (ft)` `%.0f`, SUCCESS. `battery` `%u%%`, WARNING if <20, otherwise SUCCESS.
  **Fix the drift:** `modelHeartRate` must become label `heart`, `0` when <0, DANGER. `modelPower` must become label `power`, `0` when <0, **SUCCESS**. That is what the panel draws today, and the golden is the source of truth. Mirror both corrections and the new formatters in `web/models.js`, and add `muted: "label"` to `TONE_TO_THEME`.
- `ui_task.cpp` replaces `applyActiveTheme()` with:
  ```cpp
  const uint32_t v = g_theme_version.load(); const uitheme::UiTheme copy = g_theme;
  ui::refreshActiveTheme(copy, v, g_theme_version.load());
  ```
- Every widget/layout draw uses `canvas` as `Gfx&` and `ui::activeTheme()`. `widget_registry.cpp` loses `COLOR_*` statics, `renderTile`, and the `g_settings.units` reads in the tile renderers. The renderers pass `g_settings.units == 1` into the models at the call site. The full-page widgets (sensor, settings, camera) keep drawing as they do now, but take their colors from `activeTheme().colors[...]` instead of `COLOR_*` and `ui::panel`.

- [ ] **Step 1: Write failing tests** (`tests/test_widget_model.cpp`)
```cpp
TileModel m;
modelHeartRate(-1, m); assert(!strcmp(m.label,"heart") && !strcmp(m.value,"0") && m.tone==TONE_DANGER);
modelPower(212, m);    assert(!strcmp(m.label,"power") && !strcmp(m.value,"212") && m.tone==TONE_SUCCESS);
modelAltitude(100, true, true, m); assert(!strcmp(m.label,"alt (ft)") && !strcmp(m.value,"328"));
modelAltitude(100, false, false, m); assert(!strcmp(m.value,"--"));
modelTotalAscent(100, true, m); assert(!strcmp(m.label,"asc (ft)") && !strcmp(m.value,"328"));
modelAvgSpeed(21.4f, true, m); assert(!strcmp(m.label,"avg") && !strcmp(m.value,"13.3"));
modelMaxSpeed(48.2f, false, m); assert(!strcmp(m.label,"max") && !strcmp(m.value,"48.2"));
modelGrade(4.0f, m); assert(!strcmp(m.value,"+4.0%") && m.tone==TONE_WARNING);
modelGrade(-3.0f, m); assert(m.tone==TONE_ACCENT); modelGrade(0.0f, m); assert(m.tone==TONE_SUCCESS);
modelBattery(19, m); assert(!strcmp(m.value,"19%") && m.tone==TONE_WARNING);
```
- [ ] **Step 2: Run** `python3 tests/run_native_tests.py` and expect it to fail on `modelAltitude` (undeclared), or on the heart-rate assertion.
- [ ] **Step 3: Implement** the models, then switch the renderers, `layout_manager.cpp`, and `ui_task.cpp` over.
- [ ] **Step 4: Run all gates** (the Task 2 Step 4 command). Expect `golden: … match`, which proves `pages.txt`, `sensors.txt` and `camera.txt` are unchanged. Then `grep -n "COLOR_\|ui::panel\|ui::muted\|ui::accent" src/ui/engine/widget_registry.cpp src/ui/engine/layout_manager.cpp` should print nothing.
- [ ] **Step 5: Commit** — `refactor(ui): dashboard tiles, hero and header drawn by themed components`

---

### Task 5: Press tracker, gesture rules, action registry

**Files:**
- Create: `src/ui/press_tracker.h`, `src/ui/gesture.h`, `src/ui/actions.h`, `src/ui/actions.cpp`, `tests/test_press_tracker.cpp`, `tests/test_actions.cpp`
- Modify: `src/ui/ride_view.h` (`Touch` delegates to `PressTracker`), `tests/run_native_tests.py`, `tests/run_ride_tests.py`

**Interfaces:**
- Produces (`press_tracker.h`, header-only, C++11):
  ```cpp
  namespace ui { class PressTracker { public:
    static const int kSlop = 6;
    void cancel(); int pressed() const;                 // -1 when nothing is held
    template<class Hit> int update(bool touched, int x, int y, Hit hit);  // returns the released target id or -1
  }; }
  ```
  Same rules as `rideui::Touch` today: first contact records `x0,y0` and `held=hit(x,y)`. Moving more than 6 px, or `hit(x,y)!=held`, cancels. On release it returns `held` only if not cancelled and `hit(x0,y0)==held` (re-evaluated, so a state change under the finger fires nothing), and `held>=0`. `rideui::Touch` keeps its public API and maps `Action::None` ↔ `-1`.
- Produces (`gesture.h`):
  ```cpp
  namespace ui { enum class Gesture { None, SwipeLeft, SwipeRight, Tap };
    const int kSwipeMinDx = 35, kSwipeMaxDy = 70, kTapMaxMove = 20; const uint32_t kGestureDebounceMs = 150;
    inline Gesture classifyRelease(int dx, int dy); }  // dx<-35&&|dy|<70 → Left; dx>35&&|dy|<70 → Right; |dx|<20&&|dy|<20 → Tap
  ```
- Produces (`actions.h`):
  ```cpp
  enum ActionId : uint8_t { ACT_NONE = 0, ACT_OPEN_MAP, ACT_OPEN_RIDE_MENU, ACT_RIDE_START, ACT_RIDE_PAUSE,
    ACT_RIDE_RESUME, ACT_RIDE_SAVE, ACT_POWER_OFF, ACT_POWER_RESTART, ACT_COUNT };
  struct ActionResult { bool ok; const char* message; };
  ActionResult dispatchAction(ActionId id);
  const char* actionName(ActionId id);   // "map.open","ride.menu","ride.start","ride.pause","ride.resume","ride.save","power.off","power.restart"; "" for NONE/out of range
  ```
  Mapping: OPEN_MAP→`openNavigation()`; OPEN_RIDE_MENU→`openRideMenu()`; START/RESUME→`setManualRideState(RIDE_STATE_ACTIVE)`; PAUSE→`setManualRideState(RIDE_STATE_PAUSED)`; SAVE→`ok=requestFinishRide()`; POWER_OFF/RESTART→`message=powerOff(id==ACT_POWER_RESTART)`, `ok=true`. Any other id → `{false,nullptr}`.

- [ ] **Step 1: Write failing tests**
`tests/test_press_tracker.cpp` (native runner):
```cpp
auto two = [](int x,int){ return x<100 ? 0 : 1; };
ui::PressTracker p;
p.update(true,10,10,two); assert(p.pressed()==0); assert(p.update(false,0,0,two)==0);
p.update(true,10,10,two); p.update(true,17,10,two); assert(p.update(false,0,0,two)==-1);     // slop
p.update(true,95,10,two); p.update(true,101,10,two); assert(p.update(false,0,0,two)==-1);    // left target
p.update(true,10,10,two); assert(p.update(false,0,0,[](int,int){return 1;})==-1);            // state changed under finger
p.update(true,10,10,two); p.cancel(); assert(p.update(false,0,0,two)==-1);
assert(ui::classifyRelease(-36,69)==ui::Gesture::SwipeLeft && ui::classifyRelease(-35,0)==ui::Gesture::None);
assert(ui::classifyRelease(36,-69)==ui::Gesture::SwipeRight && ui::classifyRelease(36,70)==ui::Gesture::None);
assert(ui::classifyRelease(19,-19)==ui::Gesture::Tap && ui::classifyRelease(20,0)==ui::Gesture::None);
```
The lambdas are non-generic, so they're valid C++11.
`tests/test_actions.cpp` (ride runner: real `telemetry_state.cpp`, plus fake `navigation/navigation.h`, `ui/ride_menu.h`, `hardware/power.h` that count calls and return `"Saving failed"` from `powerOff`):
```cpp
initTelemetryState();
assert(dispatchAction(ACT_RIDE_START).ok && getTelemetrySnapshot().ride_state==RIDE_STATE_ACTIVE);
assert(dispatchAction(ACT_RIDE_PAUSE).ok && getTelemetrySnapshot().ride_state==RIDE_STATE_PAUSED);
assert(dispatchAction(ACT_OPEN_MAP).ok && navOpens==1);
auto r = dispatchAction(ACT_POWER_RESTART); assert(lastRestartFlag==true && !strcmp(r.message,"Saving failed"));
assert(!dispatchAction(ACT_NONE).ok && !dispatchAction(ActionId(ACT_COUNT)).ok);
assert(!strcmp(actionName(ACT_RIDE_SAVE),"ride.save") && !strcmp(actionName(ActionId(200)),""));
```
- [ ] **Step 2: Run** both runners and expect compile failures on the new headers.
- [ ] **Step 3: Implement**. `test_ride_session.cpp`'s existing `Touch` assertions must keep passing unchanged.
- [ ] **Step 4: Run all gates** and expect the goldens to match.
- [ ] **Step 5: Commit** — `feat(ui): shared press tracker, gesture rules and action registry`

---

### Task 6: Ride menu on components and actions

**Files:**
- Modify: `src/ui/ride_menu.cpp`, `tests/run_ride_tests.py` (ride binaries also compile `resolved_theme.cpp`, `theme.cpp`, the components, `actions.cpp`; the `actions.cpp` dependencies are faked as in Task 5), `tests/test_ride_preview.cpp` (adds one NaN-distance screen)

**Interfaces:**
- Consumes: `drawLabel`, `drawButton`, `ui::activeTheme()`, `dispatchAction`.
- `ride_menu.cpp` must not include `core/telemetry_state.h` setters, `setManualRideState`, or `requestFinishRide`. Its `dispatch()` keeps the UI-only actions (Back closes, Cancel/Finish toggle `confirm`) and maps Start/Resume/Pause/Save to `ACT_*`, with `confirm=false` when SAVE returns `ok`. Mapping of `render()` to styles: title → `STYLE_TEXT`; `small()` → `STYLE_TEXT_MUTED` with tone (status color via `screenColor` becomes a `wmodel::Tone`: SUCCESS/DANGER/WARNING/ACCENT); distance → `STYLE_VALUE_XL`; time/avg → `STYLE_TEXT`; the saving panel → `fillRoundRect(...,colors[TOK_PANEL])` + `STYLE_PANEL_TITLE`/`STYLE_PANEL_TEXT`; buttons → `STYLE_BUTTON_PRIMARY`/`SECONDARY` with `pressed = touch.pressed()==b.action`; footer → `STYLE_CAPTION`; back icon in `colors[TOK_TEXT]`.

- [ ] **Step 1: Add the NaN case.** In `test_ride_preview.cpp`, add a 10th screen `nan_values` with `trip_distance_km=NAN, avg_speed_kmh=NAN`. Record it with `UPDATE_GOLDEN=1` **before** touching `ride_menu.cpp`. Confirm with `git diff tests/golden/ride.txt` that only the `nan_values` block was added and it shows `0.0`.
- [ ] **Step 2: Refactor** `ride_menu.cpp` as specified.
- [ ] **Step 3: Run all gates.** Expect the ride golden to match byte-for-byte and `test_ride_session` to pass.
- [ ] **Step 4: Commit** — `refactor(ui): ride menu drawn by components, ride commands through actions`

---

### Task 7: Power menu view, screen, and canvas move

**Files:**
- Create: `src/ui/power_view.h`, `src/ui/power_screen.h`, `src/ui/power_screen.cpp`, `tests/test_power_screen.cpp`, `tests/golden/power.txt`
- Modify: `src/ui/power_menu.cpp`, `tests/run_ride_tests.py`

**Interfaces:**
- Produces (`power_view.h`, header-only):
  ```cpp
  namespace powerui {
  struct View { char battery[48]; const char* usb; const char* charge; const char* warning; const char* footer; };
  View view(uint8_t pct, float volts, bool hasUsbSense, bool usbConnected, const char* message);
  const int kButtons = 3;                 // 0 Power off, 1 Restart, 2 Back
  const char* buttonLabel(int i); ui::Icon buttonIcon(int i);   // Power/Retry/Back
  int hit(int x, int y);                  // index whose ui::powerRect contains (x,y), else -1
  }
  ```
  Text is exactly as in today's `drawMenu()`: `Battery estimate: %u%%  %.2f V`. `usb` is `USB power connected` / `USB power disconnected` / `USB charging is automatic.`. `charge="Charge status unavailable."`, `warning="Power off / restart ends this ride."`, `footer=message?message:"Wake after power off: press BOOT"`.
- Produces (`power_screen.h`): `void drawPowerScreen(Gfx& g, const powerui::View& v, int pressed, const ui::ResolvedTheme& t);`. It clears to `TOK_BG`. `Power` is drawn with `STYLE_SCREEN_TITLE` at (12,14), the four text lines with `STYLE_SCREEN_TEXT` at y = 50, 72, 90, 116, the three buttons with `STYLE_BUTTON_POWER` at `ui::powerRect(i)` (`pressed==i` → pressed), and the footer at (12,282).
- `power_menu.cpp` keeps the `PowerButton`, screen-off, and wake logic. It draws via `drawPowerScreen(canvas, …)` + `presentFrame()` instead of `tft`. Touch goes through a `ui::PressTracker` with `powerui::hit`. Buttons 0/1 → set the message `"Saving ride..."`, draw, then `message = dispatchAction(i==0?ACT_POWER_OFF:ACT_POWER_RESTART).message`. Button 2 closes the menu. The redraw cadence (dirty, or every 2000 ms) is unchanged, and the screen also redraws when the pressed index changes.

- [ ] **Step 1: Write failing test** (`tests/test_power_screen.cpp`, ride runner, with a `preview` argument for the golden):
```cpp
auto v = powerui::view(82, 4.01f, false, false, nullptr);
assert(!strcmp(v.battery, "Battery estimate: 82%  4.01 V") && !strcmp(v.usb, "USB charging is automatic.") && !strcmp(v.footer, "Wake after power off: press BOOT"));
assert(!strcmp(powerui::view(5, 3.3f, true, true, "Saving ride...").footer, "Saving ride..."));
assert(powerui::hit(20, 140) == 0 && powerui::hit(20, 190) == 1 && powerui::hit(20, 240) == 2 && powerui::hit(20, 300) == -1);
drawPowerScreen(canvas, v, 1, ui::activeTheme());
assert(std::count(canvas.commands.begin(), canvas.commands.end(), "R|12|182|216|44|8|33808") == 1);   // Restart pressed
assert(std::find(canvas.commands.begin(), canvas.commands.end(), "T|0|60|196|65535|Restart") != canvas.commands.end());
```
- [ ] **Step 2: Run** `python3 tests/run_ride_tests.py` and expect a compile failure.
- [ ] **Step 3: Implement**, then record `power.txt` with `UPDATE_GOLDEN=1` and add `power` to the golden list.
- [ ] **Step 4: Render a preview for the owner.** Run `python3 tests/run_ride_tests.py /tmp/oc-preview` (extend the preview loop to include `power`). Attach the PNGs to the PR as the deliberate-change evidence the spec requires.
- [ ] **Step 5: Run all gates** and expect every golden to match.
- [ ] **Step 6: Commit** — `refactor(ui): power menu as view + themed screen on the canvas, power-off via actions`

---

### Task 8: Map controls use the shared button

**Files:**
- Modify: `src/navigation/navigation.cpp:701-712`, `tests/test_navigation_integration.cpp`

**Interfaces:**
- Consumes: `drawButton`, `STYLE_BUTTON_MAP` / `STYLE_BUTTON_MAP_ACTIVE`, `ui::activeTheme()`.
- The control loop becomes: for each control, `drawButton(canvas, r, {list?listLabels[i]:"", icon, true, false}, (!list&&i==2&&follow)?STYLE_BUTTON_MAP_ACTIVE:STYLE_BUTTON_MAP, t)`. Nothing else in `navigation.cpp` changes in this plan.

- [ ] **Step 1: Write a failing assertion.** In list mode, after a render, the test asserts that labels `Prev`, `Map` and `Next` are present (use the existing `label()` helper in the test). Run `python3 tests/run_navigation_tests.py` first to confirm it passes on the current code. It's a guard, not a red test. Then make the runner compile `theme.cpp`, `resolved_theme.cpp` and `components/button.cpp` and substitute `ui/gfx.h`.
- [ ] **Step 2: Replace the loop.**
- [ ] **Step 3: Run all gates**, including `python3 tests/run_navigation_tests.py`. Expect it to pass, along with `memory-failure`.
- [ ] **Step 4: Commit** — `refactor(nav): map controls drawn by the shared themed button`

---

### Task 9: Input routing in `ui_task.cpp`

**Files:**
- Modify: `src/ui/ui_task.cpp`, `src/ui/engine/layout_manager.cpp` (page action buttons and header taps via `dispatchAction`)

**Interfaces:**
- Consumes: `ui::classifyRelease`, `ui::kGestureDebounceMs`, `dispatchAction(ACT_OPEN_MAP / ACT_OPEN_RIDE_MENU)`.
- Replace the hand-written power → ride-menu branches with one ordered table. When an entry consumes input, `cancelSensorWidgetTouch()` and `cancelNavigationTouch()` run, then the `cancel` of every *later* entry, then `delayMs` and `continue`. This reproduces today's behavior exactly: power cancels the ride-menu touch; the ride menu doesn't cancel power.
  ```cpp
  struct Overlay { bool (*update)(bool touched, int16_t x, int16_t y, uint32_t now); void (*cancel)(); uint32_t delayMs; };
  static const Overlay kOverlays[] = { {powerOverlay, nullptr, 50}, {rideOverlay, cancelRideMenuTouch, 10} };
  ```
  The dashboard swipe/tap block uses `classifyRelease(deltaX, deltaY)` and `kGestureDebounceMs`. The serial log lines stay. `handlePageTouch` calls `dispatchAction(ACT_OPEN_MAP)` / `dispatchAction(ACT_OPEN_RIDE_MENU)` instead of `openNavigation()` / `openRideMenu()`.

- [ ] **Step 1: Run** `python3 tests/run_ride_tests.py`. The `page_ui` test already asserts header taps open the map/ride menu (`mapOpens`/`rideOpens`). Make its runner compile `actions.cpp` with the Task 5 fakes, and keep it passing.
- [ ] **Step 2: Refactor** `ui_task.cpp` and `layout_manager.cpp`.
- [ ] **Step 3: Run all gates.**
- [ ] **Step 4: Check on the device** (flash with `pio run -e esp32-s3-devkitc-1 -t upload`):
  - Swipe left and right changes the dashboard page.
  - A tap on a settings row still toggles it.
  - Long-pressing the power button opens the power menu, and Back closes it.
  - The ride menu Start/Pause/Finish/Save flow works.
  - Map +/−, center, list and Prev/Map/Next all work.
  - A theme pushed from `web/` (Apply theme) recolors buttons and tiles live.
  - The serial log of loop time (add a temporary `Serial.printf` of `millis()` deltas every 100 frames, and remove it before committing) shows the same period as before the plan (~50 ms).
- [ ] **Step 5: Commit** — `refactor(ui): ordered overlay routing and shared gesture rules in the UI task`

---

## Self-Review Notes

- Spec coverage (phases 0–3):
  - Phase 0 → Task 0.
  - Theme v2 tokens, fonts and styles → Task 2 (JSON/persistence of the sheet is deferred to Plan 3, because both need the nested-document parser).
  - Components → Task 3.
  - Dashboard → Task 4.
  - PressTracker, actions and screen routing → Tasks 5 and 9.
  - Ride/power menus → Tasks 6–7.
  - Map buttons → Task 8.
  - Golden parity → Task 1.
- Deviation from the spec: `font_scale_pct` is not replaced yet. `UiTheme` stays byte-identical, so stored NVS themes aren't reset; Plan 3 retires it with the v2 wire format.
- The style list is longer than the spec's illustrative list. Reproducing today's exact offsets needs per-screen button styles. The spec leaves the exact names to the builder.
