# Firmware UI: Components, Theme Engine, and UI/Logic Split — Design

Date: 2026-10-26. Scope: the ESP32 firmware UI (`src/ui/`, plus the touch/draw
parts of screens elsewhere). The Flutter app in `app/` is not changed by this
project.

## Intent

Today the firmware UI is hard to restyle and hard to change safely, because
drawing, text formatting, touch handling, and real side effects live in the
same functions. `src/ui/engine/widget_registry.cpp` (710 lines) is the clearest
case: the settings tile writes `g_settings` and calls `saveSettings()` itself,
the sensor manager calls `forgetSensorProfile()` / `requestBaroCalibration()`,
and every draw function has its own hardcoded fonts and offsets
(`b.y + 24`, `b.w - 70`). Menus (`power_menu.cpp`, `ride_menu.cpp`) each
hand-draw their own buttons.

The owner wants:

1. **Reusable UI building blocks in their own files** — `button.h/.cpp`,
   label, tile, header, and so on — each drawing itself from a style, so a
   button's look is changed in one place and every button follows.
2. **A theme engine** — one theme owns all visual decisions (colors, fonts,
   spacing, radius, named component styles).
3. **Full pages composed from those blocks** — a page/frame is a list of
   components with positions, not hand-written draw code.
4. **Customizable both in C++ and from outside** — built-in pages and themes
   are written in C++; the web companion (`web/`) and the phone app can push
   custom themes *and* custom page layouts.
5. **UI separated from logic** — components and pages never call settings,
   BLE, ride, or camera code directly.

## Goals

- A component library under `src/ui/components/`, one component per `.h/.cpp`
  pair, all with the same small contract (below).
- Theme v2: colors (existing tokens) + typography + spacing + named
  component styles, loadable at runtime, with the compiled default as
  fallback.
- A page document model shared by built-in C++ pages and pushed custom pages.
- A fixed, firmware-defined **action registry** and **binding registry**: the
  only bridge between UI and logic.
- Every existing non-map screen rebuilt on components: dashboard tiles,
  page header, ride menu, power menu, settings list, camera remote, sensor /
  BLE manager, charging screen.
- Map screen buttons use the shared button component.
- Web companion can edit page layouts and component styles, with a preview,
  and push them over Web Serial. The same documents are accepted over the
  existing BLE layout sync so the app can use them later.

## Non-goals

- Rewriting the map renderer or splitting `navigation.cpp`'s route transfer /
  map drawing. Only its buttons move to shared components. (Follow-up project.)
- Adding new fonts beyond the GFX fonts already compiled in.
- Animations or transitions.
- A layout editor inside the Flutter app (the firmware protocol supports it;
  the app UI is a separate project).
- Letting pushed layouts define new behavior. They can only arrange
  components and choose from firmware-defined actions and bindings.

## Anti-goals (would be a failure even if it works)

These are my calls; the owner did not state them explicitly.

- **The default look changes.** With no custom theme/page, the dashboard and
  menus must render as they do today (pixel-identical for the dashboard
  tiles/hero/header; ride and power menus identical or visually
  indistinguishable — any deliberate difference must be called out in the PR).
- **The screen gets slower.** The UI loop is ~50 ms today (≈20 ms blit +
  15 ms delay + draw). No measurable regression in frame period.
- **A bad theme or page can break the screen.** Every pushed document is
  staged and validated; rejection changes nothing. A page that passes
  validation but is unusable must still be escapable (see "Safety").
- **Heap allocation in the render/touch path.** All page/theme storage uses
  fixed-capacity structs, like `UiConfig` does today.
- **A component reaches into globals** (`g_settings`, `g_theme`, telemetry,
  BLE). Components get everything as arguments.

## Constraints

- ESP32-S3 N16R8 (8 MB PSRAM, 16 MB flash), Arduino + FreeRTOS, LovyanGFX.
  Display 240×320 RGB565; all drawing goes to the off-screen `canvas` sprite
  then `presentFrame()`. (`power_menu.cpp` currently draws straight to `tft`;
  it moves to `canvas` like everything else.)
