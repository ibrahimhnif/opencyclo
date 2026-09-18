# GPS ride regression — 13 September 2026

Comparison of the user-provided iGPSPORT and OpenCyclo GPX files found:

- Approximately 9m10s / 2.4 km missing at the start of OpenCyclo's track.
- Coordinate-derived distances: 44.35 km vs 41.47 km; restricting to common
  time coverage reduces the difference to approximately 0.39 km (0.93%).
- 167 OpenCyclo coordinate steps above 60 km/h, all immediately preceded by
  identical coordinates. These are derived speeds, not raw receiver speeds.
- 12 equal-timestamp pairs with differing coordinates.

iGPSPORT timestamps required approximately -7 hours alignment; this is an
inferred export clock offset, not proof of either device's absolute accuracy.
Neither file contains raw fix quality, so the initial delay cannot be assigned
conclusively to acquisition or filtering. Original ride files are unmodified.

## Changes

- Keep fresh, quality-qualified coordinates attached to their own epoch.
  Stationary detection suppresses speed only, never substitutes old coordinates.
  Consequently stationary position jitter can remain visible; this deliberately
  avoids false catch-up jumps. No invented/interpolated GPX points.
- Preserve established filter/movement state across rejected samples for less
  than five seconds since the last accepted observation. Rejected observations
  remain invalid. Five seconds without an accepted sample requires fresh
  qualification. Startup still requires two seconds of qualifying observations.
- Retain the five-satellite, 15 m horizontal accuracy, 0.6 m/s speed accuracy
  gates (HDOP <= 2 on NMEA fallback), freshness and outlier checks.
- Skip duplicate/backward GPX timestamps, including after partial writes.
  Gaps exceeding five timestamp seconds start a new GPX segment rather than
  drawing a fictitious shortcut. This exposes missing data; it cannot restore it.
- GPS serial diagnostics include a rejection reason every five seconds.

## Validation and next ride

GPS parser/filter, native GPX and ride/UI regression suites pass; ESP32-S3
release firmware builds. No field accuracy claim follows from these tests.

Next outdoor test: start both devices together, preserve serial `[GPS]` output
from power-on until first accepted fix, then record the same loop including
stops and turns. Compare first-point delay, common-window distance, repeated
positions and derived speed spikes. Raw fix=0 suggests receiver acquisition;
raw fix=1 with accuracy/satellites rejection indicates the quality gate instead.
Five-second serial snapshots may miss individual short rejection events.

This change does not alter GNSS power configuration, Bluetooth, camera controls,
GPX transfer protocol or require a Flutter update. It cannot reconstruct the
missing beginning of an existing ride.
