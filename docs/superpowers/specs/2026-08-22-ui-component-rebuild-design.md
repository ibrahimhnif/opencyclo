# OpenCyclo UI Component Rebuild Design

**Date:** 2026-08-22
**Status:** Approved for implementation planning
**Supersedes (in part):** `docs/superpowers/specs/2026-08-22-modular-ui-component-engine-design.md` — this doc keeps that spec's overall concept (a tree of pages built from reusable widgets, persisted config, phone-editable) but replaces its widget/slot contract and NVS handling to fix bugs found during hardware bring-up.

## Why this rebuild

Bringing the existing Modular UI Component Engine up on real hardware surfaced two bugs, both structural rather than one-off mistakes:

1. **Widgets assume their slot size.** Every widget's `render_fn(const Rect& b, ...)` computes its internal layout as offsets from `b`, but nothing constrains *which* `Rect` a widget can be assigned to. `renderWidgetSpeed`, for example, lays out a label + badge + large number + unit across ~94px of height assuming the 232×94 hero slot; assigned to a 74×54 slot, its own elements overlap each other. This is what produced the garbled "[GPS] / KM/H" boxes on device.
2. **`UiConfig` persists to NVS flash, which survives normal reflashes**, and the staleness guard only checks `sizeof(UiConfig)` — not schema identity. If the `WidgetType` enum's ordering changes between firmware builds (as it did during development), an old saved byte value gets silently reinterpreted as a *different* widget under the new enum. This is why a device showed `WIDGET_AVG_SPEED` in a slot the current source assigns `WIDGET_DISTANCE` to.

Both are fixed by construction below, not by patching individual symptoms.

## Visual direction

Chosen from mockup comparison: **"Garmin/Wahoo Minimal."**

- Background: near-black (`#000000` panel background, not the previous navy `#0a0e18`)
- No filled/bordered card backgrounds on tiles — a single 1px hairline (`#1c1c1c`) above each tile replaces the rounded-rect-with-border treatment
- Numerals: light-weight, larger; labels: small, lowercase, muted gray (`#666666`) instead of uppercase muted-blue labels
- Typography: switch from the default LovyanGFX bitmap font to the bundled **FreeSans** vector font family (`fonts::FreeSans9pt7b` / `12pt7b` / `18pt7b` / `24pt7b`, confirmed present in the vendored LovyanGFX under `lgfx/Fonts/GFXFF/`) for a non-blocky, proportional look. Regular weight for most text; bold reserved only if a specific element needs more visual weight after seeing it on-device. Exact point size per size-class (below) is chosen during component implementation by checking actual rendered dimensions on the physical panel — LovyanGFX's point sizes don't map 1:1 to pixel height the way the old fixed-multiple bitmap font did.
- Status bar: satellite fix shown as a dot/bar indicator rather than "GPS 3D (N)" text where practical, to reduce text-length variability (also incidentally reduces exposure to the text-erasure-width issue fixed separately in the ghosting bugfix commit)

## Component contract

```cpp
enum SizeClass : uint8_t {
  SIZE_SMALL,   // 74×54  — bottom-row tiles (Cadence, HR, Power, Battery)
  SIZE_MEDIUM,  // 114×60 — mid-row tiles (Distance, Ride Time, Avg/Max Speed)
  SIZE_LARGE,   // 232×192 — the chart slot in TEMPLATE_2_GRID_CHART (Elevation Chart)
  SIZE_HERO,    // 232×94 — the one big headline tile (Speed)
  SIZE_FULL     // 232×274 — whole-page management screens (BLE Manager, Settings List)
};
```

Each widget descriptor gains a `supported_sizes` bitmask. A template's slot declares its `SizeClass`. The page/slot assignment (in the phone app editor, and in `resetLayoutToDefaults()`) is only valid if `widget.supported_sizes` includes the slot's class. `renderWidget()` checks this at render time too — defense in depth against any config path (import, NVS, factory default) that manages to produce an invalid combination: an unsupported assignment renders a blank placeholder tile instead of attempting to draw and overlapping itself.

Trimmed widget catalog (15, from the original 18 — cut `Clock` [never wired to a real clock], `GPS Diagnostics` [duplicates the GPS block already in BLE Manager], `NMEA Console` [firmware-debug tool, not a rider-facing v1 feature]):

| Widget | Supported sizes |
|---|---|
| Speed | HERO |
| Avg Speed, Max Speed | SMALL, MEDIUM |
| Distance, Ride Time | MEDIUM |
| Cadence, Heart Rate, Power, Battery | SMALL |
| Altitude, Grade, Total Ascent | SMALL, MEDIUM |
| Elevation Chart | LARGE, FULL |
| BLE Manager, Settings List | FULL only |

## NVS schema versioning

```cpp
#define UI_CONFIG_SCHEMA_VERSION 1  // bump whenever WidgetType, SizeClass, or
                                     // the template table changes in a way that
                                     // would change the meaning of already-saved bytes
struct UiConfig {
  uint32_t schema_version;
  uint8_t active_page_count;
  PageConfig pages[MAX_PAGES];
};
```

`initLayoutConfig()` checks `g_ui_config.schema_version == UI_CONFIG_SCHEMA_VERSION` in addition to the existing size check, before trusting NVS data. A mismatch on either check resets to `resetLayoutToDefaults()` and re-saves — exactly the existing fallback behavior, just triggered by the right condition. `resetLayoutToDefaults()` stamps the current `UI_CONFIG_SCHEMA_VERSION` when it builds the default config.

## Templates

Kept as-is conceptually (named layouts made of typed slots — Hero+Grid, N-Grid, Full Container, etc.), with one change: `TemplateSlotDefinition`'s `slot_rects[]` entries carry a `SizeClass` alongside their pixel `Rect`, so the component contract above has something concrete to validate against per slot.

## Customization

**Phone app only for v1** (reusing the already-spec'd Flutter companion app's drag-and-drop layout editor over BLE — see `2026-08-22-ble-companion-app-and-ota-design.md`). The app's editor is updated to only offer widgets valid for a given slot's `SizeClass`, and to write/read the versioned `UiConfig` format. On-device stays view-only (swipe between pages, tap the existing action buttons) — no on-device drag-and-drop in this rebuild.

## Build order

Matches the requested sequence: components first, proven individually, before composition.

1. **Components**: define `SizeClass` + the trimmed 15-widget catalog with `supported_sizes`; rebuild each widget's render function for its declared size(s) using the new FreeSans-based typography; verify each renders correctly in isolation (a simple one-widget-per-slot test page covering every size class) before moving on.
2. **Pages**: rebuild the template/slot system with typed slots, wire `resetLayoutToDefaults()` to the trimmed catalog and versioned schema, verify the full default 4-page set (Ride, Climb, Sensors/BLE, Settings) renders correctly and swipes cleanly.
3. **Customization**: update the Flutter app's layout editor to the versioned schema and size-class-aware widget picker; verify a layout edited on the phone round-trips correctly to the device over BLE.

## Testing

Widget-level and template-level rendering can't be meaningfully unit-tested off-device (this is direct framebuffer drawing) — verification for steps 1-2 is visual, on the physical panel, one size class and one full page at a time, following the build order above. The `SizeClass` validation logic itself (does a given widget/slot pairing pass or fail) is pure logic and gets a native-environment unit test. The NVS schema-version check is also pure logic and gets a native unit test (old-version bytes → reset triggers; current-version bytes → data trusted).
