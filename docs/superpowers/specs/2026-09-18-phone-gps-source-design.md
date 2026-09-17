# Configurable GPS Source: Hardware M10 vs Phone GPS

**Date:** 2026-09-18
**Status:** Approved for Implementation Planning

## 1. Overview & Purpose

The onboard M10 GPS module has a slow cold/warm fix time. This spec adds a
second GPS source — the connected phone's own location — so the firmware can
use phone-provided lat/lon/accuracy instead of (or as a fallback for) the M10
fix. The source is configurable, from both the app and the device itself:

- **Hardware mode** (default): use the M10 as today. If the M10 has no valid
  fix for longer than a threshold and a fresh phone fix is available, the
  firmware **automatically falls back** to the phone's position until the M10
  regains a valid fix, then switches back.
- **Phone-forced mode**: always use the phone's position; the M10 fix is
  ignored entirely regardless of its state (for cases where the M10 hardware
  itself is degraded/unavailable).

Position data flows from phone to device over the **existing BLE GATT
connection** the app already maintains for telemetry/layout sync — not via a
separate BLE advertising/beacon channel. See §2 for why.

---

## 2. Transport Decision: GATT Write vs BLE Advertising

Two approaches were considered:

- **BLE Advertising** (phone broadcasts lat/lon as an advertisement, device
  passively scans): rejected. iOS restricts background BLE peripheral
  advertising to an overflow UUID only — custom service data does not survive
  once the app is backgrounded, which is the primary real-world usage (phone
  in a pocket/mount, screen off, device shows the nav UI). This would make
  the feature unreliable exactly when it's needed.
- **GATT Characteristic Write** (chosen): the phone is already connected as
  BLE central to sync telemetry/layout. Reusing this connection to write
  lat/lon/accuracy periodically has precedent in this codebase — the existing
  GPS assistance/AGPS channel (`0x1907`) already pushes phone→device data the
  same way. No new advertising permissions, no iOS background-peripheral
  limitation, and it piggybacks on a radio link that's already open, so it's
  also the more power-efficient option on the phone side.

---

## 3. BLE GATT Specification (additions to existing `0x1900` service)

### 3.1 New Characteristic: Phone GPS Position (`UUID: 0000190A-0000-1000-8000-00805F9B34FB`)
- **Properties**: Write Without Response
- **Payload (Binary, 11 bytes)**:
  - `int32_t lat_e7` — latitude × 1e7
  - `int32_t lon_e7` — longitude × 1e7
  - `uint16_t accuracy_cm` — horizontal accuracy in centimeters (clamped to 65535)
  - `uint8_t seq` — monotonically incrementing sequence number (staleness/dedup check)
- Written by the app roughly once per second while `Geolocator` produces a
  new position and the phone is connected.
- Firmware records `millis()` on receipt for staleness tracking; no fixed
  update-rate contract is enforced beyond "app writes on every new position".

### 3.2 New Characteristic: GPS Source Mode (`UUID: 0000190B-0000-1000-8000-00805F9B34FB`)
- **Properties**: Read, Write, Notify
- **Payload**: `uint8_t mode` — `0x00 = HARDWARE` (default, with auto-fallback), `0x01 = PHONE_FORCED`
- App reads this on connect to reflect current device state in the UI, writes
  to change it, and receives a Notify if the mode is changed locally on the
  device's own menu (keeps app and device UI in sync per the "control from
  both" requirement).

### 3.3 Existing Device Commands (`0x1903`) — unchanged
No new opcodes needed here; the dedicated `0x190B` characteristic handles mode
read/write/notify directly.

---

## 4. Firmware Changes

### 4.1 Settings persistence (`src/storage/settings.h/.cpp`)
Add to the `Settings` struct, following the existing `Preferences`/NVS pattern:
```cpp
uint8_t gps_source_mode; // 0 = HARDWARE (auto-fallback), 1 = PHONE_FORCED
```
Default `0`. Loaded/saved alongside existing fields in `initSettings()` /
`saveSettings()`.

### 4.2 Phone fix intake (`src/hardware/ble_task.cpp`)
- Add a GATT write handler for `0x190A` that decodes the 11-byte payload into
  a `GpsFix`-shaped value (`lat`, `lon`, `accuracy`, `source = SOURCE_PHONE`)
  and pushes it into a new single-slot queue `g_phone_gps_queue`
  (`xQueueOverwrite`), mirroring how `g_gps_queue` already works.
