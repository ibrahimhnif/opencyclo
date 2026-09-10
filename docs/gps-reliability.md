# GPS telemetry reliability

The decoder admits complete checksum-verified RMC/GGA sentences before feeding
TinyGPS++ 1.1.0. Empty speed fields must not commit a retained pending value.
Reset reconstructs the parser on zeroed storage rather than copying a temporary
whose pending fields are not all initialized by the library constructor.

Position and speed have separate 1.5-second freshness checks. New GGA positions
do not refresh old RMC speed. Invalid/stale GPS speed becomes zero with no speed
source, without overriding a BLE wheel-speed source. Fusion also rejects queued
data older than 1.5 seconds and breaks the distance anchor across missing fixes.
BLE telemetry callbacks cannot restore an obsolete GPS speed snapshot.

The cycling sanity ceiling is 120 km/h: negative, non-finite, empty and faster
speed samples are rejected, not clamped into ride maximum statistics. This is
an application limit, not a receiver specification or a guarantee that all
lower-speed multipath errors are detected. Existing saved rides are not edited.

UART RX buffering is 2048 bytes, drained every 10 ms. A single-slot overwrite
queue delivers the latest snapshot instead of accumulating stale fixes. The
five-second serial diagnostic reports fix, speed validity, speed, satellites,
HDOP, position age and received character count. Standby/wake and RAM-only 5 Hz
configuration remain unchanged; no backup-memory erase or GNSS cold reset.

Regression check: `python3 tests/run_gps_tests.py` uses the installed real
TinyGPS++ source (run the ESP32 PlatformIO build first to install dependencies).
Coverage includes missing fields after reset, fresh GGA with stale RMC speed,
invalid fixes, corrupted checksums, 139/300 km/h outliers, recovery and bursts.
Also run native, navigation and ride host suites before uploading.

Bench tests establish software behavior, not road accuracy, time-to-first-fix
or measured standby current. Validate outdoors after sleep/wake. If fixes still
drop, capture the serial diagnostic and raw NMEA from the GPS debug screen;
antenna reception, supply stability and receiver output remain separate checks.
