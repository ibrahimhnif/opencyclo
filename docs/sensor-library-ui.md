# Sensor library and scrolling

ESP32 Sensors now has two views:

- Paired: only stored sensors, grouped by actual known type, address and live
  connected/offline status; Forget targets that stored slot. Empty state + Scan.
- Scan results: bounded to 24 service/address/type entries, duplicates update
  RSSI instead of adding rows. Groups are Speed/Cadence, Heart rate and Power.
  Already-paired addresses are excluded. Tap Connect to subscribe, then Back
  to see the saved sensor. Failures don't persist new pairings. Existing saved
  addresses are preserved as offline entries, not silently deleted.

Speed vs cadence is unknown from the CSC service advertisement alone. The
paired label becomes Speed, Cadence or Speed/Cadence based on measurement flags.
Combined CSC needs only one row. Current capacity is two CSC devices, one HR
and one power sensor. Connecting another of a full type fails without replacing
the old sensor; Forget one first. Power uses basic 0x2A63 instantaneous watts,
not calibration/control-point features. Six BLE links cover these four sensors,
phone and camera; physical simultaneous-link testing remains necessary.

Both views scroll vertically inside a clipped list; toolbar/footer stay fixed.
An 8-pixel gesture threshold suppresses actions on drags (even out-and-back).
Horizontal swipes still switch dashboard pages. Row identities/layout are
frozen from touch-down to release, avoiding wrong-device selection when new
advertisements arrive. Scrolling has bounded offsets and a scroll indicator;
there is no inertial fling. Power/ride/map overlays cancel captured gestures.

All scan/connect/forget requests are queued to the BLE task. Scan callbacks
only record discoveries: no persistence, connection or SD work. CSC, HR and
power are persisted after successful subscription; saved devices reconnect.
Pairing here means saving a GATT sensor association, not requiring BLE bonding.

Flutter's builder preview now shows the paired-only empty layout; the phone's
own OpenCyclo device-discovery flow is unchanged. Camera controls, map gestures,
GPX recording/export and GPS filtering are not altered by this UI change.

Tests: native catalog/scroll, production widget touch/rendering, production CSC
success/failure persistence with fake GATT, camera/page regressions, Flutter
preview tests and firmware build. Not flashed or verified on physical sensors.

Reference for the power characteristic:
https://www.bluetooth.com/specifications/specs/cycling-power-service/
