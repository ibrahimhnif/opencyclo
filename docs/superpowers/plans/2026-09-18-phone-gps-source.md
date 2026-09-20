# Configurable GPS Source (Hardware M10 / Phone) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let the connected phone's own GPS position feed the firmware's GPS pipeline over the existing BLE GATT connection, selectable/auto-falling-back per a new `gps_source_mode` setting, toggleable from both the app and the device's own menu.

**Architecture:** A new pure, hardware-free C++ header (`gps_source_arbiter.h/.cpp`) decides which source (M10 vs phone) feeds `g_gps_queue` each epoch and computes phone-derived speed from position deltas; `gps_task.cpp` is the only stateful caller. Two new BLE characteristics (`0x190A` write-only for phone position, `0x190B` read/write/notify for the mode) extend the existing `0x1900` GATT service. On the Flutter side, `geolocator` supplies phone position, written to `0x190A` roughly once per second while phone mode is active; a foreground service (Android) / background modes (iOS) keep this running with the screen off.

**Tech Stack:** ESP32-S3 / PlatformIO / Arduino framework / NimBLE-Arduino / Unity (native test env) — Flutter / Riverpod / flutter_blue_plus / geolocator 14.0.3 / flutter_foreground_task 11.0.3.

**Spec:** `docs/superpowers/specs/2026-09-18-phone-gps-source-design.md`

## Global Constraints

- Transport is the existing BLE GATT connection (characteristic write), never BLE advertising — iOS restricts background peripheral advertising to an overflow UUID, making advertising unreliable exactly when the phone is locked/backgrounded during a ride.
- `PHONE_FIX_STALE_MS = 5000` — a phone fix older than this is treated as absent.
- Phone payload on `0x190A` is exactly 11 bytes: `int32 lat_e7, int32 lon_e7, uint16 accuracy_cm, uint8 seq` (all little-endian). It carries no speed field; phone-derived speed is computed firmware-side from consecutive position deltas.
- `0x190B` payload is 1 byte: `0x00 = HARDWARE` (default, auto-fallback to phone), `0x01 = PHONE_FORCED`.
- New firmware logic that can be pure (no `Arduino.h`/FreeRTOS/NimBLE dependency) must be written that way and added to `platformio.ini`'s `[env:native]` `build_src_filter`, per this repo's only unit-testing convention — `gps_task.cpp`, `ble_task.cpp`, `ble_layout_sync.cpp`, and `settings.cpp` themselves stay untested (no hardware-mocking infrastructure exists in this repo).
- New Flutter logic depending on `flutter_blue_plus`/`geolocator` platform channels must be tested via constructor-injected fakes (the existing `BleNotifier(accessGate: fakeGate)` pattern in `app/test/ble_access_test.dart`), not `ProviderContainer` overrides — no precedent for that exists here.

---

### Task 1: Pure GPS source arbiter (firmware)

**Files:**
- Create: `src/hardware/gps_source_arbiter.h`
- Create: `src/hardware/gps_source_arbiter.cpp`
- Create: `test/test_gps_source_arbiter/test_gps_source_arbiter.cpp`
- Modify: `platformio.ini` (`[env:native]` `build_src_filter`)

**Interfaces:**
- Produces: `enum GpsSourceMode : uint8_t { GPS_SOURCE_MODE_HARDWARE = 0, GPS_SOURCE_MODE_PHONE_FORCED = 1 };`
- Produces: `enum GpsFixSource : uint8_t { GPS_FIX_SOURCE_HARDWARE = 0, GPS_FIX_SOURCE_PHONE_FALLBACK = 1, GPS_FIX_SOURCE_PHONE = 2, GPS_FIX_SOURCE_NONE = 3 };`
- Produces: `const uint32_t PHONE_FIX_STALE_MS = 5000;`
- Produces: `GpsFixSource selectGpsSource(GpsSourceMode mode, bool hardwareValid, bool phonePresent, uint32_t phoneAgeMs);`
- Produces: `struct PhoneSpeedResult { bool speedValid; float speedKmh; };`
- Produces: `PhoneSpeedResult computePhoneSpeedKmh(double prevLat, double prevLon, uint32_t prevAtMs, double lat, double lon, uint32_t atMs);`
- Consumes: nothing (pure, no project headers included).

- [ ] **Step 1: Write the failing native test**

Create `test/test_gps_source_arbiter/test_gps_source_arbiter.cpp`:

```cpp
#include <unity.h>
#include "../../src/hardware/gps_source_arbiter.h"

void test_hardware_mode_uses_hardware_when_valid() {
  const GpsFixSource s = selectGpsSource(GPS_SOURCE_MODE_HARDWARE, true, true, 0);
  TEST_ASSERT_EQUAL(GPS_FIX_SOURCE_HARDWARE, s);
}

void test_hardware_mode_falls_back_to_phone_when_hardware_invalid_and_phone_fresh() {
  const GpsFixSource s = selectGpsSource(GPS_SOURCE_MODE_HARDWARE, false, true, 1000);
  TEST_ASSERT_EQUAL(GPS_FIX_SOURCE_PHONE_FALLBACK, s);
}

void test_hardware_mode_ignores_stale_phone_fix() {
  const GpsFixSource s = selectGpsSource(GPS_SOURCE_MODE_HARDWARE, false, true, PHONE_FIX_STALE_MS);
  TEST_ASSERT_EQUAL(GPS_FIX_SOURCE_NONE, s);
}

void test_hardware_mode_no_fix_when_both_unavailable() {
  const GpsFixSource s = selectGpsSource(GPS_SOURCE_MODE_HARDWARE, false, false, 0);
  TEST_ASSERT_EQUAL(GPS_FIX_SOURCE_NONE, s);
}

void test_phone_forced_mode_uses_phone_ignoring_valid_hardware() {
  const GpsFixSource s = selectGpsSource(GPS_SOURCE_MODE_PHONE_FORCED, true, true, 500);
  TEST_ASSERT_EQUAL(GPS_FIX_SOURCE_PHONE, s);
}

void test_phone_forced_mode_no_fix_when_phone_stale_even_if_hardware_valid() {
  const GpsFixSource s = selectGpsSource(GPS_SOURCE_MODE_PHONE_FORCED, true, true, PHONE_FIX_STALE_MS + 1);
  TEST_ASSERT_EQUAL(GPS_FIX_SOURCE_NONE, s);
}

void test_phone_speed_computed_from_consecutive_samples() {
  // ~111m north in 10s (~0.001 deg latitude) -> ~40 km/h.
  const PhoneSpeedResult r = computePhoneSpeedKmh(0.0, 0.0, 1000, 0.001, 0.0, 11000);
  TEST_ASSERT_TRUE(r.speedValid);
  TEST_ASSERT_FLOAT_WITHIN(5.0f, 40.0f, r.speedKmh);
}

void test_phone_speed_invalid_when_timestamp_does_not_advance() {
  const PhoneSpeedResult r = computePhoneSpeedKmh(0.0, 0.0, 1000, 0.001, 0.0, 1000);
  TEST_ASSERT_FALSE(r.speedValid);
}

void test_phone_speed_invalid_when_gap_too_large() {
  const PhoneSpeedResult r = computePhoneSpeedKmh(0.0, 0.0, 1000, 0.001, 0.0, 20000);
  TEST_ASSERT_FALSE(r.speedValid);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_hardware_mode_uses_hardware_when_valid);
  RUN_TEST(test_hardware_mode_falls_back_to_phone_when_hardware_invalid_and_phone_fresh);
  RUN_TEST(test_hardware_mode_ignores_stale_phone_fix);
  RUN_TEST(test_hardware_mode_no_fix_when_both_unavailable);
  RUN_TEST(test_phone_forced_mode_uses_phone_ignoring_valid_hardware);
  RUN_TEST(test_phone_forced_mode_no_fix_when_phone_stale_even_if_hardware_valid);
  RUN_TEST(test_phone_speed_computed_from_consecutive_samples);
  RUN_TEST(test_phone_speed_invalid_when_timestamp_does_not_advance);
  RUN_TEST(test_phone_speed_invalid_when_gap_too_large);
  return UNITY_END();
}
```

