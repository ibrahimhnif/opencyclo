# OpenCyclo Firmware Design

**Date:** 2026-08-21
**Status:** Approved for implementation planning

## Purpose

OpenCyclo is a DIY GPS cycling computer built on a 2.8" ESP32-S3 smart
display board. It shows live ride telemetry (speed, cadence, heart rate,
power, elevation/climb), supports standard BLE cycling sensors, and logs
completed rides to a microSD card as GPX tracks.

## Hardware

**Board:** lcdwiki 2.8inch ESP32-S3 Display (models ES3C28P / ES3N28P),
ESP32-S3 N16R8 (16MB flash, 8MB OPI PSRAM).

The initially-supplied pinout reference did not match this board (it
described a different ST7789/CST816D-based board). The pinout below is
verified against the manufacturer wiki
(https://www.lcdwiki.com/2.8inch_ESP32-S3_Display) and is the single
source of truth (`config/pins.h`).

| Function | Pin(s) |
|---|---|
| TFT driver | ILI9341, SPI |
| TFT CS / DC / SCK / MOSI / MISO / BL | 10 / 46 / 12 / 11 / 13 / 45 |
| TFT RST | shared with EN |
| Touch controller | FT6336G, I2C |
| Touch SDA / SCL / INT / RST | 16 / 15 / 17 / 18 |
| GPS UART (JST, external M10 module) | TX=44, RX=43 |
| External I2C JST (barometer) | shares bus: SDA=16, SCL=15 |
| microSD | SDIO mode: CLK=38, CMD=40, D0-D3=39/41/48/47 (requires `SD_MMC`, not SPI `SD`) |
| Speaker (out of scope for v1) | ES8311 codec + FM8002E amp, I2S |
| Battery | TP4054 charger, 2-pin JST; voltage sense ADC on GPIO9 |
| USB (flashing/monitor) | Native USB (GPIO19/20), no USB-serial bridge chip |
| Free expansion GPIOs | 2, 3, 14, 21 |

**External sensors:**
- GPS: u-blox M10 module via UART JST port.
- Barometer: BMP280 via external I2C JST port (shares bus with touch
  controller — no address conflict; BMP280 is 0x76/0x77, FT6336G is 0x38).
- BLE: any standard sensor advertising Cycling Speed & Cadence (0x1816),
  Heart Rate (0x180D), or Cycling Power (0x1818) GATT services.

## Scope (v1)

- GPS speed/distance/trip stats
- Barometer-based altitude/grade/total ascent
- Touchscreen UI, multiple pages
- SD card GPX ride logging
- BLE sensor support: Speed/Cadence, Heart Rate, Cycling Power — all
  three built in v1, using a sensor-source abstraction that makes
  adding further BLE profiles later (e.g. ANT+ bridge, additional
  sensor types) a matter of adding another client + queue producer,
  not a rearchitect.

**Out of scope for v1:** onboard speaker/audio (ES8311/FM8002E),
RGB status LED, wheel-magnet speed sensor (BLE CSC covers this use
case), map rendering, imperial unit hardcoding (units are switchable
but default metric).

## Architecture

### Concurrency model

Fully separated FreeRTOS tasks per subsystem, communicating via
queues into a single fusion point:

| Task | Core | Responsibility |
|---|---|---|
| `UiTask` | 1 | LovyanGFX rendering + FT6336G touch input, ~15-20Hz. Reads a snapshot of `TelemetryState`; never touches UART/I2C/SD directly. |
| `GpsTask` | 0 | Owns `HardwareSerial` on GPIO43/44, feeds TinyGPS++, pushes `GpsFix` to a queue on each valid sentence. |
| `BaroTask` | 0 | Polls BMP280 over I2C at ~2-5Hz, pushes `BaroSample` (altitude, grade) to a queue. |
| `BleTask` | 0 | NimBLE-Arduino central: scan/connect/subscribe to CSC/HR/Power characteristics; parses notifications, pushes `SensorSample` to a queue; owns reconnect logic. |
| `FusionTask` | 0 | Sole consumer of the GPS/Baro/BLE queues and **sole writer** of `TelemetryState` (mutex-protected for readers). Owns ride auto-start/stop state machine. |
| `LoggerTask` | 0 | Consumes ride-point events from `FusionTask` (1Hz while active), buffers and writes GPX via `SD_MMC`; owns file lifecycle. |

Single-writer `TelemetryState` avoids six-way mutex contention: `UiTask`
and `LoggerTask` are simple readers of a snapshot; only `FusionTask`
writes.

One explicit cross-task shared resource requiring its own mutex: the
I2C bus (SDA16/SCL15) is touched by both `UiTask` (touch polling) and
`BaroTask` (BMP280), so `Wire` access is guarded by a bus mutex.

### Module/file layout

```
src/
  main.cpp                  -- task creation, queue/mutex setup
  config/pins.h              -- verified pin map, single source of truth
  hardware/display.{h,cpp}   -- LGFX device class (ILI9341 + FT6336G)
  hardware/gps_task.cpp
  hardware/baro_task.cpp
  hardware/ble_task.cpp      -- + ble/csc_client, ble/hr_client, ble/power_client
  core/telemetry_state.h     -- shared struct + mutex accessors
  core/fusion_task.cpp       -- sensor priority logic, ride state machine
  storage/logger_task.cpp
  storage/gpx_writer.{h,cpp}
  storage/settings.{h,cpp}   -- Preferences/NVS wrapper
  ui/ui_task.cpp
  ui/pages/ride_page.cpp
  ui/pages/climb_page.cpp
  ui/pages/sensors_page.cpp
  ui/pages/settings_page.cpp
```

### `TelemetryState` (canonical shared struct)

```
speed_kmh, speed_source (GPS | BLE_CSC)
cadence_rpm            -- BLE_CSC only, "--" if not connected
heart_rate_bpm         -- BLE_HR only, "--" if not connected
power_watts            -- BLE_CP only, "--" if not connected
lat, lon, altitude_m, grade_pct, total_ascent_m
trip_distance_km, ride_time_s, avg_speed_kmh
gps_fix_quality, ble_connection_status[3], sd_status, battery_pct
ride_state: IDLE | ACTIVE | PAUSED
```

### Sensor fusion / priority

- **Speed:** BLE CSC wheel-revolution speed wins whenever a CSC sensor
  is connected (more accurate; unaffected by tunnels/urban canyon);
  falls back to GPS-derived speed immediately on disconnect or if
  never paired. `speed_source` is exposed so the UI can badge which is
  active.
- **Cadence / HR / Power:** BLE-only, no GPS equivalent — shown as
  `--` when not connected.
- **Distance:** GPS-derived.
- **Altitude/grade:** barometer-primary (fast response), periodically
  sanity-corrected against GPS altitude to bound barometric drift from
  weather pressure changes.

### Ride auto-start/stop (owned by `FusionTask`)

- `IDLE → ACTIVE`: sustained speed > 3 km/h for ~5s (debounces stoplight
  glances).
- `ACTIVE → PAUSED`: speed ~0 for ~30s (coasting/stoplight doesn't
  pause; a genuine stop does).
- `PAUSED → ACTIVE`: speed > 3 km/h resumes the same ride/file.
- `PAUSED → IDLE` (ride ends, GPX file closed): manual "End Ride" tap
  on the Ride page — the one deliberate manual interaction, since
  auto-detecting "done for the day" vs. "long coffee stop" isn't worth
  the complexity.
- Transitions emit an event to `LoggerTask` (open/close GPX file) and
  drive the Ride page's status badge.
- Ride auto-start is gated on having a valid GPS fix (needed for both
  the ride's start timestamp and GPX coordinates).

### BLE pairing

`BleTask` scans for CSC/HR/Power-advertising devices. The Sensors page
lists discovered devices; tapping one pairs it, and the chosen MAC per
profile is persisted via `settings.cpp` (NVS) so it auto-reconnects on
boot without re-scanning. A "forget sensor" action clears it.

## UI

Four pages, swiped left/right on the touchscreen:

1. **Ride** — speed (with source badge), cadence/HR/power tiles
   (dashed when not connected), trip distance, ride time, avg speed,
   ride-state indicator, "End Ride" tap target.
2. **Climb** — altitude, grade %, total ascent, elevation sparkline
   for the current ride.
3. **Sensors** — GPS fix status/sat count; one row per BLE profile
   showing connection state + scan/pair or forget action.
4. **Settings** — units (metric/imperial, switchable), screen
   brightness (PWM on GPIO45), wheel circumference (mm, for CSC
   wheel-rev → speed/distance conversion), SD logging on/off, device
   info (battery %, firmware version).

Settings are read once at boot into a `Settings` struct shared
(read-only after boot, except when the Settings page writes a change)
by `FusionTask` and `UiTask`.

## Storage: GPX logging

`LoggerTask` + `gpx_writer`:

- On `ACTIVE` entry: open `/rides/YYYYMMDD_HHMMSS.gpx` (filename
  timestamped from GPS UTC — first fix is a prerequisite), write GPX
  header + `<trk><trkseg>`.
- Every 1s while `ACTIVE`: append a `<trkpt lat lon><ele><time>` from
  the current `TelemetryState` snapshot; buffered in RAM, flushed to
  SD every ~10 points to bound wear/latency spikes.
- On `PAUSED`: stop appending points (a gap in the track is normal
  GPX), keep the file open.
- On ride end (`PAUSED → IDLE`): close `</trkseg></trk>` + GPX footer,
  close the file.

## Error handling / degraded modes

- **No GPS fix yet:** Ride page shows speed as `--`/0; Sensors page
  shows "Acquiring..."; ride auto-start is gated on a fix.
- **BLE sensor disconnects mid-ride:** `BleTask` retries reconnect
  against the last known MAC in the background; `speed_source` falls
  back to GPS immediately so the Ride page never freezes.
- **SD card missing/full/write failure:** `LoggerTask` flags
  `sd_status`; Sensors page shows a warning icon; ride tracking/stats
  continue in-memory regardless — SD logging is best-effort and never
  blocks the ride experience.
- **I2C bus contention:** guarded by an explicit bus mutex shared
  between `UiTask` (touch) and `BaroTask` (BMP280).

## Libraries (Arduino framework via PlatformIO)

- **LovyanGFX** — display (ILI9341) + touch (FT6336G via its FT5x06-family driver)
- **TinyGPSPlus** — GPS NMEA parsing
- **Adafruit BMP280** — barometer over I2C
- **NimBLE-Arduino** — BLE central for CSC/HR/CP GATT clients
- **SD_MMC** (bundled with arduino-esp32) — SDIO microSD access
- **Preferences** (NVS, bundled) — settings persistence

## Board configuration notes (for `platformio.ini`)

- `board = esp32-s3-devkitc-1` as the base, tuned for N16R8:
  `board_build.arduino.memory_type = qio_opi`,
  `board_upload.flash_size = 16MB`,
  `board_build.partitions = default_16MB.csv`.
- Native USB CDC (GPIO19/20) is used for flashing/serial monitor
  (confirmed — no USB-serial bridge chip on this board), which is why
  UART0 (GPIO43/44) is free for the GPS module without conflict.
- Battery percentage reads raw voltage via ADC on GPIO9, converted to
  percent with a discharge-curve lookup calibrated during bring-up
  (Li-Po voltage-to-charge is nonlinear; exact curve constants are a
  bring-up task, not a design decision).

## Milestone order (for the implementation plan)

Bring-up proceeds in hardware-verification order so each milestone is
independently testable on the bench before the next depends on it:

1. Display + touch bring-up (LGFX config, page-switching shell)
2. GPS task + Ride page speed/distance
3. Barometer task + Climb page
4. SD/GPX logging tied to ride state machine
5. BLE sensor clients (CSC → HR → Power) + Sensors page
6. Settings page + NVS persistence, error/degraded-mode handling
7. Polish pass
