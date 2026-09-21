# OpenCyclo

A DIY GPS cycling computer for the [lcdwiki 2.8" ESP32-S3
Display](https://www.lcdwiki.com/2.8inch_ESP32-S3_Display) board
(ES3C28P/ES3N28P, ESP32-S3 N16R8), built with PlatformIO and the Arduino
framework, with a Flutter companion app and a 3D-printable enclosure.

## Features

### Ride computer (firmware, `src/`)

- **GPS** — speed, distance, trip stats and auto-pause/resume from a u-blox M10
  over UART; AssistNow assisted GPS (registration, secure Chipcode, predictive
  cache) for fast fixes; fix-quality and accuracy gating; optional diagnostics
  log and an acquisition comparison mode.
- **Altitude** — barometric altitude, grade % and total ascent from a BME280 on
  the shared I2C bus, with manual and automatic calibration and GPS fallback.
- **BLE sensors** — speed/cadence (CSC), heart rate and cycling power over the
  standard GATT profiles, so off-the-shelf sensors work; sensor library with
  pairing, forget and scrolling; six concurrent links (phone, camera, sensors).
- **Phone as a source** — phone GPS (forced, or automatic fallback when the M10
  has no fix), phone barometer altitude, phone compass heading, and the phone's
  UTC clock for timestamps when the onboard receiver has none.
- **Ride logging** — GPX to microSD with timestamps, heart-rate and cadence
  extensions, segment splits across gaps, timestamped collision-safe file names,
  save/finish confirmation with retry, and SD mount recovery.
- **Dashboard** — touchscreen with swipeable pages built from a modular widget
  engine (tree layouts, per-page templates), customised from the app; hybrid
  icon + short-label design; ride, climb, sensors, settings and camera pages.
- **Offline navigation** — OSM road maps from the SD card (Indonesia pack
  generator in `tools/`), free ride and GPX route navigation with turn cues,
  off-route and arrival guidance, GPS direction marker, and road width scaled by
  class and zoom.
- **Camera remote** — Insta360 "GPS Remote" service: pair, shutter, mode,
  screen, wake and power off from the device.
- **Power** — BOOT button screen toggle, power-off/deep-sleep/restart menu,
  battery voltage and estimate, charging screen with an optional external USB
  detector.
- **Screen capture** — the app's Device tab → *screenshot* saves the current
  screen as a 240×320 BMP to `/screenshots` on the microSD; *download
  screenshot* pulls the latest one to the phone as PNG.
- **Firmware update over BLE** — OTA from the app, with shutdown/restart
  exclusion while an update is running.

### Companion app (Flutter, Android and iOS, `app/`)

- Scan, connect and live dashboard of the device's telemetry.
- Layout builder: design dashboard pages and sync them to the device.
- Device tab: sensor debug, assisted GPS, GPS / altitude / compass source
  toggles, screenshot and screenshot download.
- Routes tab: import a GPX and sync it as a route, list saved rides, and export
  ride GPX files with the fast binary download.
- OTA firmware update tab.

### Hardware and enclosure

- Board: lcdwiki 2.8" ESP32-S3 Display (ES3C28P, capacitive touch)
- GPS: u-blox M10 module (UART JST port)
- Barometer: BME280 (I2C JST port, shared with the touch controller)
- Any BLE cycling sensor advertising CSC (0x1816), HR (0x180D) or Cycling
  Power (0x1818)
- microSD card
- Enclosure CAD: [`hardware/cad/assembly_v13_FLIPPED.step`](hardware/cad/assembly_v13_FLIPPED.step)
  — the v13 enclosure assembly with the barometer mounted at the rear, as a
  STEP file (Open CASCADE export) for printing or further editing.

Full verified pinout is in the design doc and `src/config/pins.h`.

## Documentation

- [Offline maps and GPX navigation](docs/offline-navigation.md) — map pack
  generation, SD installation, device controls, GPX sync workflow
- [Power and charging](docs/power-and-charging.md) — controls, limits,
  verification
- [Ride display, sensor diagnostics and GPX export](docs/ride-sensors-and-export.md)
- [GPX binary download](docs/gpx-fast-download.md) — ride export and
  screenshot download protocol
- [GPX sync and explicit ride start](docs/gpx-sync-reliability.md)
- [AssistNow assisted GPS](docs/assisted-gps.md),
  [GPS telemetry reliability](docs/gps-reliability.md),
  [GPS diagnostics toggle](docs/gps-diagnostics.md),
  [GPS acquisition comparison mode](docs/gps-baseline-test.md),
  [GPS position direction marker](docs/gps-position-marker.md),
  [GPS startup and optimisation (Indonesian)](docs/gps-startup-dan-optimasi.md),
  [GPS ride regression, 13 Sep 2026](docs/gps-ride-regression-2026-09-13.md)
- [Altitude calibration](docs/altitude-calibration.md),
  [Manual barometer calibration](docs/barometer-calibration.md),
  [Barometer shared-bus repair](docs/barometer-i2c.md)
- [Sensor library and scrolling](docs/sensor-library-ui.md),
  [Hybrid UI direction](docs/hybrid-ui.md),
  [SD mount recovery](docs/sd-mount-recovery.md),
  [Android Bluetooth access](docs/android-bluetooth-access.md)
- [Android release](docs/android-release.md) — upload keystore, Fastlane lanes,
  tagged GitHub Releases
- [Publishing on Google Play](docs/play-console-setup.md) — console steps,
  declarations, and the production-access gate
- [Privacy policy](docs/privacy-policy.md) — what the app does with your data
- Design specs in [`docs/superpowers/specs/`](docs/superpowers/specs/):
  firmware architecture, companion app and OTA, modular UI engine, map road
  names, phone GPS source
- [`docs/design-review/`](docs/design-review/) — presentation-only design
  system and interactive flow review

## Status

🚧 In development. See
[`docs/superpowers/specs/2026-08-21-opencyclo-firmware-design.md`](docs/superpowers/specs/2026-08-21-opencyclo-firmware-design.md)
for the full architecture and design rationale.

## Building

Firmware:

```bash
pio run              # build
pio run -t upload    # flash
pio device monitor   # serial monitor (native USB CDC)
```

Debugging builds (not for normal rides):

```bash
pio run -e esp32-s3-gps-diag -t upload   # records GPS diagnostics to /debug on SD
pio run -e esp32-s3-usb-sense -t upload  # only with a voltage-safe VBUS detector on GPIO2
```

Host tests (no board required):

```bash
python3 tests/run_native_tests.py       # GPX, screenshot, power, GNSS, sensors, ...
python3 tests/run_navigation_tests.py   # map renderer and BLE transfer service
python3 tests/run_ride_tests.py         # ride lifecycle and touch UI
```

Companion app:

```bash
cd app
flutter test
flutter run
```

## License

TBD.
