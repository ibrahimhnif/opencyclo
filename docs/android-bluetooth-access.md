# Android Bluetooth access

The Device tab requests access after its first frame without automatically
scanning or connecting. Every scan/connect rechecks permission and radio state.
Returning from Settings refreshes access without reopening dialogs.

- Android 12+: runtime Nearby devices (SCAN + CONNECT), no location permission.
  Scan results are not used to derive the phone's physical location.
- Android 6–11: runtime fine location plus Location services for BLE discovery.
- Bluetooth off: FlutterBluePlus opens Android's enable dialog; cancellation
  leaves scanning disabled. No silent radio enable.
- Denied: explicit retry. Permanently denied: app Settings. No repeated dialog loop.
- A native failure or unknown status fails closed, with retry available.

Native implementation: MainActivity channel `opencyclo/ble_access`. Request
history is per permission so an OS upgrade does not mark new permissions blocked.
No additional plugins or firmware changes are needed.

## Device verification

Stop `flutter run` and run it again: manifest/Kotlin changes require a rebuild
and reinstall, not hot reload/restart. No uninstall or data clearing is needed.

On a fresh install, verify Nearby devices is requested before scanning. Test
Allow, Deny, repeated denial/Settings recovery, radio off/enable/cancel, and
returning from Settings. After allowing access, scan, connect, and sync a GPX.
Also verify Android 6–11 location permission/service requirements on an older
device. Automated Dart tests and APK compilation do not replace these OS tests.

Reference: https://developer.android.com/develop/connectivity/bluetooth/bt-permissions