- [ ] **Step 2: Create the (not yet existing) header so the test fails to compile for the right reason**

Create `src/hardware/gps_source_arbiter.h`:

```cpp
#ifndef OPENCYCLO_HARDWARE_GPS_SOURCE_ARBITER_H
#define OPENCYCLO_HARDWARE_GPS_SOURCE_ARBITER_H

#include <stdint.h>

// Which GPS source the user (via app or device menu) has selected.
enum GpsSourceMode : uint8_t {
  GPS_SOURCE_MODE_HARDWARE = 0,      // M10, auto-fallback to phone when no hardware fix
  GPS_SOURCE_MODE_PHONE_FORCED = 1,  // always phone; M10 ignored entirely
};

// Which source actually fed g_gps_queue this epoch (diagnostics + arbitration result).
enum GpsFixSource : uint8_t {
  GPS_FIX_SOURCE_HARDWARE = 0,
  GPS_FIX_SOURCE_PHONE_FALLBACK = 1,  // hardware mode, M10 has no fix, phone substituted
  GPS_FIX_SOURCE_PHONE = 2,           // phone-forced mode
  GPS_FIX_SOURCE_NONE = 3,            // no usable source this epoch
};

// A phone fix older than this (received over BLE characteristic 0x190A) is
// treated as absent -- covers BLE disconnects or the app being killed.
const uint32_t PHONE_FIX_STALE_MS = 5000;

// Pure decision: which source should feed the GPS queue this epoch.
// `phoneAgeMs` is ignored when `phonePresent` is false.
GpsFixSource selectGpsSource(GpsSourceMode mode, bool hardwareValid,
                              bool phonePresent, uint32_t phoneAgeMs);

struct PhoneSpeedResult {
  bool speedValid;
  float speedKmh;
};

// Speed-from-displacement for a phone-derived fix, since the 0x190A payload
// carries no speed field. `speedValid` is false when there's no usable
// previous sample, the clock didn't advance, or the gap is too large for a
// meaningful instantaneous speed (>= 10s).
PhoneSpeedResult computePhoneSpeedKmh(double prevLat, double prevLon, uint32_t prevAtMs,
                                       double lat, double lon, uint32_t atMs);

#endif  // OPENCYCLO_HARDWARE_GPS_SOURCE_ARBITER_H
```

- [ ] **Step 3: Wire the new test into the native build and run it to see it fail (link error: undefined reference)**

Edit `platformio.ini`'s `[env:native]` section — add the arbiter source to `build_src_filter`:

```ini
build_src_filter =
    -<*>
    +<ui/engine/widget_catalog.cpp>
    +<ui/engine/ui_config.cpp>
    +<ui/engine/template_engine.cpp>
    +<hardware/gps_source_arbiter.cpp>
```