- UI task stack is 8192 bytes; large structs live in static storage / PSRAM,
  never on the stack.
- Host tests compile with `-std=c++14` (`[env:native]` and the
  `tests/run_*.py` runners using fakes such as `tests/widget_fakes.h`).
  Components, theme, page validation, view models, and action dispatch must
  all be host-testable.
- No JSON library is currently a dependency; `theme.cpp` hand-parses a flat
  object. Page documents are nested (arrays of nodes). **My call:** add
  ArduinoJson 7 for theme v2 and page documents (to `lib_deps` and the native
  test build). The builder may keep a hand parser instead if it stays small
  and fully tested.
- Builds on the current uncommitted work (`theme.*`, `widget_model.*`,
  `theme_config.*`, `web_serial.*`, `web/`, camera control). That work must be
  committed before this project starts so the refactor has a clean base.

## Architecture

Four layers. Arrows mean "may include/call".

```
 logic (ride session, settings, BLE, camera, sensors)
     ▲                          │
     │ actions.cpp dispatch      │ snapshots / state
     │                          ▼
 screens/*_view   ── pure: state → ViewModel (text, enabled, which action)
     ▲
     │ bindings (name → view-model field)
 page engine      ── PageDoc + theme → draws components, maps taps to ActionId
     ▲
 components/*     ── pure drawing + hit rect, from props + resolved style
```

### 1. Components (`src/ui/components/`)

Initial set (each its own `.h/.cpp`):

| Component     | Replaces / used by |
|---------------|--------------------|
| `button`      | ride menu, power menu, camera, map controls, page action button |
| `icon_button` | header map/ride buttons, back buttons |
| `label`       | free text, status lines, titles |
| `value_tile`  | `renderTile` (label above value, hairline) |
| `hero_value`  | `renderWidgetSpeed` |
| `header`      | `layout_manager.cpp` page header |
| `list`        | settings rows, sensor list, camera mode picker (scrolling via existing `ui::ScrollGesture`) |
| `toggle`      | settings on/off rows |
| `chart`       | elevation chart |
| `battery`     | battery tile/indicator |

Contract, same shape for every component:

```cpp
struct ButtonProps {
  const char* text;
  ui::Icon icon;      // Icon::None allowed
  StyleId style;      // e.g. STYLE_BUTTON_PRIMARY
  bool pressed;       // drawn pressed while held
  bool enabled;
};
void drawButton(Gfx& gfx, const Rect& r, const ButtonProps& p,
                const ResolvedTheme& t);
```

`Gfx` is a type alias in one header: `lgfx::LovyanGFX` on device, the
recording fake canvas on host (the same header-substitution trick the
`tests/run_*.py` runners already use for `hardware/display.h`). No templates,
no extra virtual layer.

- Draws into the given target and rect only. No globals, no side effects.
- Hit testing is the page engine's job (rect containment), so components have
  no touch code. Press/cancel/release behavior is one shared `PressTracker`
  (generalized from `rideui::Touch` in `ride_view.h`: 6 px slop, cancel on
  leaving the target, release fires only if still on the original target).
- Partial redraw: components support the current "force full redraw vs
  update text only" pattern (text padding erase) so frame cost doesn't rise.

### 2. Theme engine (`src/ui/engine/theme.*`, `src/storage/theme_config.*`)

Theme v2 extends the existing `UiTheme`:

- **Color tokens** — the existing nine (`bg`, `panel`, `text`, `label`,
  `hairline`, `accent`, `success`, `warning`, `danger`).
- **Font roles** — `caption`, `label`, `value`, `title`, `hero`, each mapped to
  a font id from a fixed table of the compiled-in GFX fonts. Replaces the
  unused `font_scale_pct`. (Defaults reproduce today's choices:
  label = FreeSansBold9pt7b, value = FreeSansBold12pt7b, hero =
  FreeSansBold24pt7b.)
