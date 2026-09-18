# Ride display, sensor diagnostics and GPX export

## Changes

- Speed shown on the ESP32 and phone is a 3-second time-weighted average,
  published at 2 Hz. A stop, invalid input or source change clears the window.
  Raw speed is retained for ride state/statistics and debug. No GPS quality or
  hot-start thresholds have been relaxed.
- Two CSC slots now really connect and subscribe to 0x1816 / 0x2A5B instead
  of treating a saved MAC as a connection. Each independently decodes wheel
  and/or crank fields, wraps 16-bit event clocks, rejects malformed packets
  and implausible rates, zeros stopped events and expires silent readings.
  Capabilities determine speed/cadence, not discovery order. Address types
  and both MACs persist. Discovery does not pair automatically; select Connect
  from the scan results. Spin wheel/crank before Scan.
- Six BLE links are configured: phone, camera, HR, power and two CSC devices.
  Camera command implementation is unchanged. Basic Cycling Power Measurement
  notifications are supported; calibration/control points are not. A saved
  address is not proof of live data. See [sensor library UI](sensor-library-ui.md)
  for the paired-only list, explicit connection flow and scrolling behavior.
- Fusion owns altitude and display speed; stale BLE/logger snapshots cannot
  restore old altitude. Fresh valid barometer is preferred; otherwise fresh
  accepted GPS altitude is used. Neither available: `--`, no invented GPX
  elevation. Barometer pressure/temperature/age/source are exposed.

## Use

Phone: **Device → sensor debug**, Refresh or Live (2 s), Scan sensors and
Save debug. Captures at most 300 snapshots in memory; no continuous SD writes.
Live stops on leaving/backgrounding. ESP32: **Sensors → Debug sensors**.

Phone: finish the ride on the ESP32, then **Routes → saved rides → export**.
Choose a destination in Android's document picker. No SD file is deleted or
modified. Original bytes are preserved, including interrupted old logs useful
for debugging; export does not repair incomplete XML or guarantee Strava will
accept a particular old file. A finished GPX with valid timestamps can be
uploaded manually; no direct Strava account integration is added.

Protocol reuses the serialized RouteIO worker, not SD I/O inside GATT callbacks:
`10 + u32 index` lists one ride; `11 + u32 offset + ASCII basename` reads up to
160 bytes, returning offset, CRC32 and hex bytes. List ends with END. Names
are restricted to `/rides/*.gpx`, no separators/traversal; 32 MB download cap,
4096-entry client cap, idle ride required, OTA/shutdown exclusion per request.
Cancellation/disconnect never changes the source. Partial phone downloads are
not offered as successful exports. Requests 20/21 expose overview/CSC debug;
22 triggers a sensor scan. Updated phone and firmware are both needed.

## Accuracy limits / hardware checks still required

Barometric elevation still uses standard 1013.25 hPa, **not calibrated local
sea-level pressure**. Weather can shift absolute altitude, and flat roads may
legitimately show the same rounded altitude. A stuck 32 m cannot be attributed
to one physical cause without fresh pressure samples. Firmware still requires
a BME280 at 0x76/0x77 at boot; BMP280/wiring/supply faults are not fixed by UI.

On hardware: connect both spun-up sensors plus phone, check distinct CSC
addresses and packet counts; stop wheel/crank and verify zero/stale/fallback;
disconnect each sensor; reconnect after reboot; verify camera commands; compare
pressure while changing elevation; export an actual completed ride and compare
bytes to SD. Do not hot-plug I2C/SD while powered. Build/test success does not
substitute for these on-device checks. This change has not been flashed.

References:
- https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/CSCS_v1.0/out/en/index-en.html
- https://learn.adafruit.com/adafruit-bme280-humidity-barometric-pressure-temperature-sensor-breakout/f-a-q
