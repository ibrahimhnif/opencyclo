# OpenCyclo UI Component Rebuild Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild OpenCyclo's on-device UI widgets and page templates so widgets can only be placed into slots they're built for (fixing the overlapping-text bug from cramming a hero-sized widget into a small slot), NVS-persisted layouts can't silently drift out of sync with the firmware (fixing the stale-widget-assignment bug), and the visual style matches the approved "Garmin/Wahoo Minimal" direction.

**Architecture:** Split widget *metadata* (name/label/units/supported size classes — pure data, natively testable) from widget *rendering* (LovyanGFX draw calls — hardware-only). Tag every template slot with a `SizeClass`; a widget renders only into slots whose class it declares support for, else a blank placeholder is drawn. `UiConfig` gains a `schema_version` that must match before NVS data is trusted.

**Tech Stack:** PlatformIO, Arduino framework, ESP32-S3, LovyanGFX 1.2.27 (FreeSans GFX fonts), FreeRTOS, PlatformIO native test environment + Unity for pure-logic tests.

**Spec:** `docs/superpowers/specs/2026-08-22-ui-component-rebuild-design.md`

**Scope note:** This plan covers Build Order steps 1–2 from the spec (Components, Pages). Step 3 (updating the Flutter companion app's layout editor to the new schema) is a separate subsystem/codebase and gets its own follow-up plan once this one is verified on hardware.

## Global Constraints

- Trimmed widget catalog is exactly these 15 (plus `WIDGET_NONE`): Speed, Avg Speed, Max Speed, Distance, Ride Time, Cadence, Heart Rate, Power, Altitude, Grade, Total Ascent, Elevation Chart, Battery, BLE Manager, Settings List. `Clock`, `GPS Diagnostics`, `NMEA Console` are removed entirely (enum values, descriptors, render functions, and their page).
- `SizeClass` values: `SIZE_SMALL` (74×54), `SIZE_MEDIUM` (114×60), `SIZE_LARGE` (232×192, the `TEMPLATE_2_GRID_CHART` chart slot), `SIZE_HERO` (232×94), `SIZE_FULL` (232×274). A slot's class is the *minimum* size a widget must support to be placed there — a widget with more room than it needs is fine; less room than it needs is not.
- `UI_CONFIG_SCHEMA_VERSION` starts at `1`. Bump it in any future change to `WidgetType`, `SizeClass`, or `PageConfig`/`UiConfig` layout.
- Visual palette (all widgets and the status bar use these, no ad-hoc colors):
  ```cpp
  COLOR_BG      = TFT_BLACK                          // 0x0000, panel background
  COLOR_HAIRLINE= tft.color565(28, 28, 28)            // #1c1c1c divider above each tile
  COLOR_TEXT    = TFT_WHITE                           // #FFFFFF primary values
  COLOR_LABEL   = tft.color565(102, 102, 102)         // #666666 muted lowercase labels
  COLOR_GREEN   = tft.color565(46, 213, 115)          // active / good
  COLOR_AMBER   = tft.color565(255, 171, 0)           // paused / warning
  COLOR_RED     = tft.color565(255, 71, 87)           // heart rate / alert
  COLOR_CYAN    = tft.color565(0, 210, 255)           // BLE-sourced data / accent
  ```
- Fonts (LovyanGFX bundled GFX fonts, confirmed present under `lgfx/Fonts/GFXFF/`): `&fonts::FreeSans9pt7b` for labels/units, `&fonts::FreeSans12pt7b` for SMALL/MEDIUM tile values, `&fonts::FreeSans24pt7b` for the hero speed value. Regular weight throughout (no Bold) per the approved minimal style.
- Every widget/template task's final step is **on-device visual verification** (flash + look at the physical panel) — LovyanGFX text drawing can't be meaningfully asserted in a unit test; pixel/offset tuning happens by eye against the real screen, same as the rest of this project's UI work.
- `git commit` after every task, using the message convention already used in this repo's history (`type(scope): summary`).

---

### Task 1: Native test environment + SizeClass/WidgetType types

**Files:**
- Modify: `platformio.ini`
- Modify: `src/ui/engine/widget_types.h`

**Interfaces:**
- Produces: `enum SizeClass : uint8_t { SIZE_SMALL, SIZE_MEDIUM, SIZE_LARGE, SIZE_HERO, SIZE_FULL, SIZE_CLASS_COUNT }`; trimmed `enum WidgetType : uint8_t { WIDGET_NONE=0, WIDGET_SPEED, WIDGET_AVG_SPEED, WIDGET_MAX_SPEED, WIDGET_DISTANCE, WIDGET_RIDE_TIME, WIDGET_CADENCE, WIDGET_HEART_RATE, WIDGET_POWER, WIDGET_ALTITUDE, WIDGET_GRADE, WIDGET_TOTAL_ASCENT, WIDGET_ELEVATION_CHART, WIDGET_BATTERY, WIDGET_BLE_MANAGER, WIDGET_SETTINGS_LIST, WIDGET_TYPE_COUNT }`; unchanged `struct Rect { int16_t x,y,w,h; }`.

- [ ] **Step 1: Add a native test environment to `platformio.ini`**

Append this section (leave the existing `[env:esp32-s3-devkitc-1]` section untouched):

```ini
[env:native]
platform = native
test_framework = unity
build_flags = -std=c++14 -DUNIT_TEST
; PlatformIO otherwise auto-compiles all of src/ (including main.cpp and every
; Arduino/FreeRTOS-dependent .cpp) into the native test binary, which won't
; compile under the native platform. Exclude everything, then explicitly
; re-include only the framework-agnostic .cpp files this plan adds (Tasks
; 2-4) — the test files need these actually compiled and linked, not just
; their headers. Referencing not-yet-created files here is harmless; the
; filter just won't match anything until each task creates its .cpp.
build_src_filter =
    -<*>
    +<ui/engine/widget_catalog.cpp>
    +<ui/engine/ui_config.cpp>
    +<ui/engine/template_engine.cpp>
```

- [ ] **Step 2: Write a placeholder native test to confirm the harness works**

Create `test/test_ui_engine/test_widget_types.cpp`:

```cpp
#include <unity.h>
#include "../../src/ui/engine/widget_types.h"

void test_size_class_count_is_five() {
    TEST_ASSERT_EQUAL(5, SIZE_CLASS_COUNT);
}

void test_widget_type_count_is_sixteen() {
    // WIDGET_NONE + 15 real widgets
    TEST_ASSERT_EQUAL(16, WIDGET_TYPE_COUNT);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_size_class_count_is_five);
    RUN_TEST(test_widget_type_count_is_sixteen);
    return UNITY_END();
}
```

- [ ] **Step 3: Run the test to confirm it fails (widget_types.h doesn't have SizeClass yet)**

Run: `pio test -e native`
Expected: Compile error — `SIZE_CLASS_COUNT` / `SizeClass` not declared.

- [ ] **Step 4: Update `widget_types.h`**

Replace the file contents:

```cpp
#ifndef OPENCYCLO_UI_ENGINE_WIDGET_TYPES_H
#define OPENCYCLO_UI_ENGINE_WIDGET_TYPES_H

#include <stdint.h>

// A slot's SizeClass is the MINIMUM size a widget must support to be placed
// there. A widget with more room than it needs is fine; less is not.
enum SizeClass : uint8_t {
  SIZE_SMALL = 0,  // 74x54   - bottom-row tiles
  SIZE_MEDIUM,     // 114x60  - mid-row tiles
  SIZE_LARGE,      // 232x192 - the chart slot in TEMPLATE_2_GRID_CHART
  SIZE_HERO,       // 232x94  - the single headline tile (Speed)
  SIZE_FULL,       // 232x274 - whole-page management screens
  SIZE_CLASS_COUNT
};

enum WidgetType : uint8_t {
  WIDGET_NONE = 0,
  WIDGET_SPEED,
  WIDGET_AVG_SPEED,
  WIDGET_MAX_SPEED,
  WIDGET_DISTANCE,
  WIDGET_RIDE_TIME,
  WIDGET_CADENCE,
  WIDGET_HEART_RATE,
  WIDGET_POWER,
  WIDGET_ALTITUDE,
  WIDGET_GRADE,
  WIDGET_TOTAL_ASCENT,
  WIDGET_ELEVATION_CHART,
  WIDGET_BATTERY,
  WIDGET_BLE_MANAGER,
  WIDGET_SETTINGS_LIST,
  WIDGET_TYPE_COUNT
};

struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

#endif // OPENCYCLO_UI_ENGINE_WIDGET_TYPES_H
```

- [ ] **Step 5: Run the test to confirm it passes**

Run: `pio test -e native`
Expected: `2 Tests 0 Failures 0 Ignored / OK`

- [ ] **Step 6: Commit**

```bash
git add platformio.ini src/ui/engine/widget_types.h test/test_ui_engine/test_widget_types.cpp
git commit -m "feat(ui): add SizeClass enum, trim WidgetType to 15 widgets, native test env"
```

---

### Task 2: Widget Catalog (framework-agnostic metadata)

**Files:**
- Create: `src/ui/engine/widget_catalog.h`
- Create: `src/ui/engine/widget_catalog.cpp`
- Test: `test/test_ui_engine/test_widget_catalog.cpp`

**Interfaces:**
- Consumes: `WidgetType`, `SizeClass` from `widget_types.h` (Task 1).
- Produces: `struct WidgetMeta { WidgetType type; const char* name; const char* label; const char* unit_metric; const char* unit_imperial; uint8_t supported_sizes_mask; };` `const WidgetMeta& getWidgetMeta(WidgetType type);` `bool widgetSupportsSize(WidgetType type, SizeClass size);`

- [ ] **Step 1: Write the failing test**

Create `test/test_ui_engine/test_widget_catalog.cpp`:

```cpp
#include <unity.h>
#include "../../src/ui/engine/widget_catalog.h"

void test_speed_supports_only_hero() {
    TEST_ASSERT_TRUE(widgetSupportsSize(WIDGET_SPEED, SIZE_HERO));
    TEST_ASSERT_FALSE(widgetSupportsSize(WIDGET_SPEED, SIZE_SMALL));
    TEST_ASSERT_FALSE(widgetSupportsSize(WIDGET_SPEED, SIZE_MEDIUM));
}

void test_cadence_supports_only_small() {
    TEST_ASSERT_TRUE(widgetSupportsSize(WIDGET_CADENCE, SIZE_SMALL));
    TEST_ASSERT_FALSE(widgetSupportsSize(WIDGET_CADENCE, SIZE_HERO));
}

void test_avg_speed_supports_small_and_medium() {
    TEST_ASSERT_TRUE(widgetSupportsSize(WIDGET_AVG_SPEED, SIZE_SMALL));
    TEST_ASSERT_TRUE(widgetSupportsSize(WIDGET_AVG_SPEED, SIZE_MEDIUM));
    TEST_ASSERT_FALSE(widgetSupportsSize(WIDGET_AVG_SPEED, SIZE_HERO));
}

void test_elevation_chart_supports_large_and_full() {
    TEST_ASSERT_TRUE(widgetSupportsSize(WIDGET_ELEVATION_CHART, SIZE_LARGE));
    TEST_ASSERT_TRUE(widgetSupportsSize(WIDGET_ELEVATION_CHART, SIZE_FULL));
    TEST_ASSERT_FALSE(widgetSupportsSize(WIDGET_ELEVATION_CHART, SIZE_SMALL));
}

void test_ble_manager_supports_only_full() {
    TEST_ASSERT_TRUE(widgetSupportsSize(WIDGET_BLE_MANAGER, SIZE_FULL));
    TEST_ASSERT_FALSE(widgetSupportsSize(WIDGET_BLE_MANAGER, SIZE_HERO));
    TEST_ASSERT_FALSE(widgetSupportsSize(WIDGET_BLE_MANAGER, SIZE_SMALL));
}

void test_widget_none_supports_nothing() {
    for (uint8_t s = 0; s < SIZE_CLASS_COUNT; s++) {
        TEST_ASSERT_FALSE(widgetSupportsSize(WIDGET_NONE, (SizeClass)s));
    }
}

void test_getWidgetMeta_returns_matching_type() {
    TEST_ASSERT_EQUAL(WIDGET_SPEED, getWidgetMeta(WIDGET_SPEED).type);
    TEST_ASSERT_EQUAL_STRING("SPEED", getWidgetMeta(WIDGET_SPEED).label);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_speed_supports_only_hero);
    RUN_TEST(test_cadence_supports_only_small);
    RUN_TEST(test_avg_speed_supports_small_and_medium);
    RUN_TEST(test_elevation_chart_supports_large_and_full);
    RUN_TEST(test_ble_manager_supports_only_full);
    RUN_TEST(test_widget_none_supports_nothing);
    RUN_TEST(test_getWidgetMeta_returns_matching_type);
    return UNITY_END();
}
```

Delete `test/test_ui_engine/test_widget_types.cpp`'s `main()` conflict by removing that file's `main` — PlatformIO's native runner builds one `main` per test *directory*, not per file, so consolidate: move Task 1's two tests into this file's `UNITY_BEGIN`/`RUN_TEST` block and delete `test_widget_types.cpp`, OR keep them in separate directories. Use separate directories — rename this task's test file's directory to keep Task 1's harness isolated:

Move `test/test_ui_engine/test_widget_types.cpp` to `test/test_widget_types/test_widget_types.cpp`, and put this task's file at `test/test_widget_catalog/test_widget_catalog.cpp` (PlatformIO native runs one binary per `test/<dir>/`, each needs its own `main`).

- [ ] **Step 2: Run to confirm it fails**

Run: `pio test -e native`
Expected: Compile error — `widget_catalog.h` does not exist.

- [ ] **Step 3: Create `widget_catalog.h`**

```cpp
#ifndef OPENCYCLO_UI_ENGINE_WIDGET_CATALOG_H
#define OPENCYCLO_UI_ENGINE_WIDGET_CATALOG_H

#include "widget_types.h"

struct WidgetMeta {
  WidgetType type;
  const char* name;
  const char* label;
  const char* unit_metric;
  const char* unit_imperial;
  uint8_t supported_sizes_mask; // bitmask of (1 << SizeClass)
};

const WidgetMeta& getWidgetMeta(WidgetType type);
bool widgetSupportsSize(WidgetType type, SizeClass size);

#endif // OPENCYCLO_UI_ENGINE_WIDGET_CATALOG_H
```

- [ ] **Step 4: Create `widget_catalog.cpp`**

```cpp
#include "widget_catalog.h"

#define SIZES(...) ((uint8_t)(__VA_ARGS__))
#define BIT(size) (1u << (size))

static const WidgetMeta s_catalog[WIDGET_TYPE_COUNT] = {
  {WIDGET_NONE,            "None",           "",         "",     "",      0},
  {WIDGET_SPEED,           "Speed",          "SPEED",    "km/h", "mph",   BIT(SIZE_HERO)},
  {WIDGET_AVG_SPEED,       "Avg Speed",      "avg spd",  "km/h", "mph",   BIT(SIZE_SMALL) | BIT(SIZE_MEDIUM)},
  {WIDGET_MAX_SPEED,       "Max Speed",      "max spd",  "km/h", "mph",   BIT(SIZE_SMALL) | BIT(SIZE_MEDIUM)},
  {WIDGET_DISTANCE,        "Distance",       "distance", "km",   "mi",    BIT(SIZE_MEDIUM)},
  {WIDGET_RIDE_TIME,       "Ride Time",      "ride time","",     "",      BIT(SIZE_MEDIUM)},
  {WIDGET_CADENCE,         "Cadence",        "cadence",  "rpm",  "rpm",   BIT(SIZE_SMALL)},
  {WIDGET_HEART_RATE,      "Heart Rate",     "heart",    "bpm",  "bpm",   BIT(SIZE_SMALL)},
  {WIDGET_POWER,           "Power",          "power",    "w",    "w",     BIT(SIZE_SMALL)},
  {WIDGET_ALTITUDE,        "Altitude",       "altitude", "m",    "ft",    BIT(SIZE_SMALL) | BIT(SIZE_MEDIUM)},
  {WIDGET_GRADE,           "Grade",          "grade",    "%",    "%",     BIT(SIZE_SMALL) | BIT(SIZE_MEDIUM)},
  {WIDGET_TOTAL_ASCENT,    "Total Ascent",   "ascent",   "m",    "ft",    BIT(SIZE_SMALL) | BIT(SIZE_MEDIUM)},
  {WIDGET_ELEVATION_CHART, "Elevation Chart","elevation","",     "",      BIT(SIZE_LARGE) | BIT(SIZE_FULL)},
  {WIDGET_BATTERY,         "Battery",        "battery",  "%",    "%",     BIT(SIZE_SMALL)},
  {WIDGET_BLE_MANAGER,     "BLE Manager",    "sensors",  "",     "",      BIT(SIZE_FULL)},
  {WIDGET_SETTINGS_LIST,   "Settings List",  "settings", "",     "",      BIT(SIZE_FULL)},
};

const WidgetMeta& getWidgetMeta(WidgetType type) {
  if (type < WIDGET_TYPE_COUNT) return s_catalog[type];
  return s_catalog[WIDGET_NONE];
}

bool widgetSupportsSize(WidgetType type, SizeClass size) {
  if (type >= WIDGET_TYPE_COUNT || size >= SIZE_CLASS_COUNT) return false;
  return (s_catalog[type].supported_sizes_mask & BIT(size)) != 0;
}
```

- [ ] **Step 5: Run to confirm it passes**

Run: `pio test -e native`
Expected: All tests pass, `0 Failures`.

- [ ] **Step 6: Commit**

```bash
git add src/ui/engine/widget_catalog.h src/ui/engine/widget_catalog.cpp test/test_widget_catalog test/test_widget_types
git rm test/test_ui_engine/test_widget_types.cpp 2>/dev/null || true
git commit -m "feat(ui): add widget catalog with size-class support metadata"
```

---

### Task 3: UI Config with schema versioning (framework-agnostic)

**Files:**
- Create: `src/ui/engine/ui_config.h`
- Create: `src/ui/engine/ui_config.cpp`
- Test: `test/test_ui_config/test_ui_config.cpp`

**Interfaces:**
- Consumes: `WidgetType` from `widget_types.h` (Task 1); `LayoutTemplateId` from `template_engine.h` (unchanged, already framework-agnostic).
- Produces: `#define UI_CONFIG_SCHEMA_VERSION 1u`, `#define MAX_PAGES 8`, `#define MAX_SLOTS_PER_PAGE 8`, `struct PageConfig { char title[16]; LayoutTemplateId template_id; uint8_t widget_count; WidgetType widgets[MAX_SLOTS_PER_PAGE]; };`, `struct UiConfig { uint32_t schema_version; uint8_t active_page_count; PageConfig pages[MAX_PAGES]; };`, `bool isUiConfigValid(const UiConfig& cfg, size_t bytesRead);`

- [ ] **Step 1: Write the failing test**

Create `test/test_ui_config/test_ui_config.cpp`:

```cpp
#include <unity.h>
#include "../../src/ui/engine/ui_config.h"
#include <string.h>

void test_valid_config_passes() {
    UiConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.schema_version = UI_CONFIG_SCHEMA_VERSION;
    cfg.active_page_count = 4;
    TEST_ASSERT_TRUE(isUiConfigValid(cfg, sizeof(UiConfig)));
}

void test_wrong_byte_count_fails() {
    UiConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.schema_version = UI_CONFIG_SCHEMA_VERSION;
    cfg.active_page_count = 4;
    TEST_ASSERT_FALSE(isUiConfigValid(cfg, sizeof(UiConfig) - 1));
}

void test_stale_schema_version_fails() {
    UiConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.schema_version = UI_CONFIG_SCHEMA_VERSION - 1; // simulates an older build's saved data
    cfg.active_page_count = 4;
    TEST_ASSERT_FALSE(isUiConfigValid(cfg, sizeof(UiConfig)));
}

void test_zero_pages_fails() {
    UiConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.schema_version = UI_CONFIG_SCHEMA_VERSION;
    cfg.active_page_count = 0;
    TEST_ASSERT_FALSE(isUiConfigValid(cfg, sizeof(UiConfig)));
}

void test_too_many_pages_fails() {
    UiConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.schema_version = UI_CONFIG_SCHEMA_VERSION;
    cfg.active_page_count = MAX_PAGES + 1;
    TEST_ASSERT_FALSE(isUiConfigValid(cfg, sizeof(UiConfig)));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_valid_config_passes);
    RUN_TEST(test_wrong_byte_count_fails);
    RUN_TEST(test_stale_schema_version_fails);
    RUN_TEST(test_zero_pages_fails);
    RUN_TEST(test_too_many_pages_fails);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to confirm it fails**

Run: `pio test -e native`
Expected: Compile error — `ui_config.h` does not exist.

- [ ] **Step 3: Create `ui_config.h`**

```cpp
#ifndef OPENCYCLO_UI_ENGINE_UI_CONFIG_H
#define OPENCYCLO_UI_ENGINE_UI_CONFIG_H

#include "widget_types.h"
#include "template_engine.h"
#include <stddef.h>

#define MAX_PAGES 8
#define MAX_SLOTS_PER_PAGE 8
#define UI_CONFIG_SCHEMA_VERSION 1u

struct PageConfig {
  char title[16];
  LayoutTemplateId template_id;
  uint8_t widget_count;
  WidgetType widgets[MAX_SLOTS_PER_PAGE];
};

struct UiConfig {
  uint32_t schema_version;
  uint8_t active_page_count;
  PageConfig pages[MAX_PAGES];
};

// Pure validation: does this loaded config match the current firmware's
// schema and have sane bounds? No NVS/Arduino dependency — natively testable.
bool isUiConfigValid(const UiConfig& cfg, size_t bytesRead);

#endif // OPENCYCLO_UI_ENGINE_UI_CONFIG_H
```

- [ ] **Step 4: Create `ui_config.cpp`**

```cpp
#include "ui_config.h"

bool isUiConfigValid(const UiConfig& cfg, size_t bytesRead) {
  if (bytesRead != sizeof(UiConfig)) return false;
  if (cfg.schema_version != UI_CONFIG_SCHEMA_VERSION) return false;
  if (cfg.active_page_count == 0 || cfg.active_page_count > MAX_PAGES) return false;
  return true;
}
```

- [ ] **Step 5: Run to confirm it passes**

Run: `pio test -e native`
Expected: All tests pass.

- [ ] **Step 6: Commit**

```bash
git add src/ui/engine/ui_config.h src/ui/engine/ui_config.cpp test/test_ui_config
git commit -m "feat(ui): add schema-versioned UiConfig with pure validation"
```

---

### Task 4: Template engine — typed slots with SizeClass

**Files:**
- Modify: `src/ui/engine/template_engine.h`
- Modify: `src/ui/engine/template_engine.cpp`
- Test: `test/test_template_engine/test_template_engine.cpp`

**Interfaces:**
- Consumes: `SizeClass`, `Rect` from `widget_types.h` (Task 1).
- Produces: `struct TemplateSlot { Rect rect; SizeClass size_class; };` replacing the old `Rect slot_rects[8]` field (renamed to `slots[8]`) inside `TemplateSlotDefinition`. `const TemplateSlotDefinition& getTemplateDefinition(LayoutTemplateId templateId);` (signature unchanged).

- [ ] **Step 1: Write the failing test**

Create `test/test_template_engine/test_template_engine.cpp`:

```cpp
#include <unity.h>
#include "../../src/ui/engine/template_engine.h"

void test_hero_6_grid_slot0_is_hero_sized() {
    const TemplateSlotDefinition& def = getTemplateDefinition(TEMPLATE_HERO_6_GRID);
    TEST_ASSERT_EQUAL(SIZE_HERO, def.slots[0].size_class);
    TEST_ASSERT_EQUAL(232, def.slots[0].rect.w);
    TEST_ASSERT_EQUAL(94, def.slots[0].rect.h);
}

void test_hero_6_grid_bottom_row_is_small() {
    const TemplateSlotDefinition& def = getTemplateDefinition(TEMPLATE_HERO_6_GRID);
    TEST_ASSERT_EQUAL(SIZE_SMALL, def.slots[3].size_class);
    TEST_ASSERT_EQUAL(SIZE_SMALL, def.slots[4].size_class);
    TEST_ASSERT_EQUAL(SIZE_SMALL, def.slots[5].size_class);
}

void test_2_grid_chart_slot2_is_large() {
    const TemplateSlotDefinition& def = getTemplateDefinition(TEMPLATE_2_GRID_CHART);
    TEST_ASSERT_EQUAL(SIZE_LARGE, def.slots[2].size_class);
    TEST_ASSERT_EQUAL(192, def.slots[2].rect.h);
}

void test_full_container_is_full() {
    const TemplateSlotDefinition& def = getTemplateDefinition(TEMPLATE_FULL_CONTAINER);
    TEST_ASSERT_EQUAL(SIZE_FULL, def.slots[0].size_class);
}

void test_out_of_range_template_falls_back_to_full_container() {
    const TemplateSlotDefinition& def = getTemplateDefinition((LayoutTemplateId)99);
    TEST_ASSERT_EQUAL(TEMPLATE_FULL_CONTAINER, def.id);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_hero_6_grid_slot0_is_hero_sized);
    RUN_TEST(test_hero_6_grid_bottom_row_is_small);
    RUN_TEST(test_2_grid_chart_slot2_is_large);
    RUN_TEST(test_full_container_is_full);
    RUN_TEST(test_out_of_range_template_falls_back_to_full_container);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to confirm it fails**

Run: `pio test -e native`
Expected: Compile error — `TemplateSlotDefinition` has no member `slots` (still has `slot_rects`).

- [ ] **Step 3: Update `template_engine.h`**

```cpp
#ifndef OPENCYCLO_UI_ENGINE_TEMPLATE_ENGINE_H
#define OPENCYCLO_UI_ENGINE_TEMPLATE_ENGINE_H

#include "widget_types.h"

enum LayoutTemplateId : uint8_t {
  TEMPLATE_HERO_6_GRID = 0,
  TEMPLATE_4_GRID,
  TEMPLATE_2_GRID_CHART,
  TEMPLATE_8_GRID,
  TEMPLATE_FULL_CONTAINER,
  TEMPLATE_COUNT
};

struct TemplateSlot {
  Rect rect;
  SizeClass size_class;
};

struct TemplateSlotDefinition {
  LayoutTemplateId id;
  const char* name;
  uint8_t max_slots;
  TemplateSlot slots[8];
  bool has_action_button;
  Rect action_button_rect;
};

const TemplateSlotDefinition& getTemplateDefinition(LayoutTemplateId templateId);

#endif // OPENCYCLO_UI_ENGINE_TEMPLATE_ENGINE_H
```

- [ ] **Step 4: Update `template_engine.cpp`**

```cpp
#include "template_engine.h"

static const TemplateSlotDefinition s_templates[TEMPLATE_COUNT] = {
  // 0: TEMPLATE_HERO_6_GRID
  {
    TEMPLATE_HERO_6_GRID,
    "Hero 6-Grid",
    6,
    {
      {{4, 28, 232, 94}, SIZE_HERO},    // Slot 0: Hero
      {{4, 126, 114, 60}, SIZE_MEDIUM}, // Slot 1: Mid Left
      {{122, 126, 114, 60}, SIZE_MEDIUM}, // Slot 2: Mid Right
      {{4, 190, 74, 54}, SIZE_SMALL},   // Slot 3: Bottom Left
      {{83, 190, 74, 54}, SIZE_SMALL},  // Slot 4: Bottom Center
      {{162, 190, 74, 54}, SIZE_SMALL}  // Slot 5: Bottom Right
    },
    true,
    {4, 248, 232, 50}
  },

  // 1: TEMPLATE_4_GRID (not used by any default page; kept for future custom layouts)
  {
    TEMPLATE_4_GRID,
    "4-Grid Symmetric",
    4,
    {
      {{4, 28, 114, 130}, SIZE_MEDIUM},
      {{122, 28, 114, 130}, SIZE_MEDIUM},
      {{4, 164, 114, 134}, SIZE_MEDIUM},
      {{122, 164, 114, 134}, SIZE_MEDIUM}
    },
    false,
    {0, 0, 0, 0}
  },

  // 2: TEMPLATE_2_GRID_CHART
  {
    TEMPLATE_2_GRID_CHART,
    "2-Grid + Chart",
    3,
    {
      {{4, 28, 114, 74}, SIZE_SMALL},
      {{122, 28, 114, 74}, SIZE_SMALL},
      {{4, 106, 232, 192}, SIZE_LARGE}
    },
    false,
    {0, 0, 0, 0}
  },

  // 3: TEMPLATE_8_GRID (not used by any default page; kept for future custom layouts)
  {
    TEMPLATE_8_GRID,
    "8-Grid Pro View",
    8,
    {
      {{4, 28, 114, 64}, SIZE_SMALL},   {{122, 28, 114, 64}, SIZE_SMALL},
      {{4, 96, 114, 64}, SIZE_SMALL},   {{122, 96, 114, 64}, SIZE_SMALL},
      {{4, 164, 114, 64}, SIZE_SMALL},  {{122, 164, 114, 64}, SIZE_SMALL},
      {{4, 232, 114, 64}, SIZE_SMALL},  {{122, 232, 114, 64}, SIZE_SMALL}
    },
    false,
    {0, 0, 0, 0}
  },

  // 4: TEMPLATE_FULL_CONTAINER
  {
    TEMPLATE_FULL_CONTAINER,
    "Full Container",
    1,
    {
      {{4, 28, 232, 274}, SIZE_FULL}
    },
    false,
    {0, 0, 0, 0}
  }
};

const TemplateSlotDefinition& getTemplateDefinition(LayoutTemplateId templateId) {
  if (templateId < TEMPLATE_COUNT) {
    return s_templates[templateId];
  }
  return s_templates[TEMPLATE_FULL_CONTAINER];
}
```

- [ ] **Step 5: Run to confirm it passes**

Run: `pio test -e native`
Expected: All tests pass.

- [ ] **Step 6: Commit**

```bash
git add src/ui/engine/template_engine.h src/ui/engine/template_engine.cpp test/test_template_engine
git commit -m "feat(ui): tag every template slot with its SizeClass"
```

---

### Task 5: Widget registry — split metadata from rendering, size-validated dispatch

**Files:**
- Modify: `src/ui/engine/widget_registry.h`
- Modify: `src/ui/engine/widget_registry.cpp` (gut it — rendering functions get rebuilt in Tasks 6-10; this task only fixes the dispatch/registration plumbing and validation, using temporary stub render functions)

**Interfaces:**
- Consumes: `getWidgetMeta`, `widgetSupportsSize` from `widget_catalog.h` (Task 2); `TemplateSlot` from `template_engine.h` (Task 4).
- Produces: `typedef void (*WidgetRenderFn)(const Rect& bounds, const TelemetryState& state, bool forceFullRedraw);` (unchanged) `typedef bool (*WidgetTouchFn)(const Rect& bounds, int16_t x, int16_t y);` (unchanged) `struct WidgetDescriptor { WidgetType type; WidgetRenderFn render_fn; WidgetTouchFn touch_fn; };` (metadata fields removed — read via `getWidgetMeta()` instead) `void renderWidget(WidgetType type, const TemplateSlot& slot, const TelemetryState& state, bool forceFullRedraw);` (signature changed: takes `TemplateSlot` instead of bare `Rect`, so it can validate size) `bool handleWidgetTouch(WidgetType type, const Rect& bounds, int16_t x, int16_t y);` (unchanged)

This task is plumbing-only; there's no meaningful native test (it depends on `hardware/display.h`/LovyanGFX, same as before). Verification is a successful embedded build with visibly blank tiles (stub renderers), confirming the dispatch/validation logic runs without crashing, before Tasks 6-10 fill in real rendering.

- [ ] **Step 1: Rewrite `widget_registry.h`**

```cpp
#ifndef OPENCYCLO_UI_ENGINE_WIDGET_REGISTRY_H
#define OPENCYCLO_UI_ENGINE_WIDGET_REGISTRY_H

#include "widget_types.h"
#include "template_engine.h"
#include "core/telemetry_state.h"
#include "hardware/display.h"

typedef void (*WidgetRenderFn)(const Rect& bounds, const TelemetryState& state, bool forceFullRedraw);
typedef bool (*WidgetTouchFn)(const Rect& bounds, int16_t x, int16_t y);

struct WidgetDescriptor {
  WidgetType type;
  WidgetRenderFn render_fn;
  WidgetTouchFn touch_fn;
};

void initWidgetRegistry();
const WidgetDescriptor* getWidgetDescriptor(WidgetType type);

// Renders `type` into `slot` only if the widget supports slot.size_class;
// otherwise draws a blank placeholder tile (background fill, no text) so an
// invalid config can never produce overlapping/garbled text again.
void renderWidget(WidgetType type, const TemplateSlot& slot, const TelemetryState& state, bool forceFullRedraw);
bool handleWidgetTouch(WidgetType type, const Rect& bounds, int16_t x, int16_t y);

#endif // OPENCYCLO_UI_ENGINE_WIDGET_REGISTRY_H
```

- [ ] **Step 2: Rewrite `widget_registry.cpp` with stub renderers**

```cpp
#include "widget_registry.h"
#include "widget_catalog.h"

static uint16_t COLOR_BG = TFT_BLACK;

// Stub renderer used for every widget until Tasks 6-10 replace it with the
// real Minimal-style implementation. Draws only the background fill so the
// dispatch/validation plumbing in this task is independently verifiable.
static void renderStub(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRect(b.x, b.y, b.w, b.h, COLOR_BG);
  }
}

static const WidgetDescriptor s_descriptors[WIDGET_TYPE_COUNT] = {
  {WIDGET_NONE,            nullptr,    nullptr},
  {WIDGET_SPEED,           renderStub, nullptr},
  {WIDGET_AVG_SPEED,       renderStub, nullptr},
  {WIDGET_MAX_SPEED,       renderStub, nullptr},
  {WIDGET_DISTANCE,        renderStub, nullptr},
  {WIDGET_RIDE_TIME,       renderStub, nullptr},
  {WIDGET_CADENCE,         renderStub, nullptr},
  {WIDGET_HEART_RATE,      renderStub, nullptr},
  {WIDGET_POWER,           renderStub, nullptr},
  {WIDGET_ALTITUDE,        renderStub, nullptr},
  {WIDGET_GRADE,           renderStub, nullptr},
  {WIDGET_TOTAL_ASCENT,    renderStub, nullptr},
  {WIDGET_ELEVATION_CHART, renderStub, nullptr},
  {WIDGET_BATTERY,         renderStub, nullptr},
  {WIDGET_BLE_MANAGER,     renderStub, nullptr},
  {WIDGET_SETTINGS_LIST,   renderStub, nullptr},
};

void initWidgetRegistry() {
  Serial.println("[WIDGET REGISTRY] Initialized 15 modular widgets (Minimal style rebuild in progress).");
}

const WidgetDescriptor* getWidgetDescriptor(WidgetType type) {
  if (type < WIDGET_TYPE_COUNT) return &s_descriptors[type];
  return &s_descriptors[WIDGET_NONE];
}

void renderWidget(WidgetType type, const TemplateSlot& slot, const TelemetryState& state, bool forceFullRedraw) {
  if (type >= WIDGET_TYPE_COUNT) return;
  if (!widgetSupportsSize(type, slot.size_class)) {
    // Invalid widget/slot pairing (e.g. stale config): fail safe, don't draw garbage.
    if (forceFullRedraw) {
      tft.fillRect(slot.rect.x, slot.rect.y, slot.rect.w, slot.rect.h, COLOR_BG);
    }
    return;
  }
  const WidgetRenderFn fn = s_descriptors[type].render_fn;
  if (fn != nullptr) {
    fn(slot.rect, state, forceFullRedraw);
  }
}

bool handleWidgetTouch(WidgetType type, const Rect& bounds, int16_t x, int16_t y) {
  if (type < WIDGET_TYPE_COUNT && s_descriptors[type].touch_fn != nullptr) {
    return s_descriptors[type].touch_fn(bounds, x, y);
  }
  return false;
}
```

- [ ] **Step 3: Build for the embedded target to confirm it compiles**

Run: `pio run`
Expected: `SUCCESS` (the old 18-widget descriptor table and its LovyanGFX-drawing code, which referenced removed enum values like `WIDGET_CLOCK`, is fully replaced — no stale references remain).

- [ ] **Step 4: Commit**

```bash
git add src/ui/engine/widget_registry.h src/ui/engine/widget_registry.cpp
git commit -m "refactor(ui): split widget metadata into catalog, add size-validated dispatch"
```

---

### Task 6: Ride page tiles — Speed (hero), Distance, Ride Time, Cadence, Heart Rate, Power

**Files:**
- Modify: `src/ui/engine/widget_registry.cpp`

**Interfaces:**
- Consumes: `TelemetryState` fields `speed_kmh`, `speed_source`, `trip_distance_km`, `ride_time_s`, `cadence_rpm`, `heart_rate_bpm`, `power_watts` (all already defined in `core/telemetry_state.h`, unchanged); `g_settings.units` from `storage/settings.h` (unchanged).
- Produces: real `render_fn` implementations replacing `renderStub` for `WIDGET_SPEED`, `WIDGET_DISTANCE`, `WIDGET_RIDE_TIME`, `WIDGET_CADENCE`, `WIDGET_HEART_RATE`, `WIDGET_POWER` in the `s_descriptors` table from Task 5.

- [ ] **Step 1: Replace the stub descriptor entries and add real render functions**

In `widget_registry.cpp`, add these includes at the top (alongside the existing ones):

```cpp
#include "storage/settings.h"
#include <stdio.h>
```

Add these render functions above `s_descriptors` (keep `renderStub` — it still backs the not-yet-implemented widgets from Tasks 7-10):

```cpp
static uint16_t COLOR_HAIRLINE = tft.color565(28, 28, 28);
static uint16_t COLOR_TEXT     = TFT_WHITE;
static uint16_t COLOR_LABEL    = tft.color565(102, 102, 102);
static uint16_t COLOR_GREEN    = tft.color565(46, 213, 115);
static uint16_t COLOR_AMBER    = tft.color565(255, 171, 0);
static uint16_t COLOR_RED      = tft.color565(255, 71, 87);
static uint16_t COLOR_CYAN     = tft.color565(0, 210, 255);

// 1. SPEED (hero) — the one widget with its own layout, everything else is a "tile."
static void renderWidgetSpeed(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRect(b.x, b.y, b.w, b.h, COLOR_BG);
  }
  tft.setFont(&fonts::FreeSans9pt7b);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(b.x + 4, b.y + 4);
  tft.setTextPadding(b.w - 8);
  tft.printf("speed . %s", state.speed_source == SPEED_SOURCE_BLE_CSC ? "ble" : "gps");
  tft.setTextPadding(0);

  tft.setFont(&fonts::FreeSans24pt7b);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  char buf[12];
  float speed = (g_settings.units == 1) ? (state.speed_kmh * 0.621371f) : state.speed_kmh;
  snprintf(buf, sizeof(buf), "%.1f", speed);
  tft.setCursor(b.x + 4, b.y + 24);
  tft.setTextPadding(b.w - 70);
  tft.print(buf);
  tft.setTextPadding(0);

  tft.setFont(&fonts::FreeSans9pt7b);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(b.x + b.w - 44, b.y + b.h - 18);
  tft.print((g_settings.units == 1) ? "mph" : "km/h");
}

// Shared layout for every SMALL/MEDIUM "label above, value below, hairline
// above the tile" widget — same visual pattern, different label/value/color.
static void renderTile(const Rect& b, const char* label, const char* valueStr, uint16_t valueColor, bool force) {
  if (force) {
    tft.fillRect(b.x, b.y, b.w, b.h, COLOR_BG);
    tft.drawFastHLine(b.x, b.y, b.w, COLOR_HAIRLINE);
  }
  tft.setFont(&fonts::FreeSans9pt7b);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(b.x + 4, b.y + 6);
  tft.setTextPadding(b.w - 8);
  tft.print(label);
  tft.setTextPadding(0);

  tft.setFont(&fonts::FreeSans12pt7b);
  tft.setTextColor(valueColor, COLOR_BG);
  tft.setCursor(b.x + 4, b.y + 22);
  tft.setTextPadding(b.w - 8);
  tft.print(valueStr);
  tft.setTextPadding(0);
}

// 2. DISTANCE
static void renderWidgetDistance(const Rect& b, const TelemetryState& state, bool force) {
  char buf[16];
  float d = (g_settings.units == 1) ? (state.trip_distance_km * 0.621371f) : state.trip_distance_km;
  snprintf(buf, sizeof(buf), "%.2f %s", d, (g_settings.units == 1) ? "mi" : "km");
  renderTile(b, "distance", buf, COLOR_TEXT, force);
}

// 3. RIDE TIME
static void renderWidgetRideTime(const Rect& b, const TelemetryState& state, bool force) {
  char buf[16];
  uint32_t hrs = state.ride_time_s / 3600;
  uint32_t mins = (state.ride_time_s % 3600) / 60;
  uint32_t secs = state.ride_time_s % 60;
  if (hrs > 0) {
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", hrs, mins, secs);
  } else {
    snprintf(buf, sizeof(buf), "%02u:%02u", mins, secs);
  }
  renderTile(b, "ride time", buf, COLOR_TEXT, force);
}

// 4. CADENCE
static void renderWidgetCadence(const Rect& b, const TelemetryState& state, bool force) {
  char buf[8];
  if (state.cadence_rpm >= 0) snprintf(buf, sizeof(buf), "%d", state.cadence_rpm);
  else snprintf(buf, sizeof(buf), "--");
  renderTile(b, "cadence", buf, state.cadence_rpm >= 0 ? COLOR_CYAN : COLOR_LABEL, force);
}

// 5. HEART RATE
static void renderWidgetHeartRate(const Rect& b, const TelemetryState& state, bool force) {
  char buf[8];
  if (state.heart_rate_bpm >= 0) snprintf(buf, sizeof(buf), "%d", state.heart_rate_bpm);
  else snprintf(buf, sizeof(buf), "--");
  renderTile(b, "heart", buf, state.heart_rate_bpm >= 0 ? COLOR_RED : COLOR_LABEL, force);
}

// 6. POWER
static void renderWidgetPower(const Rect& b, const TelemetryState& state, bool force) {
  char buf[8];
  if (state.power_watts >= 0) snprintf(buf, sizeof(buf), "%d", state.power_watts);
  else snprintf(buf, sizeof(buf), "--");
  renderTile(b, "power", buf, state.power_watts >= 0 ? COLOR_GREEN : COLOR_LABEL, force);
}
```

Update the six corresponding rows in `s_descriptors`:

```cpp
  {WIDGET_SPEED,           renderWidgetSpeed,     nullptr},
  ...
  {WIDGET_DISTANCE,        renderWidgetDistance,  nullptr},
  {WIDGET_RIDE_TIME,       renderWidgetRideTime,  nullptr},
  {WIDGET_CADENCE,         renderWidgetCadence,   nullptr},
  {WIDGET_HEART_RATE,      renderWidgetHeartRate, nullptr},
  {WIDGET_POWER,           renderWidgetPower,     nullptr},
```

(Leave `WIDGET_AVG_SPEED`, `WIDGET_MAX_SPEED`, `WIDGET_ALTITUDE`, `WIDGET_GRADE`, `WIDGET_TOTAL_ASCENT`, `WIDGET_ELEVATION_CHART`, `WIDGET_BATTERY`, `WIDGET_BLE_MANAGER`, `WIDGET_SETTINGS_LIST` pointed at `renderStub` — Tasks 7-10 fill those in.)

- [ ] **Step 2: Build and flash**

Run: `pio run -t upload`
Expected: `SUCCESS`, flashes without error.

- [ ] **Step 3: Visually verify on the physical panel**

Swipe to the Ride page. Confirm: the hero SPEED tile shows a light-weight numeral with a `speed . gps` label above and `km/h` unit at bottom-right, no overlapping text; the five tiles below each show a lowercase label, a thin hairline above the tile, and a value that updates live (ride time ticking, distance/cadence/HR/power reflecting `TelemetryState`). If any value text clips against its tile edge, narrow the `setTextPadding()`/adjust the cursor x in that widget's function and reflash — this is the expected on-device tuning step for a font change.

- [ ] **Step 4: Commit**

```bash
git add src/ui/engine/widget_registry.cpp
git commit -m "feat(ui): rebuild Speed/Distance/RideTime/Cadence/HeartRate/Power in Minimal style"
```

---

### Task 7: Climb page tiles — Altitude, Grade, Total Ascent, Elevation Chart

**Files:**
- Modify: `src/ui/engine/widget_registry.cpp`

**Interfaces:**
- Consumes: `TelemetryState` fields `altitude_m`, `grade_pct`, `total_ascent_m` (unchanged); `renderTile()` helper from Task 6.
- Produces: real `render_fn` implementations for `WIDGET_ALTITUDE`, `WIDGET_GRADE`, `WIDGET_TOTAL_ASCENT`, `WIDGET_ELEVATION_CHART`.

- [ ] **Step 1: Add the four render functions**

```cpp
// 7. ALTITUDE
static void renderWidgetAltitude(const Rect& b, const TelemetryState& state, bool force) {
  char buf[16];
  float alt = (g_settings.units == 1) ? (state.altitude_m * 3.28084f) : state.altitude_m;
  snprintf(buf, sizeof(buf), "%.0f %s", alt, (g_settings.units == 1) ? "ft" : "m");
  renderTile(b, "altitude", buf, COLOR_TEXT, force);
}

// 8. GRADE
static void renderWidgetGrade(const Rect& b, const TelemetryState& state, bool force) {
  char buf[12];
  snprintf(buf, sizeof(buf), "%+.1f%%", state.grade_pct);
  uint16_t gradeColor = (state.grade_pct > 3.0f) ? COLOR_AMBER : ((state.grade_pct < -2.0f) ? COLOR_CYAN : COLOR_GREEN);
  renderTile(b, "grade", buf, gradeColor, force);
}

// 9. TOTAL ASCENT
static void renderWidgetTotalAscent(const Rect& b, const TelemetryState& state, bool force) {
  char buf[16];
  float asc = (g_settings.units == 1) ? (state.total_ascent_m * 3.28084f) : state.total_ascent_m;
  snprintf(buf, sizeof(buf), "%.0f %s", asc, (g_settings.units == 1) ? "ft" : "m");
  renderTile(b, "ascent", buf, COLOR_GREEN, force);
}

// 10. ELEVATION CHART
#define ELEV_SAMPLES 30
static float s_elevHistory[ELEV_SAMPLES];
static uint8_t s_elevHead = 0;
static bool s_elevFilled = false;

static void renderWidgetElevationChart(const Rect& b, const TelemetryState& state, bool force) {
  s_elevHistory[s_elevHead] = state.altitude_m;
  s_elevHead = (s_elevHead + 1) % ELEV_SAMPLES;
  if (s_elevHead == 0) s_elevFilled = true;

  if (force) {
    tft.fillRect(b.x, b.y, b.w, b.h, COLOR_BG);
    tft.drawFastHLine(b.x, b.y, b.w, COLOR_HAIRLINE);
    tft.setFont(&fonts::FreeSans9pt7b);
    tft.setTextColor(COLOR_LABEL, COLOR_BG);
    tft.setCursor(b.x + 4, b.y + 6);
    tft.print("elevation profile");
  }

  float minAlt = 99999.0f, maxAlt = -99999.0f;
  uint8_t count = s_elevFilled ? ELEV_SAMPLES : s_elevHead;
  if (count < 2) count = 2;
  for (uint8_t i = 0; i < count; i++) {
    float val = s_elevHistory[i];
    if (val < minAlt) minAlt = val;
    if (val > maxAlt) maxAlt = val;
  }
  if (maxAlt - minAlt < 5.0f) maxAlt = minAlt + 5.0f;

  int innerX = b.x + 4;
  int innerY = b.y + 26;
  int innerW = b.w - 8;
  int innerH = b.h - 32;
  tft.fillRect(innerX, innerY, innerW, innerH, COLOR_BG);

  int prevPx = -1, prevPy = -1;
  for (uint8_t i = 0; i < count; i++) {
    uint8_t idx = s_elevFilled ? ((s_elevHead + i) % ELEV_SAMPLES) : i;
    float val = s_elevHistory[idx];
    int px = innerX + (i * innerW) / (ELEV_SAMPLES - 1);
    int py = innerY + innerH - (int)(((val - minAlt) / (maxAlt - minAlt)) * (innerH - 4));
    if (prevPx != -1) {
      tft.drawLine(prevPx, prevPy, px, py, COLOR_GREEN);
    }
    prevPx = px;
    prevPy = py;
  }
}
```

Update the four descriptor rows:

```cpp
  {WIDGET_ALTITUDE,        renderWidgetAltitude,       nullptr},
  {WIDGET_GRADE,           renderWidgetGrade,          nullptr},
  {WIDGET_TOTAL_ASCENT,    renderWidgetTotalAscent,    nullptr},
  {WIDGET_ELEVATION_CHART, renderWidgetElevationChart, nullptr},
```

- [ ] **Step 2: Build and flash**

Run: `pio run -t upload`
Expected: `SUCCESS`.

- [ ] **Step 3: Visually verify on the physical panel**

Swipe to the Climb page. Confirm altitude/grade tiles show correctly-colored values (grade turns amber on a climb, cyan on a descent, green flat) and the elevation chart draws a line graph that grows as altitude samples accumulate, without clipping its "elevation profile" label.

- [ ] **Step 4: Commit**

```bash
git add src/ui/engine/widget_registry.cpp
git commit -m "feat(ui): rebuild Altitude/Grade/TotalAscent/ElevationChart in Minimal style"
```

---

### Task 8: Avg Speed, Max Speed, Battery tiles

**Files:**
- Modify: `src/ui/engine/widget_registry.cpp`

**Interfaces:**
- Consumes: `TelemetryState` fields `avg_speed_kmh`, `max_speed_kmh`, `battery_pct`; `readBatteryVoltage()` from `hardware/battery.h` (unchanged); `renderTile()` helper from Task 6.
- Produces: real `render_fn` implementations for `WIDGET_AVG_SPEED`, `WIDGET_MAX_SPEED`, `WIDGET_BATTERY`.

- [ ] **Step 1: Add the three render functions**

Add this include near the top of the file: `#include "hardware/battery.h"`

```cpp
// 11. AVG SPEED
static void renderWidgetAvgSpeed(const Rect& b, const TelemetryState& state, bool force) {
  char buf[12];
  float avg = (g_settings.units == 1) ? (state.avg_speed_kmh * 0.621371f) : state.avg_speed_kmh;
  snprintf(buf, sizeof(buf), "%.1f", avg);
  renderTile(b, "avg spd", buf, COLOR_TEXT, force);
}

// 12. MAX SPEED
static void renderWidgetMaxSpeed(const Rect& b, const TelemetryState& state, bool force) {
  char buf[12];
  float maxS = (g_settings.units == 1) ? (state.max_speed_kmh * 0.621371f) : state.max_speed_kmh;
  snprintf(buf, sizeof(buf), "%.1f", maxS);
  renderTile(b, "max spd", buf, COLOR_TEXT, force);
}

// 13. BATTERY
static void renderWidgetBattery(const Rect& b, const TelemetryState& state, bool force) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%u%%", state.battery_pct);
  uint16_t color = (state.battery_pct < 20) ? COLOR_AMBER : COLOR_GREEN;
  renderTile(b, "battery", buf, color, force);
}
```

Update the three descriptor rows:

```cpp
  {WIDGET_AVG_SPEED,       renderWidgetAvgSpeed,  nullptr},
  {WIDGET_MAX_SPEED,       renderWidgetMaxSpeed,  nullptr},
  ...
  {WIDGET_BATTERY,         renderWidgetBattery,   nullptr},
```

- [ ] **Step 2: Build and flash**

Run: `pio run -t upload`
Expected: `SUCCESS`.

- [ ] **Step 3: Visually verify**

These three aren't in the default page set yet (Task 12 adds pages that use them, if desired) — verify by temporarily editing `resetLayoutToDefaults()` to swap one Ride-page slot to `WIDGET_AVG_SPEED` or `WIDGET_BATTERY`, reflash, confirm it renders cleanly, then revert that temporary edit (Task 12 owns the real default layout).

- [ ] **Step 4: Commit**

```bash
git add src/ui/engine/widget_registry.cpp
git commit -m "feat(ui): rebuild AvgSpeed/MaxSpeed/Battery tiles in Minimal style"
```

---

### Task 9: BLE Manager widget (full-page)

**Files:**
- Modify: `src/ui/engine/widget_registry.cpp`

**Interfaces:**
- Consumes: `g_settings.paired_csc_mac/paired_hr_mac/paired_power_mac`, `forgetSensorProfile()`, `g_ble_scanning`, `triggerBleScan()` from `hardware/ble_task.h` (all unchanged from the existing implementation being ported).
- Produces: real `render_fn`/`touch_fn` for `WIDGET_BLE_MANAGER`.

- [ ] **Step 1: Add the render and touch functions**

Add this include near the top: `#include "hardware/ble_task.h"`

```cpp
// 14. BLE MANAGER
static void renderWidgetBleManager(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRect(b.x, b.y, b.w, b.h, COLOR_BG);
  }
  uint16_t scanBtnColor = g_ble_scanning ? COLOR_AMBER : COLOR_CYAN;
  tft.fillRoundRect(b.x + 6, b.y + 6, b.w - 12, 28, 6, scanBtnColor);
  tft.setFont(&fonts::FreeSans9pt7b);
  tft.setTextColor(TFT_BLACK, scanBtnColor);
  tft.setCursor(b.x + 40, b.y + 16);
  tft.print(g_ble_scanning ? "scanning..." : "scan & add sensors");

  struct Row { const char* label; const char* mac; uint8_t profile; };
  Row rows[3] = {
    {"speed/cad", g_settings.paired_csc_mac, 0},
    {"heart rate", g_settings.paired_hr_mac, 1},
    {"power meter", g_settings.paired_power_mac, 2},
  };

  int y = b.y + 48;
  for (int i = 0; i < 3; i++) {
    tft.setTextColor(COLOR_LABEL, COLOR_BG);
    tft.setCursor(b.x + 10, y);
    tft.setTextPadding(76);
    tft.print(rows[i].label);
    tft.setTextPadding(0);

    tft.setCursor(b.x + 90, y);
    tft.setTextPadding(b.w - 100);
    if (rows[i].mac[0] != '\0') {
      tft.setTextColor(COLOR_GREEN, COLOR_BG);
      tft.printf("%.10s..", rows[i].mac);
      tft.fillRoundRect(b.x + b.w - 60, y - 4, 52, 18, 4, COLOR_RED);
      tft.setTextColor(TFT_WHITE, COLOR_RED);
      tft.setCursor(b.x + b.w - 54, y);
      tft.print("forget");
    } else {
      tft.setTextColor(COLOR_LABEL, COLOR_BG);
      tft.print("not paired");
    }
    tft.setTextPadding(0);
    tft.drawFastHLine(b.x + 6, y + 22, b.w - 12, COLOR_HAIRLINE);
    y += 34;
  }

  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(b.x + 10, y + 6);
  tft.setTextPadding(b.w - 20);
  tft.print(state.gps_has_fix ? "gps: 3d fix valid" : "gps: searching...");
  tft.setTextPadding(0);
}

static bool touchWidgetBleManager(const Rect& b, int16_t x, int16_t y) {
  if (x >= b.x + 6 && x <= b.x + b.w - 6 && y >= b.y + 6 && y <= b.y + 34) {
    triggerBleScan();
    return true;
  }
  static const BleProfileType kRowProfiles[3] = {BLE_PROFILE_CSC, BLE_PROFILE_HR, BLE_PROFILE_POWER};
  int rowY = b.y + 48;
  for (uint8_t i = 0; i < 3; i++) {
    if (x >= b.x + b.w - 60 && x <= b.x + b.w - 8 && y >= rowY - 4 && y <= rowY + 14) {
      forgetSensorProfile(kRowProfiles[i]);
      return true;
    }
    rowY += 34;
  }
  return false;
}
```

Update the descriptor row:

```cpp
  {WIDGET_BLE_MANAGER,     renderWidgetBleManager, touchWidgetBleManager},
```

- [ ] **Step 2: Build and flash**

Run: `pio run -t upload`
Expected: `SUCCESS`. (Confirmed against `hardware/ble_task.h`: `enum BleProfileType { BLE_PROFILE_CSC=0, BLE_PROFILE_HR, BLE_PROFILE_POWER };` and `void forgetSensorProfile(BleProfileType profile);` — the code above uses these exact names.)

- [ ] **Step 3: Visually verify**

Not in the default page set until Task 12 — verify the same way as Task 8 (temporary slot swap), or wait for Task 12's Sensors page. Confirm the pairing rows render without overlap and the forget/scan buttons remain tappable at their drawn positions.

- [ ] **Step 4: Commit**

```bash
git add src/ui/engine/widget_registry.cpp
git commit -m "feat(ui): rebuild BLE Manager widget in Minimal style"
```

---

### Task 10: Settings List widget (full-page)

**Files:**
- Modify: `src/ui/engine/widget_registry.cpp`

**Interfaces:**
- Consumes: `g_settings.units/brightness/wheel_circumference_mm/sd_logging_enabled`, `saveSettings()` from `storage/settings.h` (unchanged); `setDisplayBrightness()` from `hardware/display.h` (unchanged); `readBatteryVoltage()` from `hardware/battery.h` (Task 8's include).
- Produces: real `render_fn`/`touch_fn` for `WIDGET_SETTINGS_LIST`.

- [ ] **Step 1: Add the render and touch functions**

```cpp
// 15. SETTINGS LIST
static void renderWidgetSettingsList(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    tft.fillRect(b.x, b.y, b.w, b.h, COLOR_BG);
  }
  int y = b.y + 16;
  tft.setFont(&fonts::FreeSans9pt7b);

  auto row = [&](const char* label, const char* value, uint16_t valueColor) {
    tft.setTextColor(COLOR_LABEL, COLOR_BG);
    tft.setCursor(b.x + 10, y);
    tft.setTextPadding(110);
    tft.print(label);
    tft.setTextPadding(0);
    tft.setTextColor(valueColor, COLOR_BG);
    tft.setCursor(b.x + 116, y);
    tft.setTextPadding(b.w - 126);
    tft.print(value);
    tft.setTextPadding(0);
    tft.drawFastHLine(b.x + 6, y + 20, b.w - 12, COLOR_HAIRLINE);
    y += 36;
  };

  row("units", g_settings.units == 0 ? "metric" : "imperial", COLOR_TEXT);

  char brightStr[8];
  snprintf(brightStr, sizeof(brightStr), "%u%%", (g_settings.brightness * 100) / 255);
  row("brightness", brightStr, COLOR_TEXT);

  char wheelStr[24];
  snprintf(wheelStr, sizeof(wheelStr), "%u mm", g_settings.wheel_circumference_mm);
  row("wheel size", wheelStr, COLOR_TEXT);

  row("sd logging", g_settings.sd_logging_enabled ? "enabled" : "disabled",
      g_settings.sd_logging_enabled ? COLOR_GREEN : COLOR_AMBER);

  char batStr[24];
  snprintf(batStr, sizeof(batStr), "%u%% (%.2fV)", state.battery_pct, readBatteryVoltage());
  row("battery", batStr, COLOR_GREEN);

  row("firmware", "opencyclo v0.2.0", COLOR_CYAN);
}

static bool touchWidgetSettingsList(const Rect& b, int16_t x, int16_t y) {
  int rowY = b.y + 16;
  if (y >= rowY - 8 && y <= rowY + 12) {
    g_settings.units = (g_settings.units == 0) ? 1 : 0;
    saveSettings();
    return true;
  }
  rowY += 36;
  if (y >= rowY - 8 && y <= rowY + 12) {
    g_settings.brightness = (g_settings.brightness >= 250) ? 50 : (g_settings.brightness + 50);
    setDisplayBrightness(g_settings.brightness);
    saveSettings();
    return true;
  }
  rowY += 72; // skip wheel size row (not touch-editable)
  if (y >= rowY - 8 && y <= rowY + 12) {
    g_settings.sd_logging_enabled = !g_settings.sd_logging_enabled;
    saveSettings();
    return true;
  }
  return false;
}
```

Update the descriptor row:

```cpp
  {WIDGET_SETTINGS_LIST,   renderWidgetSettingsList, touchWidgetSettingsList},
```

- [ ] **Step 2: Build and flash**

Run: `pio run -t upload`
Expected: `SUCCESS`.

- [ ] **Step 3: Visually verify**

Not in the default page set until Task 12 — verify via temporary slot swap as in Task 8.

- [ ] **Step 4: Commit**

```bash
git add src/ui/engine/widget_registry.cpp
git commit -m "feat(ui): rebuild Settings List widget in Minimal style"
```

---

### Task 11: Layout manager — Minimal-style status bar, size-validated render loop

**Files:**
- Modify: `src/ui/engine/layout_manager.h`
- Modify: `src/ui/engine/layout_manager.cpp`

**Interfaces:**
- Consumes: `TemplateSlot` from `template_engine.h` (Task 4); `UiConfig`/`PageConfig` from `ui_config.h` (Task 3, replaces the definitions layout_manager.h used to own); updated `renderWidget(WidgetType, const TemplateSlot&, ...)` signature from Task 5.
- Produces: `renderPage(const PageConfig&, uint8_t pageIdx, uint8_t totalPages, const TelemetryState&, bool forceFullRedraw)` (signature unchanged) `handlePageTouch(const PageConfig&, int16_t x, int16_t y)` (signature unchanged).

- [ ] **Step 1: Update `layout_manager.h`**

Replace the file — `PageConfig`/`UiConfig`/`MAX_PAGES`/`MAX_SLOTS_PER_PAGE` now live in `ui_config.h` (Task 3); this header keeps only the render/touch declarations:

```cpp
#ifndef OPENCYCLO_UI_ENGINE_LAYOUT_MANAGER_H
#define OPENCYCLO_UI_ENGINE_LAYOUT_MANAGER_H

#include "widget_types.h"
#include "template_engine.h"
#include "widget_registry.h"
#include "ui_config.h"
#include "core/telemetry_state.h"

void renderPage(const PageConfig& page, uint8_t pageIdx, uint8_t totalPages, const TelemetryState& state, bool forceFullRedraw);
bool handlePageTouch(const PageConfig& page, int16_t x, int16_t y);

#endif // OPENCYCLO_UI_ENGINE_LAYOUT_MANAGER_H
```

- [ ] **Step 2: Update `layout_manager.cpp`**

Replace the file:

```cpp
#include "layout_manager.h"
#include <stdio.h>

static uint16_t COLOR_BG       = TFT_BLACK;
static uint16_t COLOR_TEXT     = TFT_WHITE;
static uint16_t COLOR_LABEL    = tft.color565(102, 102, 102);
static uint16_t COLOR_GREEN    = tft.color565(46, 213, 115);
static uint16_t COLOR_AMBER    = tft.color565(255, 171, 0);
static uint16_t COLOR_CYAN     = tft.color565(0, 210, 255);

void renderPage(const PageConfig& page, uint8_t pageIdx, uint8_t totalPages, const TelemetryState& state, bool forceFullRedraw) {
  const TemplateSlotDefinition& slotDef = getTemplateDefinition(page.template_id);

  if (forceFullRedraw) {
    tft.fillScreen(COLOR_BG);

    tft.setFont(&fonts::FreeSans9pt7b);
    tft.setTextColor(COLOR_LABEL, COLOR_BG);
    tft.setCursor(6, 6);
    tft.print(page.title);

    if (totalPages > 1) {
      int startX = 120 - (totalPages * 8) / 2;
      for (int i = 0; i < totalPages; i++) {
        uint16_t dotColor = (i == pageIdx) ? COLOR_CYAN : COLOR_LABEL;
        tft.fillCircle(startX + (i * 8), 312, (i == pageIdx) ? 3 : 2, dotColor);
      }
    }
  }

  tft.setFont(&fonts::FreeSans9pt7b);
  tft.setTextColor(state.gps_has_fix ? COLOR_GREEN : COLOR_AMBER, COLOR_BG);
  tft.setCursor(6, 22);
  tft.setTextPadding(120);
  if (state.gps_has_fix) {
    tft.printf("gps 3d (%u)", state.satellites);
  } else {
    tft.print("gps search");
  }
  tft.setTextPadding(0);

  uint16_t stateColor = (state.ride_state == RIDE_STATE_ACTIVE) ? COLOR_GREEN :
                        ((state.ride_state == RIDE_STATE_PAUSED) ? COLOR_AMBER : COLOR_LABEL);
  tft.setTextColor(stateColor, COLOR_BG);
  tft.setCursor(150, 22);
  tft.setTextPadding(90);
  tft.printf("%s %u%%", (state.ride_state == RIDE_STATE_ACTIVE) ? "rec" :
                        ((state.ride_state == RIDE_STATE_PAUSED) ? "pause" : "stop"), state.battery_pct);
  tft.setTextPadding(0);

  uint8_t count = (page.widget_count < slotDef.max_slots) ? page.widget_count : slotDef.max_slots;
  for (uint8_t i = 0; i < count; i++) {
    WidgetType wType = page.widgets[i];
    if (wType != WIDGET_NONE) {
      renderWidget(wType, slotDef.slots[i], state, forceFullRedraw);
    }
  }

  if (slotDef.has_action_button) {
    const Rect& btn = slotDef.action_button_rect;
    uint16_t btnColor = (state.ride_state == RIDE_STATE_ACTIVE) ? COLOR_AMBER : COLOR_GREEN;
    tft.fillRoundRect(btn.x, btn.y, btn.w, btn.h, 8, btnColor);
    tft.setFont(&fonts::FreeSans12pt7b);
    tft.setTextColor(TFT_BLACK, btnColor);
    tft.setCursor(btn.x + 50, btn.y + 16);
    if (state.ride_state == RIDE_STATE_ACTIVE) {
      tft.print("pause ride");
    } else if (state.ride_state == RIDE_STATE_PAUSED) {
      tft.print("resume ride");
    } else {
      tft.print("start ride");
    }
  }
}

bool handlePageTouch(const PageConfig& page, int16_t x, int16_t y) {
  const TemplateSlotDefinition& slotDef = getTemplateDefinition(page.template_id);

  if (slotDef.has_action_button) {
    const Rect& btn = slotDef.action_button_rect;
    if (x >= btn.x && x <= btn.x + btn.w && y >= btn.y && y <= btn.y + btn.h) {
      TelemetryState state = getTelemetrySnapshot();
      state.ride_state = (state.ride_state == RIDE_STATE_ACTIVE) ? RIDE_STATE_PAUSED : RIDE_STATE_ACTIVE;
      setTelemetryState(state);
      Serial.println("[UI] Ride state toggled via Action Button");
      return true;
    }
  }

  uint8_t count = (page.widget_count < slotDef.max_slots) ? page.widget_count : slotDef.max_slots;
  for (uint8_t i = 0; i < count; i++) {
    WidgetType wType = page.widgets[i];
    const Rect& r = slotDef.slots[i].rect;
    if (x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h) {
      if (handleWidgetTouch(wType, r, x, y)) {
        return true;
      }
    }
  }
  return false;
}
```

- [ ] **Step 3: Build and flash**

Run: `pio run -t upload`
Expected: `SUCCESS`.

- [ ] **Step 4: Visually verify**

Swipe through all pages. Confirm the status bar (page title, GPS status, ride-state/battery) renders in the muted-gray/white Minimal palette without any leftover text (the `setTextPadding()` calls from the earlier ghosting fix are preserved here), and the action button still starts/pauses a ride on tap.

- [ ] **Step 5: Commit**

```bash
git add src/ui/engine/layout_manager.h src/ui/engine/layout_manager.cpp
git commit -m "feat(ui): restyle status bar and page chrome to Minimal palette, wire size-class dispatch"
```

---

### Task 12: Layout config — trimmed defaults, schema version, NVS migration safety

**Files:**
- Modify: `src/storage/layout_config.cpp`

**Interfaces:**
- Consumes: `isUiConfigValid()` from `ui_config.h` (Task 3); trimmed `WidgetType` values from `widget_types.h` (Task 1).
- Produces: `resetLayoutToDefaults()` / `initLayoutConfig()` / `saveLayoutConfig()` (signatures unchanged) now building/validating a 4-page default set instead of 5, with `schema_version` stamped.

- [ ] **Step 1: Update `resetLayoutToDefaults()` and `initLayoutConfig()`**

In `src/storage/layout_config.cpp`, replace `resetLayoutToDefaults()`:

```cpp
void resetLayoutToDefaults() {
  g_ui_config.schema_version = UI_CONFIG_SCHEMA_VERSION;
  g_ui_config.active_page_count = 4;

  // Page 0: Ride (Hero 6-Grid)
  snprintf(g_ui_config.pages[0].title, sizeof(g_ui_config.pages[0].title), "ride");
  g_ui_config.pages[0].template_id = TEMPLATE_HERO_6_GRID;
  g_ui_config.pages[0].widget_count = 6;
  g_ui_config.pages[0].widgets[0] = WIDGET_SPEED;
  g_ui_config.pages[0].widgets[1] = WIDGET_DISTANCE;
  g_ui_config.pages[0].widgets[2] = WIDGET_RIDE_TIME;
  g_ui_config.pages[0].widgets[3] = WIDGET_CADENCE;
  g_ui_config.pages[0].widgets[4] = WIDGET_HEART_RATE;
  g_ui_config.pages[0].widgets[5] = WIDGET_POWER;

  // Page 1: Climb (2-Grid + Chart)
  snprintf(g_ui_config.pages[1].title, sizeof(g_ui_config.pages[1].title), "climb");
  g_ui_config.pages[1].template_id = TEMPLATE_2_GRID_CHART;
  g_ui_config.pages[1].widget_count = 3;
  g_ui_config.pages[1].widgets[0] = WIDGET_ALTITUDE;
  g_ui_config.pages[1].widgets[1] = WIDGET_GRADE;
  g_ui_config.pages[1].widgets[2] = WIDGET_ELEVATION_CHART;

  // Page 2: Sensors (Full Container)
  snprintf(g_ui_config.pages[2].title, sizeof(g_ui_config.pages[2].title), "sensors");
  g_ui_config.pages[2].template_id = TEMPLATE_FULL_CONTAINER;
  g_ui_config.pages[2].widget_count = 1;
  g_ui_config.pages[2].widgets[0] = WIDGET_BLE_MANAGER;

  // Page 3: Settings (Full Container)
  snprintf(g_ui_config.pages[3].title, sizeof(g_ui_config.pages[3].title), "settings");
  g_ui_config.pages[3].template_id = TEMPLATE_FULL_CONTAINER;
  g_ui_config.pages[3].widget_count = 1;
  g_ui_config.pages[3].widgets[0] = WIDGET_SETTINGS_LIST;

  Serial.printf("[LAYOUT CONFIG] Loaded Factory Default Page Tree (4 pages, schema v%u).\n", UI_CONFIG_SCHEMA_VERSION);
}
```

Replace `initLayoutConfig()`:

```cpp
void initLayoutConfig() {
  initWidgetRegistry();

  uiPrefs.begin("opencyclo_ui", false);
  size_t readBytes = uiPrefs.getBytes("config", &g_ui_config, sizeof(UiConfig));
  uiPrefs.end();

  if (!isUiConfigValid(g_ui_config, readBytes)) {
    Serial.println("[LAYOUT CONFIG] NVS layout missing, invalid, or from an older schema — resetting to defaults.");
    resetLayoutToDefaults();
    saveLayoutConfig();
  } else {
    Serial.printf("[LAYOUT CONFIG] Loaded %u custom pages from NVS Flash (schema v%u).\n",
                  g_ui_config.active_page_count, g_ui_config.schema_version);
  }
}
```

- [ ] **Step 2: Build and flash**

Run: `pio run -t upload`
Expected: `SUCCESS`.

- [ ] **Step 3: Verify the migration actually fires on this specific board**

This is the direct regression test for the bug that started this rebuild — the board's NVS currently holds pre-rebuild data (old schema, old `WidgetType` values, no `schema_version` field).

Run: `pio device monitor` and reset the board (or just watch the boot log from the upload in Step 2).
Expected: `[LAYOUT CONFIG] NVS layout missing, invalid, or from an older schema — resetting to defaults.` — confirming the stale data was detected and replaced, not silently reinterpreted.

- [ ] **Step 4: Visually verify the full default page set**

Swipe through all 4 pages (ride / climb / sensors / settings). Confirm every tile shows the widget its label says it is (no more "AVG SPD" appearing where "DIST (KM)" should be) and nothing overlaps.

- [ ] **Step 5: Commit**

```bash
git add src/storage/layout_config.cpp
git commit -m "fix(ui): use schema-version-checked validation, trim defaults to 4 pages"
```

---

### Task 13: Remove dead code from the pre-rebuild widget engine

**Files:**
- Delete: `src/ui/pages/ride_page.cpp`
- Delete: `src/ui/pages/ride_page.h`
- Delete: `src/ui/pages/climb_page.cpp`
- Delete: `src/ui/pages/climb_page.h`
- Delete: `src/ui/pages/debug_page.cpp`
- Delete: `src/ui/pages/debug_page.h`
- Delete: `src/ui/pages/sensors_page.cpp`
- Delete: `src/ui/pages/sensors_page.h`
- Delete: `src/ui/pages/settings_page.cpp`
- Delete: `src/ui/pages/settings_page.h`

**Interfaces:** None — these files are not `#include`d or called from anywhere in `src/main.cpp`, `ui_task.cpp`, or the widget engine (confirmed during the original bug investigation: `renderRidePage` is defined but never called). Deleting them removes confusion for the next person reading the codebase, not behavior.

- [ ] **Step 1: Confirm these files are truly unreferenced before deleting**

Run: `grep -rn "ride_page.h\|climb_page.h\|debug_page.h\|sensors_page.h\|settings_page.h" src/ --include=*.cpp --include=*.h | grep -v "src/ui/pages/"`
Expected: no output (nothing outside `src/ui/pages/` includes them).

- [ ] **Step 2: Delete the files**

```bash
git rm src/ui/pages/ride_page.cpp src/ui/pages/ride_page.h \
       src/ui/pages/climb_page.cpp src/ui/pages/climb_page.h \
       src/ui/pages/debug_page.cpp src/ui/pages/debug_page.h \
       src/ui/pages/sensors_page.cpp src/ui/pages/sensors_page.h \
       src/ui/pages/settings_page.cpp src/ui/pages/settings_page.h
```

- [ ] **Step 3: Build to confirm nothing broke**

Run: `pio run`
Expected: `SUCCESS` — confirms Step 1's grep was right and nothing depended on these files.

- [ ] **Step 4: Commit**

```bash
git commit -m "chore(ui): remove dead pre-rebuild page files superseded by the widget engine"
```

---

## Summary

After Task 13: 15-widget catalog with declared size support, every template slot tagged with its `SizeClass`, `UiConfig` schema-versioned so stale NVS data can never again be silently misinterpreted, full Minimal-style visual rebuild across all widgets and page chrome, and the dead pre-rebuild UI files removed. Task 12, Step 3 is the direct on-hardware regression test proving the original bug report (stale/garbled widget rendering) is actually fixed, not just theoretically addressed.

**Not in this plan:** updating the Flutter companion app's layout editor to the new schema (spec's Build Order step 3) — separate codebase, separate follow-up plan, only worth doing once this firmware side is confirmed solid on hardware.