- **Spacing** — `pad`, `gap`, `radius`, `border`.
- **Component styles** — a fixed list of named styles, each
  `{bg, fg, border color token, border width, radius, font role, pad}`
  referencing tokens by name. Initial list: `button.primary`,
  `button.secondary`, `button.danger`, `button.ghost`, `tile.default`,
  `tile.accent`, `tile.success`, `tile.warning`, `tile.danger`, `hero`,
  `header`, `list.row`, `list.row.selected`, `label.muted`, `label.title`.
  Tile tone variants map from the existing `wmodel::Tone`.

`ResolvedTheme` holds RGB565 colors and font pointers per style, resolved
once when the theme changes (not per frame). The render loop reads it through
an accessor; the web serial / BLE task swaps it in by setting a version
counter the UI task checks at the top of each frame (no torn reads).

Back-compat: a v1 theme document (colors + `font_scale`) is still accepted;
missing v2 fields take defaults.

### 3. Page engine (`src/ui/engine/page_*`)

A page is data:

```cpp
struct PageNode {
  NodeType type;        // BUTTON, LABEL, VALUE_TILE, HERO, LIST, WIDGET, ...
  Rect rect;
  StyleId style;
  char text[24];        // literal text, or empty when bound
  BindingId bind;       // e.g. BIND_RIDE_TITLE, BIND_SPEED; NONE for literal
  ui::Icon icon;
  ActionId action;      // e.g. ACT_RIDE_PAUSE, or a slot action (below)
  VisibilityId visible; // ALWAYS, or a view-provided condition
};
struct PageDoc {
  uint16_t page_id;     // which screen/page this defines
  char title[16];
  uint8_t node_count;
  PageNode nodes[MAX_NODES_PER_PAGE];  // builder picks cap, ~24
};
```

- **Built-in pages** are `constexpr PageDoc` definitions in C++
  (`src/ui/pages/*.cpp`). They use exactly the same struct as pushed pages.
- **Custom pages** pushed from web/app override a built-in by `page_id`, are
  persisted (NVS like `layout_config.cpp`, or SD if size demands — builder's
  call), and can be reset per page or all at once.
- **Dashboard pages** become page docs too. The current templates
  (`TEMPLATE_HERO_6_GRID`, etc.) become built-in docs whose slots are `WIDGET`
  nodes rendering a data widget into a rect. The existing v1 layout JSON
  (`OC+SET_LAYOUT`, BLE layout sync) keeps working by converting
  template + widget list into page docs on import. `widgetSupportsSize`
  stays the rule for which widget fits which rect.
- **Slot actions**: screens whose buttons change with state (the ride menu's
  primary button is Start / Pause / Resume / Retry) use slot actions such as
  `ACT_RIDE_PRIMARY`; the screen's view model resolves the slot to the
  concrete action and label each frame. Bindings like `BIND_RIDE_PRIMARY_LABEL`
  supply the text. This keeps `ride_view.h` as the single source of truth.
- **Visibility** lets one page doc cover a screen's states (e.g. hide the
  back button while saving).

Validation (pure, host-tested) rejects the whole document when: schema
version mismatches, a rect leaves 240×320, any id (type, style, action,
binding, visibility, page) is unknown, a `WIDGET` node's rect doesn't match a
size class the widget supports, node count exceeds the cap, strings overflow,
or the document is truncated.

**Safety:** a page that validates but is unusable must still be escapable.
The page header's back/map/ride hit areas and the hardware power button
behavior are not overridable by custom pages, and a long-press reset (builder
picks gesture) restores built-in pages.

### 4. Screens, views, actions (UI/logic split)

Each stateful screen splits into three parts:

| Part | Example (ride menu) | Rules |
|------|---------------------|-------|
| `*_view` | existing `rideui::view()` | Pure: snapshot → ViewModel. No drawing, no side effects. Host-tested. |
| page doc | built-in ride menu `PageDoc` | Layout only. Customizable. |
| actions | `ACT_RIDE_START` → `requestStartRide()` | The only code that calls logic. |

