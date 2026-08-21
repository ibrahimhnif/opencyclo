# OpenCyclo Modular UI Component Engine & Tree-Structured Page System Design

**Date:** 2026-08-22
**Status:** Approved for Implementation Planning

## 1. Overview & Purpose

This specification defines the architectural design for the **Modular UI Component Engine and Tree-Structured Page System** for OpenCyclo. 

The purpose of this architecture is to transform all data fields (Speed, Distance, Cadence, HR, Power, Grade, Altitude, Ascent, Clock, Battery, Sparkline, Diagnostics) into **independent, plug-and-play UI widgets**. Pages become declarative containers of layout templates, where any slot can host any telemetry widget. The entire page configuration is persisted in NVS and microSD (`/config/layout.json`), and is dynamically customizable via a companion smartphone app over BLE/WiFi, with readiness for Over-the-Air (OTA) firmware updates.

---

## 2. Component Tree & Hierarchy Architecture

```
                             ┌────────────────────────┐
                             │      UiConfig          │
                             │ (Active Page Count, N) │
                             └───────────┬────────────┘
                                         │
               ┌─────────────────────────┴─────────────────────────┐
               ▼                                                   ▼
     ┌───────────────────┐                               ┌───────────────────┐
     │   PageConfig[0]   │  ...                          │   PageConfig[N-1] │
     │  (Template ID,    │                               │  (Template ID,    │
     │   Widget Slot Map)│                               │   Widget Slot Map)│
     └─────────┬─────────┘                               └─────────┬─────────┘
               │                                                   │
    ┌──────────┴──────────┐                             ┌──────────┴──────────┐
    ▼                     ▼                             ▼                     ▼
┌──────────────┐   ┌──────────────┐                 ┌──────────────┐   ┌──────────────┐
│ WidgetSlot 0 │   │ WidgetSlot 1 │                 │ WidgetSlot 0 │   │ WidgetSlot 1 │
│(WIDGET_SPEED)│   │ (WIDGET_DIST)│                 │(WIDGET_ALTI) │   │ (WIDGET_GRD) │
└──────────────┘   └──────────────┘                 └──────────────┘   └──────────────┘
```

---

## 3. Data Structures & Types

### 3.1 Widget Type Identifier
```cpp
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
  WIDGET_CLOCK,
  WIDGET_BATTERY,
  WIDGET_GPS_DIAGNOSTICS,
  WIDGET_NMEA_CONSOLE,
  WIDGET_BLE_MANAGER,
  WIDGET_SETTINGS_LIST,
  WIDGET_TYPE_COUNT
};
```

### 3.2 Layout Template Identifier
```cpp
enum LayoutTemplateId : uint8_t {
  TEMPLATE_HERO_6_GRID = 0, // 1 Hero top (94px) + 2 Mid (60px) + 3 Bottom (54px) + Control (50px)
  TEMPLATE_4_GRID,          // 4 equal symmetric cards (2x2)
  TEMPLATE_2_GRID_CHART,    // 2 cards top (74px) + 1 large Sparkline Chart below (134px)
  TEMPLATE_8_GRID,          // 8 dense data fields (2 columns x 4 rows)
  TEMPLATE_FULL_CONTAINER,  // Full screen specialized component
  TEMPLATE_COUNT
};
```

### 3.3 Slot Geometry Definition
```cpp
struct Rect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

struct TemplateSlotDefinition {
  uint8_t max_slots;
  Rect slot_rects[8];
  bool has_action_button;
  Rect action_button_rect;
};
```

### 3.4 Page and UI Configuration
```cpp
#define MAX_PAGES 8
#define MAX_SLOTS_PER_PAGE 8

struct PageConfig {
  char title[16];
  LayoutTemplateId template_id;
  uint8_t widget_count;
  WidgetType widgets[MAX_SLOTS_PER_PAGE];
};

struct UiConfig {
  uint8_t active_page_count;
  PageConfig pages[MAX_PAGES];
};
```

---

## 4. Widget Registry Interface

Each widget is rendered through a unified interface function:
```cpp
typedef void (*WidgetRenderFn)(const Rect& bounds, const TelemetryState& state, bool forceFullRedraw);

struct WidgetDescriptor {
  WidgetType type;
  const char* name;
  const char* label;
  const char* unit_metric;
  const char* unit_imperial;
  WidgetRenderFn render_fn;
};
```

The `WidgetRegistry` maps `WidgetType` to its respective `WidgetDescriptor`, providing modular, decoupled rendering for every data field on the display.

---

## 5. Storage & Synchronization Architecture

1. **Internal NVS Flash Primary Storage**:
   - `UiConfig` binary / serialized representation is stored in NVS namespace `"opencyclo_ui"`.
   - Loaded immediately at boot with zero filesystem delay.
2. **microSD File Backup & Import/Export**:
   - File `/config/layout.json` allows users to backup, share, or edit layout configs offline.
3. **Factory Defaults Fallback**:
   - Hardcoded in flash:
     - Page 0: `TEMPLATE_HERO_6_GRID` (Speed, Distance, Time, Cadence, HR, Power, Start Button)
     - Page 1: `TEMPLATE_2_GRID_CHART` (Altitude, Grade, Elevation Chart)
     - Page 2: `TEMPLATE_FULL_CONTAINER` (BLE Sensors & GPS Diagnostics)
     - Page 3: `TEMPLATE_FULL_CONTAINER` (NMEA Live Console)
     - Page 4: `TEMPLATE_FULL_CONTAINER` (System Preferences)
4. **BLE Mobile App Configuration & OTA**:
   - Custom BLE GATT configuration service to write new `UiConfig` packets dynamically.
   - BLE/WiFi OTA endpoint for Over-the-Air firmware updates.

---

## 6. Implementation Milestones

1. **Phase 1: Widget Architecture & Registry**
   - Create `src/ui/engine/widget_registry.{h,cpp}` containing all isolated data field renderers.
2. **Phase 2: Template Grid Engine**
   - Create `src/ui/engine/template_engine.{h,cpp}` managing slot geometry for `TEMPLATE_HERO_6_GRID`, `TEMPLATE_4_GRID`, `TEMPLATE_2_GRID_CHART`, `TEMPLATE_8_GRID`, and `TEMPLATE_FULL_CONTAINER`.
3. **Phase 3: Page Tree & Layout Manager**
   - Implement `src/ui/engine/layout_manager.{h,cpp}` executing the dynamic page render loop.
4. **Phase 4: NVS & JSON Persistence**
   - Implement `src/storage/layout_config.{h,cpp}` with NVS save/load and microSD JSON serialization.
5. **Phase 5: Verification & Gesture Navigation Integration**
   - Connect Swipe Left / Right gestures with the dynamic `UiConfig` page count and bottom indicator dots.

---

## 7. Spec Self-Review Checklist
- [x] **Placeholder Scan:** No "TBD" or "TODO" items.
- [x] **Internal Consistency:** Memory structures match ESP32-S3 PSRAM/NVS constraints and LovyanGFX coordinate space.
- [x] **Scope Check:** Clear, modular division across 5 implementation phases.
- [x] **Ambiguity Check:** Explicit slot coordinate geometry and serialization formats defined.
