# setTextPadding() print()->drawString() fix report

## Bug (confirmed fact, not re-derived)

`tft.setTextPadding(N)` only widens the erase rect for `LGFXBase::draw_string()`
(used by `drawString()`/`drawNumber()`). It is never consulted by the
`print()`/`printf()` path (`Print::write()` -> `LGFXBase::write(uint8_t)`), so
every `setTextPadding(N); setCursor(x,y); print()/printf(); setTextPadding(0);`
site in this codebase was a silent no-op for the erase-width benefit.

## Fix applied

For each vulnerable site: dropped the `setCursor()` call, kept the
`setTextPadding(N)`/`setTextPadding(0)` bracket exactly as-is, and replaced
`print(str)` with `drawString(str, x, y)` (top-left anchored, same position
as the old `setCursor`+`print`). For sites using `printf(fmt, ...)` directly,
introduced a local `char buf[N]; snprintf(buf, sizeof(buf), fmt, ...);` then
`drawString(buf, x, y)`, since `drawString()` has no printf-style overload.

No padding widths, colors, fonts, or positions were changed — purely draw-call
swaps, per the task's explicit instruction.

## Call sites found and disposition

### `src/ui/engine/layout_manager.cpp`

1. **Page title** (`renderPage`, status header segment 1) — `setTextPadding(STATUS_TITLE_W)` bracketing `tft.print(page.title)`, inside `if (forceFullRedraw)`.
   - **Disposition: FIXED** (`drawString(page.title, STATUS_TITLE_X, STATUS_ROW_Y)`).
   - Note: this one is only drawn once per force-redraw and is immediately preceded by an unconditional `tft.fillScreen(COLOR_BG)`, so it was already functionally safe regardless of `padding_x`. I fixed it anyway for consistency with the verification rule ("every remaining `setTextPadding` must be adjacent to `drawString`, not `print`/`printf`") and because it is a zero-risk one-line swap — the task's own list of pre-verified-safe sites did not name this one.

2. **GPS segment** (`renderPage`, status header segment 2) — `setTextPadding(STATUS_GPS_W)` bracketing an `if/else` of `tft.printf("gps %u", ...)` / `tft.print("gps --")`. Runs every frame, unconditional (not gated by `forceFullRedraw`). `state.satellites`/`state.gps_has_fix` change frame to frame.
   - **Disposition: FIXED.** `printf` branch -> `snprintf` into `char gpsBuf[10]` then `drawString`; `print("gps --")` branch -> `drawString("gps --", ...)`.

3. **Ride state segment** (`renderPage`, status header segment 3) — `setTextPadding(STATUS_STATE_W)` bracketing `tft.printf("%s %u%%", ...)`. Runs every frame, unconditional. `ride_state`/`battery_pct` change frame to frame.
   - **Disposition: FIXED.** `snprintf` into `char stateBuf[16]` then `drawString`.

4. **Action button** ("pause ride"/"resume ride"/"start ride") — `tft.print(...)` at the end of `renderPage`, immediately preceded every frame (unconditional, not gated by `force`) by `tft.fillRoundRect(btn.x, btn.y, btn.w, btn.h, 8, btnColor)`. No `setTextPadding` used here at all.
   - **Disposition: LEFT ALONE — verified already safe.** Confirmed the `fillRoundRect` call is unconditional (outside any `if(forceFullRedraw)` block) and wipes the button's full rect every frame before the text draw, exactly as the task described. Matches the task's named "known safe" example.

### `src/ui/engine/widget_registry.cpp`

5. **`renderWidgetSpeed` — speed-source label** ("speed . ble"/"speed . gps") — `setTextPadding(b.w-8)` bracketing `tft.printf("speed . %s", ...)`. Not inside the function's `if(force)` block (only the background `fillRect` is gated); runs every frame; `speed_source` can change.
   - **Disposition: FIXED.** `snprintf` into `char srcBuf[16]` then `drawString`.

6. **`renderWidgetSpeed` — speed value** (large font, formatted `buf`) — `setTextPadding(b.w-70)` bracketing `tft.print(buf)`. `buf` already `snprintf`-formatted upstream; runs every frame; value changes continuously.
   - **Disposition: FIXED** — pure one-line swap (`print(buf)` -> `drawString(buf, b.x+4, b.y+24)`).