Screens to split: ride menu, power menu, settings list, camera remote,
sensor/BLE manager (including baro calibration sub-view), charging screen,
dashboard header. Their per-screen state that is pure UI (current camera page,
selected mode, scroll offset) stays in the screen; state that is real logic
(paired sensors, settings values) comes from the view model.

`src/ui/actions.cpp` holds one `dispatch(ActionId)` switch. It is the single
place the UI calls `saveSettings()`, ride session requests, BLE sensor
connect/forget, baro calibration, camera commands, power off, etc.

**Bindings** are a fixed table: `BindingId` → function that writes text from
the current view model / telemetry snapshot into a buffer, using
`widget_model` formatters (so web preview and device format identically).
Remaining widgets (heart rate, power, altitude, grade, ascent, avg/max speed,
battery) move their formatting into `widget_model` as part of this.

### 5. Screen manager and touch routing (`ui_task.cpp`)

Replace the hand-ordered chain in `uiTaskLoop` (power UI → ride menu →
sensor stream → navigation → swipe/tap) with a small screen stack: an overlay
(power menu, ride menu) gets input first; otherwise the active screen (map or
dashboard page). Swipe-between-dashboard-pages stays the dashboard's
behavior. Gesture thresholds (35 px swipe, 70 px vertical tolerance, 20 px tap,
150 ms debounce) move into one named-constants header. Behavior must stay the
same.

## Wire format and protocol

Theme v2 (example, keys are the builder's to finalize):

```json
{"schema":2,
 "colors":{"bg":"#000000","text":"#FFFFFF","accent":"#00D2FF", "...":"..."},
 "fonts":{"label":"sans_bold_9","value":"sans_bold_12","hero":"sans_bold_24"},
 "spacing":{"pad":4,"gap":8,"radius":8,"border":1},
 "styles":{"button.primary":{"bg":"accent","fg":"bg","radius":10,"font":"label"}}}
```

Page document (example):

```json
{"schema":1,"page":"ride_menu","title":"Ride",
 "nodes":[
  {"type":"label","rect":[12,60,216,24],"style":"label.title","bind":"ride.title"},
  {"type":"button","rect":[12,198,216,48],"style":"button.primary",
   "bind":"ride.primary.label","action":"ride.primary"},
  {"type":"button","rect":[12,256,216,48],"style":"button.secondary",
   "bind":"ride.secondary.label","action":"ride.secondary"}]}
```

Web Serial (`OC+…`, one reply line per command, staged + validated like today):

- `OC+HELLO` reply gains a **capabilities catalog**: component types, style
  names, font ids, action names, binding names, visibility names, page ids,
  and caps. The web editor builds its pickers from this, so editor and
  firmware lists can't drift.
- `OC+GET_THEME` / `OC+SET_THEME` accept v2 (and v1).
- New: `OC+GET_PAGE:<id>`, `OC+SET_PAGE:<doc>`, `OC+RESET_PAGE:<id|all>`,
  `OC+LIST_PAGES`.
- `OC+SET_LAYOUT` (v1 dashboard layout) keeps working via conversion.

BLE: the existing layout sync (`ble_layout_sync.cpp`) carries theme v2 and
page documents with the same validation, so the app can adopt them later.

## Web companion (`web/`)

- Theme tab: adds font roles, spacing, and per-style editing.
- New Page tab: pick a page id, see it at 240×320, add/move/resize nodes on a
  grid, choose type/style/binding/action from the device catalog, push,
  reset to built-in, export/import JSON.
- Preview: JS mirrors each component's drawing (as `preview.js` does for
  tiles today) with sample view-model data. Exact pixel parity in the browser
  is not required; layout, colors, and text must match.

## Build order

Each phase ships on its own and leaves the default look unchanged.

0. Commit the current uncommitted theme / widget model / web serial / camera
   work as the base.
1. Theme v2 + `ResolvedTheme`; components `button`, `icon_button`, `label`,
   `value_tile`, `hero_value`, `header`; dashboard tiles, hero, and header
   redrawn through them. Golden-image parity check.
2. `PressTracker`, action registry + `actions.cpp`, screen manager in
   `ui_task.cpp`.