- Add a GATT read/write/notify handler for `0x190B` that gets/sets
  `Settings.gps_source_mode`, persists via `saveSettings()`, and `notify()`s
  connected clients on change (including changes made via the device's own
  menu, per §4.4).

### 4.3 Source arbitration (`src/hardware/gps_task.cpp`)
This is the single choke point that already writes into `g_gps_queue`
(consumed by `FusionTask`). Extend the loop that currently does
`xQueueOverwrite(g_gps_queue, &fix)` from the M10-derived fix:

```
read latest M10 fix (existing GpsFilter::apply() output)
read latest phone fix from g_phone_gps_queue (non-blocking peek) + its age

if gps_source_mode == PHONE_FORCED:
    if phone fix age < PHONE_FIX_STALE_MS:
        push phone fix (mark GpsFix.source = SOURCE_PHONE)
    else:
        push "no fix" (fixType = NONE) — do not fall back to M10 in this mode
else: // HARDWARE
    if M10 fix is valid (per existing GpsFilter logic):
        push M10 fix (source = SOURCE_HARDWARE)
    else if phone fix age < PHONE_FIX_STALE_MS:
        push phone fix (source = SOURCE_PHONE_FALLBACK)
    else:
        push "no fix"
```
`PHONE_FIX_STALE_MS` = 5000ms (a phone fix older than this is treated as
absent — covers BLE disconnects or the app being killed).

`GpsFix` (`src/core/telemetry_state.h`) gains a `source` field (enum:
`SOURCE_HARDWARE`, `SOURCE_PHONE_FALLBACK`, `SOURCE_PHONE`) purely for
diagnostics/UI ("using phone GPS" indicator); it does not change how
`FusionTask` consumes `lat`/`lon`/`accuracy` — no changes needed in
`fusion_task.cpp` beyond reading this new field if the UI wants to surface it.

### 4.4 Device-side toggle (mode control from the device itself)
Per the "toggle from both app and device" decision: add a settings menu entry
(wherever the existing device UI settings live, e.g. alongside
`sd_logging_enabled`) that cycles `gps_source_mode` and calls the same
`setGpsSourceMode()` helper used by the BLE write handler in §4.2, so both
paths persist identically and both trigger the `0x190B` Notify to the app.

---

## 5. Flutter App Changes (`/app`)

### 5.1 New dependencies (`pubspec.yaml`)
- `geolocator` — phone position stream.
- Android: request `ACCESS_FINE_LOCATION`, and (only while phone-mode is
  active) run a **foreground service** with a persistent notification to keep
  location + the BLE connection alive with the screen off. Use
  `flutter_foreground_task` or an equivalent already-vetted package — final
  package choice is an implementation-time decision, not a design constraint.
- iOS: request **"Always"** location authorization, and enable
  `UIBackgroundModes: [location, bluetooth-central]` in `Info.plist` so the
  existing BLE central connection and GPS updates keep running backgrounded.

### 5.2 New files
- `lib/core/location/phone_gps_service.dart` — wraps
  `Geolocator.getPositionStream()` (best accuracy, ~1s interval), exposes a
  stream of `(lat, lon, accuracyM)`.
- `lib/core/ble/ble_protocol.dart` (extend existing BLE service) — encode
  `0x190A` payload (11 bytes, see §3.1) and write it (write-without-response)
  on each new position while `gps_source_mode` (app-side mirrored state) is
  non-default and BLE is connected. Add read/write/subscribe for `0x190B`.
- `lib/state/gps_source_provider.dart` — Riverpod provider holding the current
  mode (synced from the device via `0x190B` read + Notify on connect), and
  exposing a setter that writes `0x190B` and starts/stops
  `phone_gps_service.dart` + the foreground service accordingly.

### 5.3 Settings UI
No settings screen exists yet in the app (`device_tab.dart` currently only
handles scan/connect). Add a small "GPS Source" section to `device_tab.dart`
with a two-option segmented control (Hardware / Phone), bound to
`gps_source_provider`. This intentionally stays inline in the existing device
tab rather than introducing a new screen, since it's the only setting the app
currently exposes.