Run: `pio test -e native -f test_gps_source_arbiter`
Expected: FAIL — link error, `selectGpsSource`/`computePhoneSpeedKmh` undefined (the `.cpp` doesn't exist yet).

- [ ] **Step 4: Implement the arbiter**

Create `src/hardware/gps_source_arbiter.cpp`:

```cpp
#include "gps_source_arbiter.h"
#include <cmath>
#include <algorithm>

static double haversineMeters(double lat1, double lon1, double lat2, double lon2) {
  const double r = 3.141592653589793 / 180.0;
  const double x = std::sin((lat2 - lat1) * r / 2);
  const double y = std::sin((lon2 - lon1) * r / 2);
  const double h = x * x + std::cos(lat1 * r) * std::cos(lat2 * r) * y * y;
  return 12742000.0 * std::asin(std::sqrt(std::min(1.0, std::max(0.0, h))));
}

GpsFixSource selectGpsSource(GpsSourceMode mode, bool hardwareValid,
                              bool phonePresent, uint32_t phoneAgeMs) {
  const bool phoneFresh = phonePresent && phoneAgeMs < PHONE_FIX_STALE_MS;
  if (mode == GPS_SOURCE_MODE_PHONE_FORCED) {
    return phoneFresh ? GPS_FIX_SOURCE_PHONE : GPS_FIX_SOURCE_NONE;
  }
  if (hardwareValid) return GPS_FIX_SOURCE_HARDWARE;
  return phoneFresh ? GPS_FIX_SOURCE_PHONE_FALLBACK : GPS_FIX_SOURCE_NONE;
}

PhoneSpeedResult computePhoneSpeedKmh(double prevLat, double prevLon, uint32_t prevAtMs,
                                       double lat, double lon, uint32_t atMs) {
  PhoneSpeedResult out{false, 0.0f};
  if (atMs <= prevAtMs) return out;
  const uint32_t dtMs = atMs - prevAtMs;
  if (dtMs >= 10000) return out;  // gap too large for a meaningful instantaneous speed
  const double meters = haversineMeters(prevLat, prevLon, lat, lon);
  const float speedKmh = (float)(meters / (dtMs / 1000.0) * 3.6);
  if (!std::isfinite(speedKmh) || speedKmh < 0 || speedKmh > 120) return out;
  out.speedValid = true;
  out.speedKmh = speedKmh;
  return out;
}
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `pio test -e native -f test_gps_source_arbiter`
Expected: PASS — 9/9 assertions.

- [ ] **Step 6: Commit**

```bash
git add src/hardware/gps_source_arbiter.h src/hardware/gps_source_arbiter.cpp test/test_gps_source_arbiter/test_gps_source_arbiter.cpp platformio.ini
git commit -m "feat(gps): add pure GPS source arbitration logic"
```

---

### Task 2: Settings persistence for `gps_source_mode` (firmware)

**Files:**
- Modify: `src/storage/settings.h`
- Modify: `src/storage/settings.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `g_settings.gps_source_mode` (uint8_t, 0 = hardware default), persisted under NVS key `"gps_src"`.

This field isn't unit-testable in isolation (settings.cpp depends on Arduino `Preferences`, excluded from the native env per the Global Constraints) — verified instead by the end-to-end check in Task 13.

- [ ] **Step 1: Add the field to the Settings struct**

Edit `src/storage/settings.h`, after `bool sd_logging_enabled;`:

```cpp
  bool sd_logging_enabled;
  // 0 = HARDWARE (M10, auto-fallback to phone when no fix), 1 = PHONE_FORCED
  // (always use the phone-supplied position; M10 ignored). See
  // hardware/gps_source_arbiter.h for GpsSourceMode.
  uint8_t gps_source_mode;
```

- [ ] **Step 2: Load and save it in settings.cpp**

Edit `src/storage/settings.cpp`, in `initSettings()` right after the `sd_logging_enabled` line:

```cpp
  g_settings.sd_logging_enabled = prefs.getBool("sdlog", true);
  g_settings.gps_source_mode = prefs.getUChar("gps_src", 0);
```

And in `saveSettings()` right after the `sd_logging_enabled` line:

```cpp
  prefs.putBool("sdlog", g_settings.sd_logging_enabled);
  prefs.putUChar("gps_src", g_settings.gps_source_mode);
```

- [ ] **Step 3: Build the native env to confirm nothing else regressed**

Run: `pio test -e native`
Expected: PASS (all existing native suites unaffected; `settings.cpp` isn't part of this env).

- [ ] **Step 4: Commit**

```bash
git add src/storage/settings.h src/storage/settings.cpp
git commit -m "feat(settings): persist gps_source_mode in NVS"
```

---

### Task 3: `GpsFix.source` diagnostics field (firmware)

**Files:**
- Modify: `src/core/telemetry_state.h`

**Interfaces:**
- Produces: `GpsFix.source` (uint8_t, holds a `GpsFixSource` value from Task 1 — plain `uint8_t` here rather than the enum type, so this header keeps no dependency on `gps_source_arbiter.h`).

- [ ] **Step 1: Add the field**

Edit `src/core/telemetry_state.h`, in `struct GpsFix`, after `uint32_t ageMs;`:

```cpp
  uint32_t ageMs;
  // Which source produced this fix this epoch: see GpsFixSource in
  // hardware/gps_source_arbiter.h (0=hardware, 1=phone-fallback, 2=phone,
  // 3=none). Diagnostics only -- FusionTask keeps consuming lat/lon/speed
  // identically regardless of source.
  uint8_t source;
```

- [ ] **Step 2: Build the firmware env to confirm it still compiles**

Run: `pio run -e esp32-s3-devkitc-1` (use this repo's actual firmware env name from `platformio.ini` if it differs)
Expected: PASS — no compile errors (adding a trailing struct field is source-compatible with all existing `GpsFix{}`/aggregate-init call sites).

- [ ] **Step 3: Commit**

```bash
git add src/core/telemetry_state.h
git commit -m "feat(gps): add source field to GpsFix for diagnostics"
```

---

### Task 4: Phone position queue + arbitration integration (firmware)

**Files:**
- Modify: `src/hardware/gps_task.h`
- Modify: `src/hardware/gps_task.cpp`

**Interfaces:**
- Consumes: `selectGpsSource`, `computePhoneSpeedKmh`, `GpsSourceMode`, `GpsFixSource`, `PHONE_FIX_STALE_MS` (Task 1); `g_settings.gps_source_mode` (Task 2); `GpsFix.source` (Task 3).
- Produces: `extern QueueHandle_t g_phone_gps_queue;`, `void setPhoneGpsSample(double lat, double lon, float accuracyM);` — called by the BLE write handler added in Task 5.

Not independently unit-testable (FreeRTOS/UART dependent, excluded from native env) — the pure decision logic it calls was already verified in Task 1; this task is verified end-to-end in Task 13.

- [ ] **Step 1: Declare the new queue and setter in the header**

Edit `src/hardware/gps_task.h`:

```cpp
extern QueueHandle_t g_gps_queue;
extern QueueHandle_t g_phone_gps_queue;

void startGpsTask();
void gpsTaskLoop(void* pvParameters);
bool prepareGpsForPowerOff();
void gpsAssistanceCommand(const uint8_t* bytes, size_t length);
void gpsAssistanceStatus(uint8_t out[12]);
void gpsIdentityCommand(const uint8_t* bytes, size_t length);
size_t gpsIdentityRead(uint8_t out[20]);
// Called from the 0x190A BLE write handler (ble_layout_sync.cpp) whenever
// the app reports a new phone position. Non-blocking, overwrite semantics --
// only the latest sample matters.
void setPhoneGpsSample(double lat, double lon, float accuracyM);
```

- [ ] **Step 2: Add includes, the queue, the setter, and per-loop phone-position tracking state**

Edit `src/hardware/gps_task.cpp` — add includes near the top (after the existing `#include "gps_identity.h"`):

```cpp
#include "gps_identity.h"
#include "gps_source_arbiter.h"
#include "storage/settings.h"
```

Add the queue definition next to `g_gps_queue`:

```cpp
QueueHandle_t g_gps_queue = NULL;
QueueHandle_t g_phone_gps_queue = NULL;

struct PhoneGpsSample {
  double latitude;
  double longitude;
  float accuracyM;
  uint32_t receivedAtMs;
};

void setPhoneGpsSample(double lat, double lon, float accuracyM) {
  if (g_phone_gps_queue == NULL) return;
  PhoneGpsSample sample{lat, lon, accuracyM, millis()};
  xQueueOverwrite(g_phone_gps_queue, &sample);
}

// Tracks the previous phone sample actually used to derive a phone-sourced
// GpsFix, so computePhoneSpeedKmh() has a delta to work with across loop
// iterations. Lives here (not in the pure arbiter) because it's stateful.
static bool havePreviousPhoneSample = false;
static double previousPhoneLat = 0, previousPhoneLon = 0;
static uint32_t previousPhoneAtMs = 0;
```

- [ ] **Step 3: Create the queue in `startGpsTask()`**

Edit `src/hardware/gps_task.cpp`, in `startGpsTask()`:

```cpp
void startGpsTask() {
  if (g_gps_queue == NULL) {
    g_gps_queue = xQueueCreate(1, sizeof(GpsFix));
  }
  if (g_phone_gps_queue == NULL) {
    g_phone_gps_queue = xQueueCreate(1, sizeof(PhoneGpsSample));
  }
  ...
```
(leave the rest of the function, `xTaskCreatePinnedToCore(...)`, unchanged)

- [ ] **Step 4: Replace the push block in `gpsTaskLoop()` with arbitration**

Edit `src/hardware/gps_task.cpp` — replace this existing block:

```cpp
    // Send GPS fix status to queue
    if (now - lastPushMs >= 500 || changed) {
      lastPushMs = now;
      const GpsFix raw=gps.snapshot(now);
      GpsFix fix=gpsFilter.apply(raw,now);
      captureGpsDiagnostic(raw,fix,gpsFilter.rejectionReason(),now);

      if (g_gps_queue != NULL) {
        xQueueOverwrite(g_gps_queue, &fix);
      }
    }
```

with:

```cpp
    // Send GPS fix status to queue
    if (now - lastPushMs >= 500 || changed) {
      lastPushMs = now;
      const GpsFix raw=gps.snapshot(now);
      GpsFix fix=gpsFilter.apply(raw,now);
      captureGpsDiagnostic(raw,fix,gpsFilter.rejectionReason(),now);

      PhoneGpsSample phone{};
      const bool havePhone = g_phone_gps_queue != NULL &&
        xQueuePeek(g_phone_gps_queue, &phone, 0) == pdTRUE;
      const uint32_t phoneAgeMs = havePhone ? now - phone.receivedAtMs : UINT32_MAX;
      const GpsFixSource source = selectGpsSource(
        (GpsSourceMode)g_settings.gps_source_mode, fix.isValid, havePhone, phoneAgeMs);

      GpsFix outFix{};
      if (source == GPS_FIX_SOURCE_HARDWARE) {
        outFix = fix;
        outFix.source = GPS_FIX_SOURCE_HARDWARE;
      } else if (source == GPS_FIX_SOURCE_PHONE || source == GPS_FIX_SOURCE_PHONE_FALLBACK) {
        outFix.isValid = true;
        outFix.accuracyValid = true;
        outFix.latitude = phone.latitude;
        outFix.longitude = phone.longitude;
        outFix.horizontalAccuracyM = phone.accuracyM;
        outFix.quality = 2;
        outFix.receivedAtMs = now;
        outFix.source = (uint8_t)source;
        if (havePreviousPhoneSample) {
          const PhoneSpeedResult speed = computePhoneSpeedKmh(
            previousPhoneLat, previousPhoneLon, previousPhoneAtMs,
            phone.latitude, phone.longitude, phone.receivedAtMs);
          outFix.speedValid = speed.speedValid;
          outFix.speedKmh = speed.speedKmh;
        }
        if (!havePreviousPhoneSample || phone.receivedAtMs != previousPhoneAtMs) {
          previousPhoneLat = phone.latitude;
          previousPhoneLon = phone.longitude;
          previousPhoneAtMs = phone.receivedAtMs;
          havePreviousPhoneSample = true;
        }
      } else {
        outFix.receivedAtMs = now;
        outFix.source = GPS_FIX_SOURCE_NONE;
      }

      if (g_gps_queue != NULL) {
        xQueueOverwrite(g_gps_queue, &outFix);
      }
    }
```

- [ ] **Step 5: Build the firmware env**

Run: `pio run -e esp32-s3-devkitc-1` (match the actual env name in `platformio.ini`)
Expected: PASS — no compile errors.

- [ ] **Step 6: Commit**

```bash
git add src/hardware/gps_task.h src/hardware/gps_task.cpp
git commit -m "feat(gps): arbitrate hardware vs phone GPS source into g_gps_queue"
```

---

### Task 5: BLE GATT characteristics 0x190A / 0x190B (firmware)

**Files:**
- Modify: `src/hardware/ble_layout_sync.h`
- Modify: `src/hardware/ble_layout_sync.cpp`

**Interfaces:**
- Consumes: `setPhoneGpsSample()` (Task 4), `g_settings.gps_source_mode` + `saveSettings()` (Task 2).
- Produces: `void setGpsSourceMode(uint8_t mode);` — called by the device-side settings menu in Task 6.

Not independently unit-testable (NimBLE-dependent, excluded from native env) — verified end-to-end in Task 13.

- [ ] **Step 1: Add the UUIDs and `setGpsSourceMode` declaration to the header**

Edit `src/hardware/ble_layout_sync.h`:

```cpp
#define BLE_OPENCYCLO_SERVICE_UUID     "00001900-0000-1000-8000-00805F9B34FB"
#define BLE_LAYOUT_CONFIG_CHAR_UUID    "00001901-0000-1000-8000-00805F9B34FB"
#define BLE_TELEMETRY_STREAM_CHAR_UUID "00001902-0000-1000-8000-00805F9B34FB"
#define BLE_DEVICE_COMMAND_CHAR_UUID   "00001903-0000-1000-8000-00805F9B34FB"
#define BLE_PHONE_GPS_CHAR_UUID        "0000190A-0000-1000-8000-00805F9B34FB"
#define BLE_GPS_SOURCE_MODE_CHAR_UUID  "0000190B-0000-1000-8000-00805F9B34FB"

void initBleLayoutSyncService(NimBLEServer* pServer);
void notifyBleTelemetry(const TelemetryState& state);
// Persists mode, notifies subscribed clients on 0x190B. Called from the
// 0x190B write handler and from the device's own settings menu.
void setGpsSourceMode(uint8_t mode);
```

- [ ] **Step 2: Add includes, callbacks, and `setGpsSourceMode` to the .cpp**

Edit `src/hardware/ble_layout_sync.cpp` — add an include near the top:

```cpp
#include "ble_layout_sync.h"
#include "storage/layout_config.h"
#include "storage/settings.h"
#include "core/telemetry_state.h"
```

Add the characteristic pointer, callbacks, and `setGpsSourceMode` near the other callback classes (after `GpsCacheCallbacks`, before `initBleLayoutSyncService`):

```cpp
static NimBLECharacteristic* pGpsSourceModeChar = nullptr;

class PhoneGpsCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c) override {
    const std::string v = c->getValue();
    if (v.size() < 11) return;
    int32_t latE7, lonE7;
    uint16_t accCm;
    memcpy(&latE7, v.data() + 0, 4);
    memcpy(&lonE7, v.data() + 4, 4);
    memcpy(&accCm, v.data() + 8, 2);
    // Byte 10 is a sequence number, informational only (no reassembly needed).
    setPhoneGpsSample(latE7 / 1e7, lonE7 / 1e7, accCm / 100.0f);
  }
};

void setGpsSourceMode(uint8_t mode) {
  g_settings.gps_source_mode = mode;
  saveSettings();
  if (pGpsSourceModeChar != nullptr) {
    pGpsSourceModeChar->setValue(&g_settings.gps_source_mode, 1);
    pGpsSourceModeChar->notify();
  }
}

class GpsSourceModeCallbacks : public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* c) override {
    c->setValue(&g_settings.gps_source_mode, 1);
  }
  void onWrite(NimBLECharacteristic* c) override {
    const std::string v = c->getValue();
    if (v.empty()) return;
    setGpsSourceMode((uint8_t)v[0]);
  }
};
```

- [ ] **Step 3: Register both characteristics in `initBleLayoutSyncService()`**

Edit `src/hardware/ble_layout_sync.cpp`, right after the existing `cacheChar` registration and before `pService->start();`:

```cpp
  auto* cacheChar=pService->createCharacteristic(
    "00001909-0000-1000-8000-00805F9B34FB",NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  cacheChar->setCallbacks(new GpsCacheCallbacks());

  auto* phoneGpsChar = pService->createCharacteristic(
    BLE_PHONE_GPS_CHAR_UUID, NIMBLE_PROPERTY::WRITE_NR);
  phoneGpsChar->setCallbacks(new PhoneGpsCallbacks());

  pGpsSourceModeChar = pService->createCharacteristic(
    BLE_GPS_SOURCE_MODE_CHAR_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  pGpsSourceModeChar->setValue(&g_settings.gps_source_mode, 1);
  pGpsSourceModeChar->setCallbacks(new GpsSourceModeCallbacks());

  pService->start();
```

- [ ] **Step 4: Build the firmware env**

Run: `pio run -e esp32-s3-devkitc-1` (match the actual env name in `platformio.ini`)
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/hardware/ble_layout_sync.h src/hardware/ble_layout_sync.cpp
git commit -m "feat(ble): add phone GPS position and GPS source mode characteristics"
```

---

### Task 6: Device-side settings menu toggle (firmware)

**Files:**
- Modify: `src/ui/engine/widget_registry.cpp`

**Interfaces:**
- Consumes: `setGpsSourceMode()` (Task 5), `g_settings.gps_source_mode` (Task 2).

Not unit-testable (rendering/touch-hit-testing code, no test harness exists for this file in the repo) — verified visually on hardware in Task 13.

- [ ] **Step 1: Add the include**

Edit `src/ui/engine/widget_registry.cpp`, add to the existing include block:

```cpp
#include "hardware/ble_task.h"
#include "hardware/ble_layout_sync.h"
#include "hardware/ble_camera_remote.h"
```

- [ ] **Step 2: Render the new row**

Edit `src/ui/engine/widget_registry.cpp` — insert right after the existing `row("sd logging", ...)` call and before the `battery` row:

```cpp
  row("sd logging", g_settings.sd_logging_enabled ? "enabled" : "disabled",
      g_settings.sd_logging_enabled ? COLOR_GREEN : COLOR_AMBER);

  row("gps source", g_settings.gps_source_mode == 0 ? "hardware" : "phone",
      g_settings.gps_source_mode == 0 ? COLOR_TEXT : COLOR_CYAN);

  char batStr[24];
```

- [ ] **Step 3: Make the new row touchable**

Edit `src/ui/engine/widget_registry.cpp`, in `touchWidgetSettingsList()` — insert right after the existing `if (settingsRowHit(b, y, 3))` (sd logging) block:

```cpp
  if (settingsRowHit(b, y, 3)) { // sd logging
    g_settings.sd_logging_enabled = !g_settings.sd_logging_enabled;
    saveSettings();
    return true;
  }
  if (settingsRowHit(b, y, 4)) { // gps source
    setGpsSourceMode(g_settings.gps_source_mode == 0 ? 1 : 0);
    return true;
  }
  return false;
```

- [ ] **Step 4: Build the firmware env**

Run: `pio run -e esp32-s3-devkitc-1` (match the actual env name in `platformio.ini`)
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/ui/engine/widget_registry.cpp
git commit -m "feat(ui): add GPS source toggle to the device settings menu"
```

---

### Task 7: App dependencies and platform permissions

**Files:**
- Modify: `app/pubspec.yaml`
- Modify: `app/android/app/src/main/AndroidManifest.xml`
- Modify: `app/ios/Runner/Info.plist`

**Interfaces:**
- Produces: `geolocator` and `flutter_foreground_task` packages available to later tasks.

Not independently testable (dependency/manifest/plist config) — verified by `flutter pub get` succeeding and the app building.

- [ ] **Step 1: Add dependencies**

Edit `app/pubspec.yaml`, in `dependencies:`:

```yaml
  flutter_blue_plus: ^1.35.0
  flutter_riverpod: ^2.6.1
  geolocator: ^14.0.3
  flutter_foreground_task: ^11.0.3
```

- [ ] **Step 2: Run `flutter pub get`**

Run: `cd app && flutter pub get`
Expected: resolves successfully, `pubspec.lock` updated.

- [ ] **Step 3: Update AndroidManifest.xml permissions**

Edit `app/android/app/src/main/AndroidManifest.xml` — the existing `ACCESS_FINE_LOCATION` line is capped `maxSdkVersion="30"` (added only for pre-Android-12 BLE scanning); geolocator needs it unconditionally, so drop the cap and add the new permissions:

```xml
    <uses-permission android:name="android.permission.INTERNET" />
    <uses-permission android:name="android.permission.BLUETOOTH" android:maxSdkVersion="30" />
    <uses-permission android:name="android.permission.BLUETOOTH_ADMIN" android:maxSdkVersion="30" />
    <uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
    <uses-permission android:name="android.permission.ACCESS_BACKGROUND_LOCATION" />
    <uses-permission android:name="android.permission.FOREGROUND_SERVICE" />
    <uses-permission android:name="android.permission.FOREGROUND_SERVICE_LOCATION" />
    <uses-permission android:name="android.permission.BLUETOOTH_SCAN" android:usesPermissionFlags="neverForLocation" />
    <uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />
    <uses-feature android:name="android.hardware.bluetooth_le" android:required="false" />
```

- [ ] **Step 4: Update iOS Info.plist**

Edit `app/ios/Runner/Info.plist`, add before the closing `</dict>`:

```xml
	<key>NSLocationWhenInUseUsageDescription</key>
	<string>opencyclo uses your location as an alternative GPS source for the connected bike computer when the onboard GPS module has a slow or weak fix.</string>
	<key>NSLocationAlwaysAndWhenInUseUsageDescription</key>
	<string>opencyclo needs Always location access to keep supplying your position to the connected bike computer while the app is in the background or the screen is locked during a ride.</string>
	<key>UIBackgroundModes</key>
	<array>
		<string>location</string>
		<string>bluetooth-central</string>
	</array>
```

- [ ] **Step 5: Build to confirm the manifest/plist changes are well-formed**

Run: `cd app && flutter build apk --debug`
Expected: PASS (build succeeds; confirms manifest merges cleanly).

- [ ] **Step 6: Commit**

```bash
git add app/pubspec.yaml app/pubspec.lock app/android/app/src/main/AndroidManifest.xml app/ios/Runner/Info.plist
git commit -m "feat(app): add geolocator and foreground-service dependencies and permissions"
```

---

### Task 8: BLE protocol constants + phone GPS payload encoder

**Files:**
- Modify: `app/lib/core/ble/ble_protocol.dart`
- Create: `app/test/ble_protocol_gps_test.dart`

**Interfaces:**
- Consumes: nothing new.
- Produces: `BleProtocol.phoneGpsCharUuid`, `BleProtocol.gpsSourceModeCharUuid`, `BleProtocol.gpsSourceHardware`, `BleProtocol.gpsSourcePhoneForced`, `Uint8List encodePhoneGpsSample(double lat, double lon, double accuracyM, int seq)`.

- [ ] **Step 1: Write the failing test**

Create `app/test/ble_protocol_gps_test.dart`:

```dart
import 'dart:typed_data';
import 'package:flutter_test/flutter_test.dart';
import 'package:opencyclo/core/ble/ble_protocol.dart';

void main() {
  test('encodePhoneGpsSample packs lat/lon/accuracy/seq into 11 bytes', () {
    final bytes = encodePhoneGpsSample(37.7749295, -122.4194155, 12.5, 42);
    expect(bytes.length, 11);

    final view = ByteData.sublistView(bytes);
    expect(view.getInt32(0, Endian.little) / 1e7, closeTo(37.7749295, 1e-6));
    expect(view.getInt32(4, Endian.little) / 1e7, closeTo(-122.4194155, 1e-6));
    expect(view.getUint16(8, Endian.little), 1250); // 12.5m -> centimeters
    expect(bytes[10], 42);
  });

  test('encodePhoneGpsSample clamps accuracy and wraps sequence to a byte', () {
    final bytes = encodePhoneGpsSample(0, 0, 1000.0, 300);
    final view = ByteData.sublistView(bytes);
    expect(view.getUint16(8, Endian.little), 65535); // clamped, not overflowed
    expect(bytes[10], 300 & 0xFF);
  });
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cd app && flutter test test/ble_protocol_gps_test.dart`
Expected: FAIL — `encodePhoneGpsSample` undefined.

- [ ] **Step 3: Implement the constants and encoder**

Edit `app/lib/core/ble/ble_protocol.dart`:

```dart
import 'dart:typed_data';

class BleProtocol {
  // Service UUIDs
  static const String devInfoServiceUuid = "180a";
  static const String openCycloServiceUuid = "00001900-0000-1000-8000-00805f9b34fb";
  static const String otaServiceUuid       = "00001910-0000-1000-8000-00805f9b34fb";

  // OpenCyclo Communication Characteristic UUIDs
  static const String layoutConfigCharUuid    = "00001901-0000-1000-8000-00805f9b34fb";
  static const String telemetryStreamCharUuid = "00001902-0000-1000-8000-00805f9b34fb";
  static const String deviceCommandCharUuid   = "00001903-0000-1000-8000-00805f9b34fb";
  static const String phoneGpsCharUuid        = "0000190a-0000-1000-8000-00805f9b34fb";
  static const String gpsSourceModeCharUuid   = "0000190b-0000-1000-8000-00805f9b34fb";

  // OTA Characteristic UUIDs
  static const String otaControlCharUuid = "00001911-0000-1000-8000-00805f9b34fb";
  static const String otaDataCharUuid    = "00001912-0000-1000-8000-00805f9b34fb";

  // Commands
  static const int cmdStartRide    = 0x01;
  static const int cmdPauseRide    = 0x02;
  static const int cmdResetDefault = 0x03;
  static const int cmdReboot       = 0x04;

  // OTA Commands
  static const int otaCmdBegin = 0x01;
  static const int otaCmdEnd   = 0x02;
  static const int otaCmdAbort = 0x03;

  // GPS Source Modes (0x190B payload)
  static const int gpsSourceHardware    = 0x00;
  static const int gpsSourcePhoneForced = 0x01;
}

/// Encodes a phone position for the 0x190A characteristic: 11 bytes,
/// little-endian `int32 lat_e7, int32 lon_e7, uint16 accuracy_cm, uint8 seq`.
Uint8List encodePhoneGpsSample(double lat, double lon, double accuracyM, int seq) {
  final bytes = ByteData(11);
  bytes.setInt32(0, (lat * 1e7).round(), Endian.little);
  bytes.setInt32(4, (lon * 1e7).round(), Endian.little);
  final accuracyCm = (accuracyM * 100).round().clamp(0, 65535);
  bytes.setUint16(8, accuracyCm, Endian.little);
  bytes.setUint8(10, seq & 0xFF);
  return bytes.buffer.asUint8List();
}
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `cd app && flutter test test/ble_protocol_gps_test.dart`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add app/lib/core/ble/ble_protocol.dart app/test/ble_protocol_gps_test.dart
git commit -m "feat(ble): add phone GPS UUIDs and payload encoder"
```

---

### Task 9: Phone GPS position service

**Files:**
- Create: `app/lib/core/location/phone_gps_service.dart`
- Create: `app/test/phone_gps_service_test.dart`

**Interfaces:**
- Consumes: `geolocator` package (Task 7).
- Produces: `class PhoneGpsSample { final double latitude, longitude, accuracyM; }`, `class PhoneGpsService { Stream<PhoneGpsSample> positions(); }` — injectable `positionStreamFactory` for testing.

- [ ] **Step 1: Write the failing test**

Create `app/test/phone_gps_service_test.dart`:

```dart
import 'package:flutter_test/flutter_test.dart';
import 'package:geolocator/geolocator.dart';
import 'package:opencyclo/core/location/phone_gps_service.dart';

Position _fakePosition(double lat, double lon, double accuracy) {
  return Position(
    latitude: lat,
    longitude: lon,
    timestamp: DateTime.now(),
    accuracy: accuracy,
    altitude: 0,
    altitudeAccuracy: 0,
    heading: 0,
    headingAccuracy: 0,
    speed: 0,
    speedAccuracy: 0,
  );
}

void main() {
  test('positions() maps Geolocator Position into PhoneGpsSample', () async {
    final service = PhoneGpsService(
      positionStreamFactory: () => Stream.fromIterable(
        [_fakePosition(37.7749, -122.4194, 8.0)],
      ),
    );

    final sample = await service.positions().first;
    expect(sample.latitude, 37.7749);
    expect(sample.longitude, -122.4194);
    expect(sample.accuracyM, 8.0);
  });
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cd app && flutter test test/phone_gps_service_test.dart`
Expected: FAIL — `PhoneGpsService` undefined.

- [ ] **Step 3: Implement the service**

Create `app/lib/core/location/phone_gps_service.dart`:

```dart
import 'package:geolocator/geolocator.dart';

typedef PositionStreamFactory = Stream<Position> Function();

class PhoneGpsSample {
  final double latitude;
  final double longitude;
  final double accuracyM;
  const PhoneGpsSample({
    required this.latitude,
    required this.longitude,
    required this.accuracyM,
  });
}

class PhoneGpsService {
  final PositionStreamFactory _positionStreamFactory;

  const PhoneGpsService({PositionStreamFactory? positionStreamFactory})
      : _positionStreamFactory = positionStreamFactory ?? _defaultPositionStream;

  static Stream<Position> _defaultPositionStream() {
    return Geolocator.getPositionStream(
      locationSettings: const LocationSettings(
        accuracy: LocationAccuracy.best,
        distanceFilter: 0,
      ),
    );
  }

  Stream<PhoneGpsSample> positions() {
    return _positionStreamFactory().map((p) => PhoneGpsSample(
          latitude: p.latitude,
          longitude: p.longitude,
          accuracyM: p.accuracy,
        ));
  }
}
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `cd app && flutter test test/phone_gps_service_test.dart`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add app/lib/core/location/phone_gps_service.dart app/test/phone_gps_service_test.dart
git commit -m "feat(location): add phone GPS position service"
```

---

### Task 10: `BleService` wiring for 0x190A / 0x190B

**Files:**
- Modify: `app/lib/core/ble/ble_service.dart`

**Interfaces:**
- Consumes: `encodePhoneGpsSample` (Task 8).
- Produces: `BleService.writePhoneGpsSample(double lat, double lon, double accuracyM, int seq)`, `BleService.readGpsSourceMode()`, `BleService.writeGpsSourceMode(int mode)`, `BleService.gpsSourceModeStream` (`Stream<int>`).

Not independently unit-testable (real `flutter_blue_plus` characteristics, no fakes exist in this repo for `BleService` — see Global Constraints) — the pure encoding it depends on is already tested in Task 8; this task's wiring is exercised through `GpsSourceNotifier`'s injected-fake tests in Task 11, and end-to-end in Task 13.

- [ ] **Step 1: Add characteristic fields and reset them on connect/disconnect**

Edit `app/lib/core/ble/ble_service.dart`, add fields:

```dart
  BluetoothCharacteristic? _gpsAssistanceChar;
  BluetoothCharacteristic? _gpsIdentityChar;
  BluetoothCharacteristic? _gpsCacheChar;
  BluetoothCharacteristic? _phoneGpsChar;
  BluetoothCharacteristic? _gpsSourceModeChar;
```

In `connect()`, add to the reset block at the top and to the disconnect-listener reset block:

```dart
      _gpsAssistanceChar = null;
      _gpsIdentityChar = null;
      _gpsCacheChar = null;
      _phoneGpsChar = null;
      _gpsSourceModeChar = null;
      _credentialKey = null;
```
(apply the same three added lines in both places it currently resets `_gpsCacheChar = null;`)

In `disconnect()`:

```dart
  Future<void> disconnect() async {
    _gpsAssistanceChar = null;
    _gpsIdentityChar = null;
    _gpsCacheChar = null;
    _phoneGpsChar = null;
    _gpsSourceModeChar = null;
    _credentialKey = null;
```

- [ ] **Step 2: Pick up the new characteristics during service discovery**

Edit `app/lib/core/ble/ble_service.dart`, in the `discoverServices()` loop, add to the `if (sUuid.contains("1900"))` branch's `else if` chain:

```dart
            } else if (uuid.contains("1909")) {
              _gpsCacheChar = char;
            } else if (uuid.contains("190a")) {
              _phoneGpsChar = char;
            } else if (uuid.contains("190b")) {
              _gpsSourceModeChar = char;
              await _subscribeGpsSourceMode(char);
            }
```

- [ ] **Step 3: Add the stream, subscribe helper, read/write methods**

Edit `app/lib/core/ble/ble_service.dart`, add near `telemetryStream`:

```dart
  final _gpsSourceModeController = StreamController<int>.broadcast();
  Stream<int> get gpsSourceModeStream => _gpsSourceModeController.stream;
```

Add near `_subscribeTelemetry`:

```dart
  Future<void> _subscribeGpsSourceMode(BluetoothCharacteristic char) async {
    await char.setNotifyValue(true);
    char.lastValueStream.listen((value) {
      if (value.isNotEmpty) _gpsSourceModeController.add(value[0]);
    });
  }
```

Add new public methods (near `sendCommand`):

```dart
  Future<void> writePhoneGpsSample(
      double lat, double lon, double accuracyM, int seq) async {
    final c = _phoneGpsChar;
    if (c == null) return;
    try {
      await c.write(encodePhoneGpsSample(lat, lon, accuracyM, seq),
          withoutResponse: true);
    } catch (e) {
      debugPrint("[BLE ERROR] Failed to write phone GPS sample: $e");
    }
  }

  Future<int?> readGpsSourceMode() async {
    final c = _gpsSourceModeChar;
    if (c == null) return null;
    try {
      final v = await c.read();
      return v.isNotEmpty ? v[0] : null;
    } catch (e) {
      debugPrint("[BLE ERROR] Failed to read GPS source mode: $e");
      return null;
    }
  }

  Future<void> writeGpsSourceMode(int mode) async {
    final c = _gpsSourceModeChar;
    if (c == null) return;
    try {
      await c.write([mode], withoutResponse: false);
    } catch (e) {
      debugPrint("[BLE ERROR] Failed to write GPS source mode: $e");
    }
  }
```

- [ ] **Step 4: Analyze and run the existing app test suite to confirm nothing regressed**

Run: `cd app && flutter test`
Expected: PASS (existing suite unaffected; this task adds no new test file since `BleService` isn't independently testable per Global Constraints).

- [ ] **Step 5: Commit**

```bash
git add app/lib/core/ble/ble_service.dart
git commit -m "feat(ble): wire phone GPS write and GPS source mode read/write/notify"
```

---

### Task 11: `GpsSourceProvider` (Riverpod state + orchestration)

**Files:**
- Create: `app/lib/state/gps_source_provider.dart`
- Create: `app/test/gps_source_provider_test.dart`

**Interfaces:**
- Consumes: `PhoneGpsService` (Task 9), `BleService` (Task 10, via a small injectable interface — see Step 1), `flutter_foreground_task`.
- Produces: `enum GpsSourceMode { hardware, phoneForced }`, `class GpsSourceState { GpsSourceMode mode; bool syncing; }`, `class GpsSourceNotifier extends StateNotifier<GpsSourceState>` with `loadFromDevice()` and `setMode(GpsSourceMode)`, `final gpsSourceProvider = StateNotifierProvider<GpsSourceNotifier, GpsSourceState>(...)`.

`BleService` is a concrete singleton, not designed for fakes (Global Constraints). Rather than fake the whole class, this task defines a minimal interface with only the 4 members `GpsSourceNotifier` needs, and makes `BleService` implement it — the same constructor-injection pattern as `BleNotifier(accessGate: fakeGate)`.

- [ ] **Step 1: Write the failing test**

Create `app/test/gps_source_provider_test.dart`:

```dart
import 'dart:async';
import 'package:flutter_test/flutter_test.dart';
import 'package:opencyclo/core/location/phone_gps_service.dart';
import 'package:opencyclo/state/gps_source_provider.dart';

class FakeGpsSourceBleChannel implements GpsSourceBleChannel {
  int? modeToReturnOnRead;
  final List<int> writtenModes = [];
  final List<List<num>> writtenSamples = [];
  final _controller = StreamController<int>.broadcast();

  @override
  Stream<int> get gpsSourceModeStream => _controller.stream;

  @override
  Future<int?> readGpsSourceMode() async => modeToReturnOnRead;

  @override
  Future<void> writeGpsSourceMode(int mode) async => writtenModes.add(mode);

  @override
  Future<void> writePhoneGpsSample(
      double lat, double lon, double accuracyM, int seq) async {
    writtenSamples.add([lat, lon, accuracyM, seq]);
  }

  void dispose() => _controller.close();
}

void main() {
  test('loadFromDevice reflects the mode read from the device', () async {
    final channel = FakeGpsSourceBleChannel()..modeToReturnOnRead = 1;
    final notifier = GpsSourceNotifier(
      bleChannel: channel,
      phoneGpsService: const PhoneGpsService(
          positionStreamFactory: _emptyPositionStream),
    );
    addTearDown(() {
      notifier.dispose();
      channel.dispose();
    });

    await notifier.loadFromDevice();
    expect(notifier.state.mode, GpsSourceMode.phoneForced);
  });

  test('setMode writes the mode byte to the device', () async {
    final channel = FakeGpsSourceBleChannel();
    final notifier = GpsSourceNotifier(
      bleChannel: channel,
      phoneGpsService: const PhoneGpsService(
          positionStreamFactory: _emptyPositionStream),
    );
    addTearDown(() {
      notifier.dispose();
      channel.dispose();
    });

    await notifier.setMode(GpsSourceMode.phoneForced);
    expect(channel.writtenModes, [1]);
    expect(notifier.state.mode, GpsSourceMode.phoneForced);
  });

  test('a device-side mode change (Notify) updates state', () async {
    final channel = FakeGpsSourceBleChannel();
    final notifier = GpsSourceNotifier(
      bleChannel: channel,
      phoneGpsService: const PhoneGpsService(
          positionStreamFactory: _emptyPositionStream),
    );
    addTearDown(() {
      notifier.dispose();
      channel.dispose();
    });

    channel._controller.add(1);
    await Future<void>.delayed(Duration.zero);
    expect(notifier.state.mode, GpsSourceMode.phoneForced);
  });
}

Stream<dynamic> _emptyPositionStream() => const Stream.empty();
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cd app && flutter test test/gps_source_provider_test.dart`
Expected: FAIL — `GpsSourceBleChannel`/`GpsSourceNotifier` undefined.

- [ ] **Step 3: Implement the interface, notifier, and provider**

Create `app/lib/state/gps_source_provider.dart`:

```dart
import 'dart:async';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_foreground_task/flutter_foreground_task.dart';
import '../core/ble/ble_service.dart';
import '../core/ble/ble_protocol.dart';
import '../core/location/phone_gps_service.dart';

/// The subset of BleService that GpsSourceNotifier needs -- lets tests
/// inject a fake without mocking the whole flutter_blue_plus-backed class.
abstract class GpsSourceBleChannel {
  Stream<int> get gpsSourceModeStream;
  Future<int?> readGpsSourceMode();
  Future<void> writeGpsSourceMode(int mode);
  Future<void> writePhoneGpsSample(
      double lat, double lon, double accuracyM, int seq);
}

enum GpsSourceMode { hardware, phoneForced }

GpsSourceMode gpsSourceModeFromByte(int b) =>
    b == BleProtocol.gpsSourcePhoneForced
        ? GpsSourceMode.phoneForced
        : GpsSourceMode.hardware;

int gpsSourceModeToByte(GpsSourceMode m) =>
    m == GpsSourceMode.phoneForced
        ? BleProtocol.gpsSourcePhoneForced
        : BleProtocol.gpsSourceHardware;

class GpsSourceState {
  final GpsSourceMode mode;
  final bool syncing;
  const GpsSourceState({
    this.mode = GpsSourceMode.hardware,
    this.syncing = false,
  });

  GpsSourceState copyWith({GpsSourceMode? mode, bool? syncing}) =>
      GpsSourceState(mode: mode ?? this.mode, syncing: syncing ?? this.syncing);
}

class GpsSourceNotifier extends StateNotifier<GpsSourceState> {
  final GpsSourceBleChannel _bleChannel;
  final PhoneGpsService _phoneGpsService;
  StreamSubscription<PhoneGpsSample>? _positionSub;
  StreamSubscription<int>? _modeSub;
  int _seq = 0;

  GpsSourceNotifier({
    GpsSourceBleChannel? bleChannel,
    PhoneGpsService? phoneGpsService,
  })  : _bleChannel = bleChannel ?? BleService.instance,
        _phoneGpsService = phoneGpsService ?? const PhoneGpsService(),
        super(const GpsSourceState()) {
    _modeSub = _bleChannel.gpsSourceModeStream.listen((byte) {
      state = state.copyWith(mode: gpsSourceModeFromByte(byte));
    });
  }

  Future<void> loadFromDevice() async {
    final byte = await _bleChannel.readGpsSourceMode();
    if (byte != null && mounted) {
      state = state.copyWith(mode: gpsSourceModeFromByte(byte));
    }
  }

  Future<void> setMode(GpsSourceMode mode) async {
    state = state.copyWith(mode: mode, syncing: true);
    await _bleChannel.writeGpsSourceMode(gpsSourceModeToByte(mode));
    if (mode == GpsSourceMode.phoneForced) {
      await _startStreamingPosition();
    } else {
      await _stopStreamingPosition();
    }
    if (mounted) state = state.copyWith(syncing: false);
  }

  Future<void> _startStreamingPosition() async {
    if (_positionSub != null) return;
    FlutterForegroundTask.init(
      androidNotificationOptions: AndroidNotificationOptions(
        channelId: 'gps_source_channel',
        channelName: 'Phone GPS Source',
        channelDescription:
            'Keeps phone GPS active while used as the opencyclo GPS source.',
      ),
      iosNotificationOptions: const IOSNotificationOptions(),
      foregroundTaskOptions: ForegroundTaskOptions(
        eventAction: ForegroundTaskEventAction.nothing(),
        autoRunOnBoot: false,
        allowWakeLock: true,
      ),
    );
    await FlutterForegroundTask.startService(
      notificationTitle: 'opencyclo',
      notificationText: 'using phone GPS as location source',
    );
    _positionSub = _phoneGpsService.positions().listen((sample) {
      _seq = (_seq + 1) & 0xFF;
      _bleChannel.writePhoneGpsSample(
          sample.latitude, sample.longitude, sample.accuracyM, _seq);
    });
  }

  Future<void> _stopStreamingPosition() async {
    await _positionSub?.cancel();
    _positionSub = null;
    await FlutterForegroundTask.stopService();
  }

  @override
  void dispose() {
    _positionSub?.cancel();
    _modeSub?.cancel();
    super.dispose();
  }
}

final gpsSourceProvider =
    StateNotifierProvider<GpsSourceNotifier, GpsSourceState>((ref) {
  return GpsSourceNotifier();
});
```

Then make `BleService` satisfy the new interface — edit `app/lib/core/ble/ble_service.dart`'s class declaration:

```dart
import '../../state/gps_source_provider.dart' show GpsSourceBleChannel;

class BleService implements GpsSourceBleChannel {
```
(`BleService` already has all 4 required members from Task 10 — `gpsSourceModeStream`, `readGpsSourceMode`, `writeGpsSourceMode`, `writePhoneGpsSample` — so this is a marker-only change.)

- [ ] **Step 4: Run the test to verify it passes**

Run: `cd app && flutter test test/gps_source_provider_test.dart`
Expected: PASS.

- [ ] **Step 5: Run the full app test suite**

Run: `cd app && flutter test`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add app/lib/state/gps_source_provider.dart app/lib/core/ble/ble_service.dart app/test/gps_source_provider_test.dart
git commit -m "feat(state): add GpsSourceNotifier with foreground-service-backed phone GPS streaming"
```

---

### Task 12: GPS Source toggle UI

**Files:**
- Modify: `app/lib/ui/screens/tabs/device_tab.dart`

**Interfaces:**
- Consumes: `gpsSourceProvider` (Task 11).

Not independently unit-tested — this repo has no widget tests for `DeviceTab` beyond the app-wide smoke test in `app/test/widget_test.dart`; verified by that smoke test still passing plus manual verification in Task 13.

- [ ] **Step 1: Watch the provider and load device state on connect**

Edit `app/lib/ui/screens/tabs/device_tab.dart`, add the import:

```dart
import '../../../state/ble_provider.dart';
import '../../../state/gps_source_provider.dart';
```

In `build()`, after `final bleNotifier = ref.read(bleProvider.notifier);`:

```dart
    final gpsSourceState = ref.watch(gpsSourceProvider);
    final gpsSourceNotifier = ref.read(gpsSourceProvider.notifier);

    ref.listen<BleState>(bleProvider, (previous, next) {
      final justConnected = next.status == DeviceConnectionStatus.connected &&
          previous?.status != DeviceConnectionStatus.connected;
      if (justConnected) gpsSourceNotifier.loadFromDevice();
    });
```

- [ ] **Step 2: Add the toggle row**

Edit `app/lib/ui/screens/tabs/device_tab.dart`, insert after the existing `assisted GPS` `DeviceButton` block (right after its trailing `const SizedBox(height: 16),`):

```dart
          const SizedBox(height: 16),
          DeviceSectionLabel(text: 'gps source'),
          DeviceListRow(
            label: gpsSourceState.mode == GpsSourceMode.hardware
                ? 'hardware (m10)'
                : 'phone',
            detail: gpsSourceState.mode == GpsSourceMode.hardware
                ? 'auto-falls back to phone if m10 has no fix'
                : 'always uses phone location; m10 ignored',
            trailing: DeviceChip(
              text: gpsSourceState.mode == GpsSourceMode.hardware
                  ? 'use phone'
                  : 'use hardware',
              color: AppTheme.cyan,
              onTap: connected && !gpsSourceState.syncing
                  ? () => gpsSourceNotifier.setMode(
                      gpsSourceState.mode == GpsSourceMode.hardware
                          ? GpsSourceMode.phoneForced
                          : GpsSourceMode.hardware)
                  : null,
            ),
            showDivider: false,
          ),
          const SizedBox(height: 16),
```

- [ ] **Step 3: Run the app test suite**

Run: `cd app && flutter test`
Expected: PASS (`widget_test.dart` smoke test still passes with the new section present).

- [ ] **Step 4: Commit**

```bash
git add app/lib/ui/screens/tabs/device_tab.dart
git commit -m "feat(ui): add GPS source toggle to the device tab"
```

---

### Task 13: End-to-end hardware verification

**Files:** none (manual verification against real firmware + app + M10 module)

No code changes — this task confirms the pieces built in Tasks 1-12 work together on real hardware, since none of the BLE/GATT/queue-arbitration wiring is exercisable in the native or Flutter test suites.

- [ ] **Step 1: Flash firmware and install the app**

Run: `pio run -e esp32-s3-devkitc-1 -t upload` (match the actual env name), then `cd app && flutter run` on a phone with a Bluetooth debugger or serial log visibility into the ESP32 (`pio device monitor`).

- [ ] **Step 2: Verify default state**

Connect the app to the device. Open the device tab — "gps source" row should read "hardware (m10)". On the device's own settings menu, the new "gps source" row should also read "hardware".

- [ ] **Step 3: Verify auto-fallback**

With the M10 antenna disconnected (or indoors with no fix), confirm via `pio device monitor` and/or the live dashboard that after ~1.5-5s of no hardware fix, position switches to phone-sourced data (compare displayed lat/lon against the phone's actual location) once the app has written at least one `0x190A` sample.

- [ ] **Step 4: Verify switch-back**

Reconnect the M10 antenna outdoors; once it reacquires a valid fix, confirm the displayed position switches back to hardware-sourced data.

- [ ] **Step 5: Verify phone-forced mode**

Toggle to "phone" from the app. Confirm the device ignores the M10 even if it has a valid fix (compare device menu / dashboard position against phone-only movement, e.g. walking away from the device").

- [ ] **Step 6: Verify bidirectional toggle sync**

With the app open on the device tab, toggle GPS source from the device's own settings menu; confirm the app's toggle updates via the `0x190B` Notify without needing to reconnect. Then toggle from the app and confirm the device menu updates.

- [ ] **Step 7: Verify background operation (phone-forced mode)**

With phone mode active, lock the phone screen (Android: confirm the foreground-service notification is showing) and confirm position data keeps updating on the device for at least 2 minutes. Repeat on iOS with the app backgrounded (not force-quit).

- [ ] **Step 8: Record results**

No commit for this task — if any step fails, file it as a follow-up rather than editing this plan retroactively.

---

## Self-Review

**Spec coverage:** §3.1/§3.2 (GATT characteristics) → Task 5. §4.1 (Settings) → Task 2. §4.2 (phone fix intake) → Tasks 4-5. §4.3 (arbitration) → Tasks 1, 4 (also corrects the spec's informal claim that firmware "derives speed from position deltas as it does for the M10" — the M10's speed comes from the receiver's own Doppler measurement, not firmware-computed deltas; `computePhoneSpeedKmh` in Task 1 is the actual delta-based derivation, used only for phone-sourced fixes). §4.4 (device toggle) → Task 6. §5.1-5.4 (app: dependencies, files, UI, permissions) → Tasks 7-12. §6 (data flow) → Tasks 4-5, 10-11 wire every arrow in the diagram. §8 milestones 1-7 map 1:1 to Tasks 1-2/3, 4, 6, 9-10, 11, 12, 13.

**Placeholder scan:** no TBD/TODO; every step has runnable code or an exact command.

**Type consistency:** `GpsFixSource`/`GpsSourceMode` (Task 1) used identically in Tasks 4-6; `PhoneGpsSample` (Dart, Task 9) vs `PhoneGpsSample` (C++ struct, Task 4) are distinct same-named types in different languages, not shared — noted here to avoid confusion for the implementer. `GpsSourceBleChannel`/`GpsSourceMode`/`GpsSourceState`/`GpsSourceNotifier` (Task 11) used identically in Task 12. `encodePhoneGpsSample` (Task 8) is the single encoder used by both `BleService.writePhoneGpsSample` (Task 10) and its test.
