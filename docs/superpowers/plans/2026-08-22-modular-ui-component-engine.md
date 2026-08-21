# Modular UI Component Engine & Tree-Structured Page System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a modular, tree-structured UI component engine where telemetry data fields are decoupled widgets rendered into template grid slots, persisted in NVS/SD, and dynamically customizable.

**Architecture:** A `WidgetRegistry` renders independent telemetry data widgets by ID. A `TemplateEngine` defines slot geometry across standard grid presets (`HERO_6_GRID`, `4_GRID`, `2_GRID_CHART`, `8_GRID`, `FULL_CONTAINER`). A `LayoutManager` orchestrates dynamic page rendering from an in-memory `UiConfig` tree loaded from NVS and microSD `/config/layout.json`.

**Tech Stack:** C++17, ESP32-S3, Arduino framework, LovyanGFX, FreeRTOS, Preferences (NVS), SD_MMC.

**Spec:** [docs/superpowers/specs/2026-08-22-modular-ui-component-engine-design.md](file:///Users/hanif/Labs/personal/opencyclo/docs/superpowers/specs/2026-08-22-modular-ui-component-engine-design.md)

## Global Constraints
- Target hardware: ESP32-S3 N16R8 (16MB Flash, 8MB OPI PSRAM), LovyanGFX ILI9341 SPI (240x320, 180° rotation).
- Concurrency model: Core 1 `UiTask` renders UI from `TelemetryState` snapshots; never block FreeRTOS tasks.
- Touch controller: FT6336G on I2C (SDA 16, SCL 15) protected by `g_i2c_mutex`.
- Color scheme: High-contrast Garmin dark slate (`COLOR_BG`, `COLOR_CARD`, `COLOR_CYAN`, `COLOR_GREEN`, `COLOR_AMBER`, `COLOR_RED`, `COLOR_TEXT_MUT`).

---

### Task 1: Core Widget Types & Registry

**Files:**
- Create: `src/ui/engine/widget_types.h`
- Create: `src/ui/engine/widget_registry.h`
- Create: `src/ui/engine/widget_registry.cpp`

**Interfaces:**
- Produces: `enum WidgetType`, `struct Rect`, `struct WidgetDescriptor`, `void initWidgetRegistry()`, `void renderWidget(WidgetType type, const Rect& bounds, const TelemetryState& state, bool forceFullRedraw)`
- Consumes: `src/core/telemetry_state.h`, `src/hardware/display.h`, `src/storage/settings.h`

- [ ] **Step 1: Create `src/ui/engine/widget_types.h`**
Define `WidgetType` enum and `Rect` struct representing UI geometry coordinates.

- [ ] **Step 2: Create `src/ui/engine/widget_registry.h`**
Declare `WidgetDescriptor` and registry lookup and render functions.

- [ ] **Step 3: Implement `src/ui/engine/widget_registry.cpp`**
Implement isolated render functions for `WIDGET_SPEED`, `WIDGET_AVG_SPEED`, `WIDGET_MAX_SPEED`, `WIDGET_DISTANCE`, `WIDGET_RIDE_TIME`, `WIDGET_CADENCE`, `WIDGET_HEART_RATE`, `WIDGET_POWER`, `WIDGET_ALTITUDE`, `WIDGET_GRADE`, `WIDGET_TOTAL_ASCENT`, `WIDGET_CLOCK`, `WIDGET_BATTERY`.

- [ ] **Step 4: Verify build with PlatformIO**
Run: `pio check` or `pio run` to verify compilation.

- [ ] **Step 5: Commit Task 1**
```bash
git add src/ui/engine/
git commit -m "feat(ui): add core widget types and registry implementation"
```

---

### Task 2: Specialized Full-Screen & Complex Widget Renderers

**Files:**
- Modify: `src/ui/engine/widget_registry.h`
- Modify: `src/ui/engine/widget_registry.cpp`

**Interfaces:**
- Produces: Rendering for `WIDGET_ELEVATION_CHART`, `WIDGET_GPS_DIAGNOSTICS`, `WIDGET_NMEA_CONSOLE`, `WIDGET_BLE_MANAGER`, `WIDGET_SETTINGS_LIST`
- Consumes: `src/hardware/ble_task.h`, `src/hardware/battery.h`, `src/core/telemetry_state.h`

- [ ] **Step 1: Integrate Elevation Sparkline Chart renderer into `widget_registry.cpp`**
Implement dynamic 30-sample polygon elevation sparkline inside assigned bounding box.

- [ ] **Step 2: Integrate GPS Diagnostics and Live NMEA Console renderers**
Implement GPS fix summary and rolling matrix terminal log renderers.

- [ ] **Step 3: Integrate BLE Sensor Manager and Settings List renderers**
Implement sensor list with scan & forget buttons and system settings toggles.

- [ ] **Step 4: Verify compilation**
Run: `pio run`

- [ ] **Step 5: Commit Task 2**
```bash
git add src/ui/engine/widget_registry.cpp
git commit -m "feat(ui): implement specialized widget renderers in registry"
```

---

### Task 3: Layout Template Engine & Slot Geometry

**Files:**
- Create: `src/ui/engine/template_engine.h`
- Create: `src/ui/engine/template_engine.cpp`

**Interfaces:**
- Produces: `enum LayoutTemplateId`, `struct TemplateSlotDefinition`, `const TemplateSlotDefinition& getTemplateDefinition(LayoutTemplateId templateId)`
- Consumes: `src/ui/engine/widget_types.h`

- [ ] **Step 1: Create `src/ui/engine/template_engine.h`**
Declare `LayoutTemplateId` and slot definition getters.

- [ ] **Step 2: Implement `src/ui/engine/template_engine.cpp`**
Define exact coordinate arrays for:
- `TEMPLATE_HERO_6_GRID`: Slot 0 (Hero, y:28..122), Slots 1-2 (Mid, y:126..186), Slots 3-5 (Bottom, y:190..244), Action Button (y:248..298).
- `TEMPLATE_4_GRID`: 4 equal-sized cards (y:28..158, y:166..298).
- `TEMPLATE_2_GRID_CHART`: 2 cards top (y:28..102), Chart (y:108..298).
- `TEMPLATE_8_GRID`: 8 cards (2 columns x 4 rows).
- `TEMPLATE_FULL_CONTAINER`: Single card (y:28..302).

- [ ] **Step 3: Verify compilation**
Run: `pio run`

- [ ] **Step 4: Commit Task 3**
```bash
git add src/ui/engine/template_engine.*
git commit -m "feat(ui): implement Layout Template Engine and slot geometry"
```

---

### Task 4: Layout Manager & Page Tree Renderer

**Files:**
- Create: `src/ui/engine/layout_manager.h`
- Create: `src/ui/engine/layout_manager.cpp`

**Interfaces:**
- Produces: `struct PageConfig`, `struct UiConfig`, `void renderPage(const PageConfig& page, const TelemetryState& state, bool forceFullRedraw)`, `bool handlePageTouch(const PageConfig& page, int16_t x, int16_t y)`
- Consumes: `src/ui/engine/template_engine.h`, `src/ui/engine/widget_registry.h`, `src/core/telemetry_state.h`

- [ ] **Step 1: Create `src/ui/engine/layout_manager.h`**
Define `PageConfig` and `UiConfig` tree structures and rendering interface.

- [ ] **Step 2: Implement `src/ui/engine/layout_manager.cpp`**
Implement page rendering loop that fetches slot definitions from `TemplateEngine` and invokes `WidgetRegistry::renderWidget` for each assigned slot.

- [ ] **Step 3: Implement unified touch delegator `handlePageTouch()`**
Route touch events to interactive widgets (Start button, BLE Scan, Settings toggles).

- [ ] **Step 4: Verify compilation**
Run: `pio run`

- [ ] **Step 5: Commit Task 4**
```bash
git add src/ui/engine/layout_manager.*
git commit -m "feat(ui): implement Layout Manager and dynamic page tree renderer"
```

---

### Task 5: Configuration Persistence (NVS & SD Card JSON)

**Files:**
- Create: `src/storage/layout_config.h`
- Create: `src/storage/layout_config.cpp`

**Interfaces:**
- Produces: `extern UiConfig g_ui_config`, `void initLayoutConfig()`, `void saveLayoutConfig()`, `void resetLayoutToDefaults()`, `bool exportLayoutToJson(const char* filepath)`, `bool importLayoutFromJson(const char* filepath)`
- Consumes: `src/ui/engine/layout_manager.h`, `Preferences.h`, `SD_MMC.h`

- [ ] **Step 1: Create `src/storage/layout_config.h`**
Declare layout storage functions and global `g_ui_config`.

- [ ] **Step 2: Implement `src/storage/layout_config.cpp`**
Implement factory default layout tree, NVS read/write binary serialization, and SD Card `/config/layout.json` exporter.

- [ ] **Step 3: Verify compilation**
Run: `pio run`

- [ ] **Step 4: Commit Task 5**
```bash
git add src/storage/layout_config.*
git commit -m "feat(storage): implement NVS and SD layout configuration persistence"
```

---

### Task 6: Wire into `UiTask` and Touch Gesture Navigation

**Files:**
- Modify: `src/ui/ui_task.h`
- Modify: `src/ui/ui_task.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Produces: Dynamic multi-page navigation adapting to `g_ui_config.active_page_count` with Swipe Left / Right gestures and dynamic bottom page indicator dots.
- Consumes: `src/ui/engine/layout_manager.h`, `src/storage/layout_config.h`

- [ ] **Step 1: Initialize layout configuration in `src/main.cpp`**
Call `initLayoutConfig()` before launching `startUiTask()`.

- [ ] **Step 2: Update `src/ui/ui_task.cpp`**
Replace hardcoded page switch statements with dynamic `LayoutManager::renderPage(g_ui_config.pages[currentPageIdx], state, forceRedraw)`.

- [ ] **Step 3: Update bottom page indicator dots**
Dynamically draw `g_ui_config.active_page_count` dots at bottom center (`y: 310`).

- [ ] **Step 4: Verify compilation**
Run: `pio run`

- [ ] **Step 5: Commit Task 6**
```bash
git add src/ui/ui_task.cpp src/main.cpp
git commit -m "feat(ui): integrate LayoutManager and dynamic page tree into UiTask"
```

---

### Task 7: Full System Build, Flash & Hardware Verification

**Files:**
- Build system verification across all modules.

- [ ] **Step 1: Execute full compilation and upload**
Run: `pio run -t upload --upload-port /dev/cu.usbmodem21242101`
Verify 100% upload success.

- [ ] **Step 2: Verify touch gestures and rendering**
Test Swipe Left and Swipe Right page transitions across all 5 pages.

- [ ] **Step 3: Verify metrics & buttons**
Verify start/pause button, BLE scanning, and settings adjustments on hardware.

- [ ] **Step 4: Final commit & documentation update**
```bash
git commit -m "feat(ui): complete Modular UI Component Engine and Tree-Structured Page System"
```