### 5.4 Permission & background behavior notes (surfaced to user, not silent)
- On first enabling phone mode, show a one-time explainer before the OS
  permission prompt: iOS requires "Always" location (stronger prompt/wording
  than "While Using"); Android will show a persistent notification while
  phone-mode is active (required for the foreground service).
- Recommend (in-app copy, not enforced) keeping the phone screen off/locked
  during phone-mode rides for battery life — per the battery estimates
  discussed, background GPS+BLE only costs roughly 5-12%/hour vs. 15-25%/hour
  with the phone's own screen on for maps.

---

## 6. Data Flow Summary

```
┌─────────────────────────┐        0x190A write (lat/lon/acc, ~1Hz)
│   Flutter App (phone)    │ ─────────────────────────────────────────┐
│  Geolocator position     │        0x190B read/write/notify (mode)   │
│  stream → BLE GATT write │ ◄─────────────────────────────────────┐  │
└─────────────────────────┘                                        │  │
                                                                     │  │
┌────────────────────────────────────────────────────────────────┐ │  │
│                        ESP32-S3 Firmware                        │ │  │
│  ble_task.cpp: 0x190A handler → g_phone_gps_queue                │ │  │
│                0x190B handler → Settings.gps_source_mode ───────┘ │  │
│                                        │        (persisted, NVS)   │
│  gps_task.cpp: arbitration (§4.3)      │                           │
│    M10 fix (GpsFilter) ─┐              │                           │
│    phone fix ───────────┼─► g_gps_queue (single xQueueOverwrite)   │
│                          │                                         │
│  FusionTask: reads g_gps_queue → TelemetryState.lat/lon (unchanged)│
│                                                                     │
│  Device settings menu → setGpsSourceMode() ────────────────────────┘
│    (also persists + triggers 0x190B Notify)
└────────────────────────────────────────────────────────────────┘
```

---

## 7. Out of Scope
- No changes to `FusionTask`'s distance/speed/grade calculation — it keeps
  consuming `GpsFix.lat/lon` exactly as today, regardless of source.
- No automatic re-encoding of phone speed/altitude into telemetry; the phone
  only supplies lat/lon/accuracy (per the chosen payload), firmware continues
  deriving speed/distance/grade from position deltas as it does for the M10.
- No UI change to `live_dashboard_tab.dart` beyond (optionally, at
  implementation time) surfacing the `source` field as a small "using phone
  GPS" indicator — not required for correctness.
- GPX ride-file logging on the device is unaffected; it logs whatever
  `TelemetryState` holds regardless of GPS source.

---

## 8. Implementation Milestones

1. **Firmware: settings + GATT characteristics** — add `gps_source_mode` to
   `Settings`, implement `0x190A`/`0x190B` handlers in `ble_task.cpp`.
2. **Firmware: source arbitration** — implement §4.3 logic in `gps_task.cpp`,
   add `source` field to `GpsFix`.
3. **Firmware: device-side toggle** — settings menu entry per §4.4.
4. **App: phone GPS service** — `geolocator` integration, foreground
   service (Android) / background modes (iOS) plumbing.
5. **App: BLE protocol + provider** — `0x190A` write loop, `0x190B`
   read/write/notify, `gps_source_provider.dart`.
6. **App: Settings UI** — GPS Source toggle in `device_tab.dart`, permission
   explainer copy.
7. **End-to-end verification** — force M10 to no-fix (e.g. antenna
   disconnected) and confirm auto-fallback to phone within
   `PHONE_FIX_STALE_MS`; confirm switch-back once M10 recovers; confirm
   phone-forced mode ignores M10 entirely; confirm mode toggled on-device
   reflects in app via Notify and vice versa.

---

## 9. Spec Self-Review Checklist
- [x] **Placeholder Scan:** No "TBD"/"TODO" items; package choice for the
      Android foreground service is explicitly left open as an
      implementation-time decision, not a placeholder.
- [x] **Internal Consistency:** UUIDs (`0x190A`, `0x190B`) don't collide with
      existing `0x1901-0x1903`, `0x1906-0x1909`; payload sizes match between
      firmware decode and app encode.
- [x] **Scope Check:** Single feature, 7 milestones, spans firmware + app but
      shares one protocol — not decomposed further.
- [x] **Ambiguity Check:** Explicit byte layout, staleness threshold, and
      mode-arbitration pseudocode; battery/permission trade-offs stated
      rather than left implicit.