3. Ride menu and power menu: view + built-in page doc + actions. Map buttons
   use `button`.
4. Settings list, camera remote, sensor/BLE manager, charging screen
   (`list`, `toggle`, `chart`, `battery` components). `widget_registry.cpp`
   loses all side effects.
5. Page doc model, validation, persistence, Web Serial commands, capabilities
   catalog; dashboard templates as built-in docs; v1 layout conversion; BLE
   sync accepts the new documents.
6. Web editor: Page tab and theme v2 editing.

## Testing

- Host tests (new runners or entries in `tests/run_native_tests.py` /
  `[env:native]`): theme v2 parse, validation, v1 back-compat; page doc
  validation (each rejection rule, truncation, caps); v1 layout → page doc
  conversion; every screen's view model; `actions.cpp` dispatch against
  fakes; `PressTracker`; components drawing against a recording fake canvas.
- **Golden parity:** render each default screen on the host into a 240×320
  buffer before and after the refactor and compare (the repo already renders
  previews on the host — `tests/test_ride_preview.cpp`,
  `tests/render_ride_preview.py`). Dashboard must be pixel-identical.
- On device: confirm frame period is unchanged (log loop time before/after),
  push a bad theme and a bad page and confirm nothing changes, push a valid
  custom page and confirm it survives reboot and resets.

## Resolved details (my calls, from design review)

1. **Dashboard pages and v1 layout.** `UiConfig` stays as the dashboard page
   *list* (count 1–8, order, titles). Each entry is either a template entry
   (as today) or a reference to a custom page doc (`dash.0`…`dash.7`). A
   pushed page doc may replace an existing dashboard entry or append one up
   to the cap; `OC+LIST_PAGES` plus a remove command handle deletion. A v1
   layout push (`OC+SET_LAYOUT` or BLE) replaces the whole list — last write
   wins, custom dashboard docs are dropped. `OC+GET_LAYOUT` / BLE read keep
   returning valid v1: template entries as today; custom entries as the
   template they were derived from, or `TEMPLATE_FULL_CONTAINER` with no
   widgets if none. The builder must confirm the Flutter app's parser accepts
   that (the app is not changed).
2. **Settings, sensor/BLE manager, camera remote stay dashboard widgets**
   (`WIDGET_SETTINGS_LIST` etc. in `SIZE_FULL` slots), reached by swiping as
   today. Internally each still splits into view + layout + actions; their
   inner layout is a built-in page doc rendered inside the widget rect.
   Escapability is guaranteed by the reset gesture and by validation
   rejecting a dashboard list with no settings widget on any page.
3. **Actions and bindings are scoped per page kind** (dashboard, ride menu,
   power menu, settings, sensor manager, camera). The capabilities catalog
   lists them per kind; validation rejects an action/binding/visibility id
   not allowed for the doc's page kind. Only the active screen's view model
   is computed each frame. Destructive actions (`power.off`, `sensor.forget`,
   settings writes) exist only on their own screens.
4. **Golden parity** = identical recorded draw-command stream (host fake
   canvas) before vs after, for every default dashboard template and the
   header. For ride/power menus and the full-page widgets, a deliberate
   stream change is allowed only if the PR includes before/after rasterized
   previews (extend `tests/render_ride_preview.py` as needed) for the owner
   to approve.
5. **`LIST` contents are fixed by firmware.** A pushed page controls a list's
   rect, row height, and styles only — not which items appear or their order.
   `PageNode` has no item sub-arrays.
6. **Reserved regions.** On dashboard pages the 44 px header strip is always
   drawn by firmware (style customizable) and validation rejects nodes that
   overlap it. On overlays (ride menu, power menu) the back target
   `(0,0,44,44)` is reserved the same way.

## Left to the builder

File names inside the listed directories, exact caps, final JSON key names,
font id strings, the persistence backend for custom pages, the reset gesture,
whether the old `PageConfig`/`UiConfig` types are kept internally or replaced
once conversion exists, and the Page tab's interaction details.