7. **`renderWidgetSpeed` — units suffix** ("mph"/"km/h") — `tft.print(...)` at the bottom-right of the speed tile. **No `setTextPadding` call wraps this site at all.**
   - **Disposition: LEFT ALONE — out of scope.** The confirmed bug is specifically about `setTextPadding()` being a no-op for `print()`; this site never calls `setTextPadding()` in the first place, so this fix (a pure draw-call swap that must not add/change padding) doesn't touch it. Flagging as a **separate pre-existing latent ghosting risk** ("mph" is narrower than "km/h" at a fixed anchor x, so toggling units could leave a stale trailing character) — worth a follow-up ticket, but outside this fix's scope per the task's own framing.

8. **`renderTile`** (shared helper for distance/ride time/cadence/heart rate/power/altitude/grade/total ascent/avg speed/max speed/battery tiles) — label and value, each individually `setTextPadding`-bracketed around `tft.print(...)`. Called every frame regardless of `force` (only the tile's background `fillRect`+hairline are force-gated).
   - **Disposition: FIXED (both label and value).** Straight swap to `drawString(label, b.x+4, b.y+4)` and `drawString(valueStr, b.x+4, b.y+24)`.

9. **`renderWidgetElevationChart` — "elevation profile" label** — `tft.print("elevation profile")` inside `if (force) { ... }`. No `setTextPadding` used.
   - **Disposition: LEFT ALONE — static, force-gated, no padding.** Only drawn once per force-redraw, right after the tile's own `fillRect`, and the string is a compile-time constant that never changes. Not vulnerable, and not in scope (no `setTextPadding`).

10. **`renderWidgetBleManager` — scan button label** ("scanning..."/"scan & add sensors") — `tft.print(...)`, immediately preceded every frame (unconditional, outside the `if(force)` block which only gates the widget's base `fillRect`) by `tft.fillRoundRect(b.x+6, b.y+6, b.w-12, 28, 6, scanBtnColor)`. No `setTextPadding` here.
    - **Disposition: LEFT ALONE — verified already safe.** Confirmed the `fillRoundRect` runs unconditionally every frame right before the text. Matches the task's named "known safe" example.

11. **`renderWidgetBleManager` — sensor row label** (`rows[i].label`, e.g. "speed/cad") — `setTextPadding(76)` bracketing `tft.print(rows[i].label)`. Runs every frame in an unconditional loop (not gated by `force`). The label text itself is a fixed constant per row index (never actually changes across frames), but the call site matches the broken pattern exactly and the swap is risk-free.
    - **Disposition: FIXED** for consistency/verification-cleanliness — `drawString(rows[i].label, b.x+10, y)`.

12. **`renderWidgetBleManager` — MAC/pairing status** (`%.10s..` of `rows[i].mac`, or "not paired") — `setTextPadding(b.w-100)` bracketing an `if/else` of `tft.printf("%.10s..", rows[i].mac)` / `tft.print("not paired")`, both drawn at cursor `(b.x+90, y)`. Runs every frame, unconditional. `rows[i].mac` changes when a sensor is paired/forgotten.
    - **Disposition: FIXED (both branches).** `printf` branch -> `snprintf` into `char macBuf[14]` then `drawString(macBuf, b.x+90, y)`; `print("not paired")` -> `drawString("not paired", b.x+90, y)`.

13. **`renderWidgetBleManager` — "forget" button label** — `tft.print("forget")`, drawn under the *same* still-active `setTextPadding(b.w-100)` bracket as #12 above (no reset in between), but at a different cursor position (`b.x+b.w-54, y`), immediately preceded every frame by `tft.fillRoundRect(b.x+b.w-60, y-4, 52, 18, 4, COLOR_RED)`.
    - **Disposition: LEFT ALONE — deliberately, after checking for a regression risk.** Two reasons:
      (a) it's already safe: the `fillRoundRect` unconditionally wipes the whole button every frame right before the text is drawn, and the text itself never changes ("forget" is a constant), so there's no ghosting risk regardless.
      (b) **converting it would introduce a new bug**: the active `setTextPadding(b.w-100)` width was sized for the MAC text's cursor position (`b.x+90`), not this button's position (`b.x+b.w-54`). If `drawString()` had been used here, it would attempt to erase a `b.w-100`-wide rect starting from `b.x+b.w-54`, which overflows well past the widget's right edge for typical widget widths (e.g. for `b.w≈232`, that's an erase reaching `b.x+310`, ~78px past the widget). Left as `print()`, which never reads `padding_x`, so it stays exactly as safe as it is today. A comment was added in the code explaining this.

14. **`renderWidgetBleManager` — gps status footer** ("gps: 3d fix valid"/"gps: searching...") — `setTextPadding(b.w-20)` bracketing `tft.print(...)`. Runs every frame, unconditional. `gps_has_fix` changes.
    - **Disposition: FIXED.** `drawString(state.gps_has_fix ? "gps: 3d fix valid" : "gps: searching...", b.x+10, y+6)`.

15. **`renderWidgetSettingsList` — `row` lambda** (label + value for each settings row: units, brightness, wheel size, sd logging, battery, firmware) — `setTextPadding(110)` / `setTextPadding(b.w-126)` each bracketing a `tft.print(...)`. Called every frame from `renderWidgetSettingsList`, which is **not** gated by `force` (only the widget's base `fillRect` is force-gated; the `row()` calls themselves run unconditionally on every frame). Several values change frame-to-frame (brightness %, battery % + voltage), others toggle on touch (units, sd logging enabled/disabled).
    - **Disposition: FIXED (both label and value in the lambda).**
    - **IMPORTANT DISCREPANCY FROM TASK DESCRIPTION:** the task described this site as already-safe, claiming "each row's colored box is `fillRoundRect`'d every frame before its text." I read the actual current code fresh (per instructions, not relying on memory) and **found no `fillRoundRect` anywhere in `renderWidgetSettingsList` or its `row` lambda** — the only rect fill is the widget's base `if (force) { tft.fillRect(...) }`, which does not run every frame. I grepped the whole file for `fillRoundRect` to confirm: only 2 occurrences exist in `widget_registry.cpp`, both in `renderWidgetBleManager` (the scan button and the forget button), none in the settings list. So this site is genuinely vulnerable and needed fixing, not the safe pattern the task assumed. Treated it as needing the fix per the task's own fallback instruction: "if you're not sure whether a given call site is truly safe, treat it as needing the fix rather than assuming safety."

## Verification

### 1. `setTextPadding` grep — every remaining occurrence adjacent to `drawString()`

```
$ grep -n -A2 "setTextPadding(" src/ui/engine/layout_manager.cpp src/ui/engine/widget_registry.cpp | grep -B2 "print(\|printf("
src/ui/engine/layout_manager.cpp:79:  tft.setTextPadding(STATUS_STATE_W);
src/ui/engine/layout_manager.cpp-80-  char stateBuf[16];
src/ui/engine/layout_manager.cpp-81-  snprintf(stateBuf, sizeof(stateBuf), "%s %u%%", ...
--
src/ui/engine/widget_registry.cpp:25:  tft.setTextPadding(b.w - 8);
src/ui/engine/widget_registry.cpp-26-  char srcBuf[16];
src/ui/engine/widget_registry.cpp-27-  snprintf(srcBuf, sizeof(srcBuf), "speed . %s", ...
```
(These are `snprintf` calls used to build the buffer that's passed to `drawString` on the next line — not `tft.print`/`tft.printf` draw calls. False positives from the grep pattern matching the substring "printf" inside "snprintf".)

```
$ grep -n "tft\.printf(" src/ui/engine/layout_manager.cpp src/ui/engine/widget_registry.cpp
(no output — zero remaining tft.printf calls)
```

Remaining `tft.print(...)` calls (7 total) are all confirmed non-adjacent to any `setTextPadding`, and each was individually verified above as either statically safe or protected by an unconditional rect-fill:
`layout_manager.cpp`: "pause ride"/"resume ride"/"start ride" (action button, rect-fill protected).
`widget_registry.cpp`: "mph"/"km/h" (no padding, out of scope), "elevation profile" (static, force-gated, no padding), "scanning..."/"scan & add sensors" (rect-fill protected, no padding), "forget" (rect-fill protected, deliberately not converted — see #13).

### 2. `pio run`

```
PLATFORM: Espressif 32 (6.12.0) > Espressif ESP32-S3-DevKitC-1-N8 (8 MB QD, No PSRAM)
HARDWARE: ESP32S3 240MHz, 320KB RAM, 8MB Flash
...
RAM:   [=         ]  10.4% (used 34108 bytes from 327680 bytes)
Flash: [=         ]  12.1% (used 793301 bytes from 6553600 bytes)
esptool.py v4.9.0
Creating esp32s3 image...
========================= [SUCCESS] Took 4.52 seconds =========================
Environment         Status    Duration
esp32-s3-devkitc-1  SUCCESS   00:00:04.520
```

### 3. `pio test -e native`

```
Collected 4 tests

Processing test_ui_config in native environment
...
test/test_ui_config/test_ui_config.cpp:47: test_valid_config_passes	[PASSED]
test/test_ui_config/test_ui_config.cpp:48: test_wrong_byte_count_fails	[PASSED]
test/test_ui_config/test_ui_config.cpp:49: test_stale_schema_version_fails	[PASSED]
test/test_ui_config/test_ui_config.cpp:50: test_zero_pages_fails	[PASSED]
test/test_ui_config/test_ui_config.cpp:51: test_too_many_pages_fails	[PASSED]
--------------- native:test_ui_config [PASSED] Took 1.07 seconds ---------------

Processing test_widget_types in native environment
...
test/test_widget_types/test_widget_types.cpp:15: test_size_class_count_is_five	[PASSED]
test/test_widget_types/test_widget_types.cpp:16: test_widget_type_count_is_sixteen	[PASSED]
------------- native:test_widget_types [PASSED] Took 0.81 seconds -------------

Processing test_widget_catalog in native environment
...
test/test_widget_catalog/test_widget_catalog.cpp:69: test_speed_supports_only_hero	[PASSED]
test/test_widget_catalog/test_widget_catalog.cpp:70: test_cadence_supports_only_small	[PASSED]
test/test_widget_catalog/test_widget_catalog.cpp:71: test_avg_speed_supports_small_and_medium	[PASSED]
test/test_widget_catalog/test_widget_catalog.cpp:72: test_elevation_chart_supports_large_and_full	[PASSED]
test/test_widget_catalog/test_widget_catalog.cpp:73: test_ble_manager_supports_only_full	[PASSED]
test/test_widget_catalog/test_widget_catalog.cpp:74: test_widget_none_supports_nothing	[PASSED]
test/test_widget_catalog/test_widget_catalog.cpp:75: test_size_support_is_membership_not_ordering	[PASSED]
test/test_widget_catalog/test_widget_catalog.cpp:76: test_out_of_range_size_or_type_is_rejected	[PASSED]
test/test_widget_catalog/test_widget_catalog.cpp:77: test_getWidgetMeta_returns_matching_type	[PASSED]
------------ native:test_widget_catalog [PASSED] Took 0.71 seconds ------------

Processing test_template_engine in native environment
...
test/test_template_engine/test_template_engine.cpp:36: test_hero_6_grid_slot0_is_hero_sized	[PASSED]
test/test_template_engine/test_template_engine.cpp:37: test_hero_6_grid_bottom_row_is_small	[PASSED]
test/test_template_engine/test_template_engine.cpp:38: test_2_grid_chart_slot2_is_large	[PASSED]
test/test_template_engine/test_template_engine.cpp:39: test_full_container_is_full	[PASSED]
test/test_template_engine/test_template_engine.cpp:40: test_out_of_range_template_falls_back_to_full_container	[PASSED]
------------ native:test_template_engine [PASSED] Took 0.70 seconds ------------

=================================== SUMMARY ===================================
Environment    Test                  Status    Duration
-------------  --------------------  --------  ------------
native         test_ui_config        PASSED    00:00:01.067
native         test_widget_types     PASSED    00:00:00.810
native         test_widget_catalog   PASSED    00:00:00.709
native         test_template_engine  PASSED    00:00:00.698
================= 21 test cases: 21 succeeded in 00:00:03.284 =================
```

Same 4 test files / 21 test cases as before the change — count unchanged, all pass, as expected (neither touched file is natively tested).

### 4. Full diff review

Reviewed the full `git diff` for both files. Confirmed:
- No padding widths, colors, fonts, or draw positions changed anywhere.
- Every `setCursor(x,y); print(str);` pair became `drawString(str, x, y);` at the identical `(x,y)`.
- Every direct `printf(fmt, ...)` call became `snprintf` into a locally-scoped `char buf[N]` (sized generously for the known-max content, matching the existing header comments about max string widths) followed by `drawString(buf, x, y)`.
- The one deliberately-skipped site inside a still-active padding bracket ("forget") got an explanatory comment so a future reader doesn't "fix" it into a regression.

## Concerns

- **Not visually verified on hardware or in a simulator this session.** All positional math (`x`, `y` coordinates carried over from `setCursor`) was verified by direct text diff to be byte-identical to what was previously passed to `setCursor()`, and the build/tests both pass, but I have not rendered any of these screens to confirm pixel-level correctness (e.g. that the erase widths chosen upstream — like `STATUS_GPS_W = 58`, `b.w - 100`, etc. — are in fact wide enough for their real max content, per the existing header comments). That was pre-existing sizing, not something this fix changed, but flagging it since I can't confirm visually.
- The task's description of the settings-list rows as "already safe via fillRoundRect" did not match the code I read; I fixed that site as a genuinely vulnerable one instead of leaving it alone. Flagging this prominently in case it indicates the task's author was working from a stale/different version of this file.
