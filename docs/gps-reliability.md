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
five-second serial diagnostic reports raw fix/speed, filtered quality/speed,
satellites, HDOP, accuracy availability, horizontal/speed accuracy, position age
and received character count. Standby/wake and RAM-only 5 Hz measurement rate
remain unchanged; no backup-memory erase or GNSS cold reset.

## Accuracy-aware filtering (September 2026)

For a recognized M10, RAM configuration now enables UART1 UBX NAV-PVT every
navigation epoch. Its checksum-validated 92-byte solution supplies position,
ground speed, horizontal accuracy and speed accuracy from the same epoch.
Require a 3D fix, gnssFixOK and valid LLH; reject duplicate/backwards epochs.
Once PVT is configured/observed, missing PVT expires rather than silently
switching to less-informative NMEA. If configuration fails before PVT is seen,
the NMEA fallback remains available with a stricter HDOP/satellite gate.
PVT pDOP is not mislabeled as HDOP; HDOP is populated only by fresh GGA.

Application thresholds (tunable policy, not vendor specifications):

- PVT: at least 6 satellites, horizontal accuracy estimate >0 and <=15 m,
  speed accuracy estimate <=0.6 m/s. NMEA fallback: >=8 satellites, 0<HDOP<=2.
- Require 2 seconds of consecutive good, distinct observations before ready.
- Reject position jumps beyond speed-times-elapsed plus twice the summed
  horizontal uncertainties. Reject speed changes exceeding 8 km/h plus
  25 km/h per second elapsed. Existing 120 km/h cycling ceiling still applies.
- Hold the qualified initial position with zero speed until speed exceeds
  max(1.5 km/h, twice speed uncertainty) for 2 seconds AND displacement exceeds
  max(3 m, 1.5 times horizontal accuracy). This intentionally delays start.
- After motion, speed below max(1 km/h, speed uncertainty) for 1.5 seconds
  re-enters stationary hold. In NMEA fallback, use 3 km/h start, 1.5 km/h stop
  and a 7.5 m displacement gate; no fabricated accuracy is exposed as measured.
- Poor/stale observations invalidate usable GPS; reacquisition requalifies.
  Stationary coordinates are held, preventing wandering map/GPX points.
  Fusion adds no distance at zero filtered GPS speed and resets its anchor.
  BLE wheel speed is not overwritten. Manual Start/session ownership is unchanged.

UI distinguishes weak/settling observations from ready. A weak first fix leaves
the explicitly labeled map preview; loss after a usable fix holds the previous
viewport, without marking that old position as a fresh GPS fix.

Limitations: receiver accuracy values are estimates, not guarantees. Smooth,
confidently wrong multipath can still pass. This cannot recover real coordinates
from an indoor signal or add a second RF band. Slow starts and reacquisition
can undercount distance. Thresholds need outdoor and stop/start road validation;
no claim of Garmin-equivalent accuracy or measured power change.

## What the public vendor documentation establishes

Garmin recommends letting satellites acquire outdoors and using Auto Pause to
reduce distance accumulated while stopped. Its supported products also offer
multi-band/Auto Select modes. These are documented features, not disclosure of
its internal position filter or Kalman parameters:
[Garmin accuracy guidance](https://support.garmin.com/en-HK/?faq=Te47runiFR93oKcUAItwU7),
[Garmin SatIQ](https://support.garmin.com/fi-FI/?faq=wgKpMjTlpV29AQB5PVvTL9).

iGPSPORT lists L1+L5 dual-band GNSS for iGS630S. This is model-specific hardware
capability; its public product page does not reveal a stationary filter:
[iGS630S specifications](https://arg.igpsport.com/igs630s/).

Our protocol fields/keys come from the receiver's own documentation:
[u-blox M10 SPG 5.10](https://content.u-blox.com/sites/default/files/u-blox-M10-SPG-5.10_InterfaceDescription_UBX-21035062.pdf),
NAV-PVT section 3.15.11, CFG-MSGOUT and CFG-UART1OUTPROT. The gates and motion
state machine above are our own conservative implementation, not copied vendor code.

Regression check: `python3 tests/run_gps_tests.py` uses the installed real
TinyGPS++ source (run the ESP32 PlatformIO build first to install dependencies).
Coverage includes missing fields after reset, fresh GGA with stale RMC speed,
invalid fixes, corrupted checksums, 139/300 km/h outliers, recovery and bursts.
The quality suite also checks PVT coordinates/units/flags/length/epochs, indoor
quality rejection, stationary drift, slow movement, braking, jumps, stale
reacquisition and monotonic clock wrap. Navigation tests cover the weak-fix UI.
Also run native, navigation and ride host suites before uploading.

Bench tests establish software behavior, not road accuracy, time-to-first-fix
or measured standby current. Validate outdoors after sleep/wake. If fixes still
drop, capture the serial diagnostic and raw NMEA from the GPS debug screen;
antenna reception, supply stability and receiver output remain separate checks.
