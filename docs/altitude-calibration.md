# Altitude calibration

Open Sensors → Calibrate → GPS auto. Default OFF preserves existing manual
calibration; the toggle and successful reference pressure are stored in NVS.
Saving a manual elevation disables automatic calibration.

When enabled, auto calibration waits for an idle ride, fresh barometer, accepted
GPS fix with NAV-PVT vertical accuracy >0 and ≤10 m, and GPS/display source speed
below 1.5 km/h. It averages distinct GPS epochs over 30 continuous seconds
(at least 25 samples, no gap ≥1.5 s, altitude range ≤6 m). Invalid data, movement,
or starting a ride restarts the window. These are application policy thresholds,
not a guarantee of absolute accuracy. NMEA without vertical accuracy cannot
automatically calibrate.

One successful automatic calibration per boot (or explicit re-enable) computes
reference pressure from mean GPS MSL altitude and the current pressure. It never
recalibrates during an active or paused ride. The barometer remains the altitude
source; smoothing/grade baselines reset when the reference changes. The UI shows
waiting or calibrated; serial logs include `[BARO CAL]` source and reference.

Without fresh barometer data, use fresh GPS altitude with a 3-second time-based
EMA, without reprocessing duplicate epochs. NAV-PVT fallback requires vertical
accuracy ≤25 m; NMEA fallback requires fresh altitude but has no vAcc estimate.
Missing/stale GPS is not displayed as valid zero elevation.

NAV-PVT hMSL at byte 36 is mean-sea-level altitude; vAcc at byte 44 is the
receiver's vertical accuracy estimate, both in millimetres:
[u-blox M10 interface, NAV-PVT](https://content.u-blox.com/sites/default/files/u-blox-M10-SPG-5.10_InterfaceDescription_UBX-21035062.pdf).

Field check: enable auto before starting a ride, remain stationary outdoors until
calibrated, compare with a surveyed elevation, then start. Check manual save,
reboot persistence, barometer disconnect fallback, and GPS loss. A GPS fix alone
does not establish accurate elevation; manual known elevation remains preferable
when available.
