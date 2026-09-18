# Manual barometer calibration

On the ESP32, open Sensors → Calibrate. Enter the known elevation in meters
using -10, -1, +1 or +10, then Save. For a 5 m reference, tap +1 five times
from the initial 0 m value. Wait for Saved; altitude smoothing takes a few
seconds to settle. The value is always meters, even if dashboard units are imperial.

Save requires a fresh barometer sample and no active or paused ride. The worker
rechecks these conditions using its current sample. It derives the sea-level
pressure reference and persists it to NVS namespace baro / key reference.
Only successful writes update the running reference. Invalid or missing stored
values fall back to 1013.25 hPa. No changes are made to past GPX files.

Supported input is -500 to 9000 m, subject to a plausible derived pressure
reference of 800–1200 hPa. Errors remain visible and allow retry. A matching
elevation from another bike computer is a relative reference, not proof of
absolute accuracy. Recalibrate as weather changes. GPS auto-calibration and
phone-side remote calibration are not included.
