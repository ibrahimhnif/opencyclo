# OpenCyclo BLE Companion App & Wireless OTA Firmware Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement ESP32-S3 dual-role BLE GATT server for telemetry streaming, layout sync, and wireless OTA updates, along with a full cross-platform Flutter companion mobile app in `/app`.

**Architecture:** ESP32-S3 firmware runs a NimBLE Central client for sensors while advertising as a Peripheral server (`OpenCyclo-GPS`). It exposes GATT services for Device Info (`0x180A`), Layout & Telemetry Sync (`0x1900`), and Wireless OTA Flashing (`0x1910`). The Flutter app (`/app`) connects via `flutter_blue_plus`, provides a visual drag-and-drop page builder, live ride telemetry mirror, and binary OTA flasher.

**Tech Stack:** C++17, ESP32-S3, Arduino, NimBLE-Arduino, ESP32 `Update` OTA, Dart, Flutter 3.x, `flutter_blue_plus`, `flutter_riverpod`.

**Spec:** [docs/superpowers/specs/2026-08-22-ble-companion-app-and-ota-design.md](file:///Users/hanif/Labs/personal/opencyclo/docs/superpowers/specs/2026-08-22-ble-companion-app-and-ota-design.md)

## Global Constraints
- ESP32-S3 NimBLE Dual Role: Central client scanning/connected to bike sensors must not be disrupted by phone connections.
- BLE MTU: Negotiate MTU 512 for high-throughput JSON layout sync and OTA chunk streaming.
- OTA Safety: Dual-partition table ensures failed transfers do not brick the device; previous firmware remains bootable.
- Flutter Target: Cross-platform iOS and Android codebase inside `/app`.
- UI Theme: Garmin-inspired high-contrast dark slate styling across firmware and mobile app.

---

### Task 1: ESP32-S3 BLE Dual-Role GATT Peripheral & Telemetry Stream

**Files:**
- Modify: `src/hardware/ble_task.h`
- Modify: `src/hardware/ble_task.cpp`

**Interfaces:**
- Produces: BLE advertising `"OpenCyclo-GPS"`, Device Info Service (`0x180A`), Custom Service (`0x1900`), and Telemetry Notify Characteristic (`0x1902`).
- Consumes: `src/core/telemetry_state.h`, `NimBLEDevice.h`

- [ ] **Step 1: Update `ble_task.h`**
Declare GATT Server initialization, connection callbacks, and telemetry notify streamer function.

- [ ] **Step 2: Implement GATT Peripheral Server in `ble_task.cpp`**
Create NimBLEServer, configure Device Info and Custom Service (`0x1900`), setup 24-byte binary telemetry notification loop running at 2Hz.

- [ ] **Step 3: Verify compilation with PlatformIO**
Run: `pio run`

- [ ] **Step 4: Commit Task 1**
```bash
git add src/hardware/ble_task.*
git commit -m "feat(ble): add BLE GATT Peripheral Server and live telemetry streaming"
```

---

### Task 2: ESP32-S3 Layout Config GATT Synchronization

**Files:**
- Create: `src/hardware/ble_layout_sync.h`
- Create: `src/hardware/ble_layout_sync.cpp`
- Modify: `src/storage/layout_config.h`
- Modify: `src/storage/layout_config.cpp`

**Interfaces:**
- Produces: Layout JSON Read/Write characteristic (`0x1901`), JSON parser importing `UiConfig` dynamically and saving to NVS.
- Consumes: `src/ui/engine/layout_manager.h`, `src/storage/layout_config.h`

- [ ] **Step 1: Implement JSON layout parser & exporter in `layout_config.cpp`**
Implement `exportLayoutToString(char* buffer, size_t maxLen)` and `importLayoutFromString(const char* jsonStr)`.

- [ ] **Step 2: Implement BLE Layout Sync callbacks in `ble_layout_sync.cpp`**
Handle `onWrite` to parse incoming layout JSON from mobile app, update `g_ui_config`, save to NVS, and trigger display redraw.

- [ ] **Step 3: Verify compilation**
Run: `pio run`

- [ ] **Step 4: Commit Task 2**
```bash
git add src/hardware/ble_layout_sync.* src/storage/layout_config.*
git commit -m "feat(ble): implement BLE GATT JSON layout configuration synchronization"
```

---

### Task 3: ESP32-S3 Wireless OTA Update Handler

**Files:**
- Create: `src/hardware/ble_ota_handler.h`
- Create: `src/hardware/ble_ota_handler.cpp`
- Modify: `src/hardware/ble_task.cpp`
- Modify: `platformio.ini`

**Interfaces:**
- Produces: OTA Service (`0x1910`), OTA Control Characteristic (`0x1911`), OTA Data Characteristic (`0x1912`).
- Consumes: `Update.h`, `esp_ota_ops.h`

- [ ] **Step 1: Implement `ble_ota_handler.cpp`**
Handle OTA Start command (`0x01`), write binary chunks to `Update.write()`, verify MD5, end OTA, and schedule restart.

- [ ] **Step 2: Register OTA Service in `ble_task.cpp`**
Attach OTA characteristics to the NimBLE Server instance.

- [ ] **Step 3: Verify compilation**
Run: `pio run`

- [ ] **Step 4: Commit Task 3**
```bash
git add src/hardware/ble_ota_handler.* platformio.ini src/hardware/ble_task.cpp
git commit -m "feat(ota): implement BLE Over-The-Air wireless firmware update service"
```

---

### Task 4: Flutter Mobile App Setup & BLE Service Layer

**Files:**
- Create: `app/pubspec.yaml`
- Create: `app/lib/main.dart`
- Create: `app/lib/core/ble/ble_protocol.dart`
- Create: `app/lib/core/ble/ble_service.dart`
- Create: `app/lib/core/models/telemetry_model.dart`
- Create: `app/lib/core/models/layout_config_model.dart`
- Create: `app/lib/state/ble_provider.dart`

**Interfaces:**
- Produces: Complete Flutter project scaffolding, BLE scanning & auto-connect, binary telemetry parser, layout JSON encoder/decoder.

- [ ] **Step 1: Create `app/pubspec.yaml`**
Include `flutter_blue_plus`, `flutter_riverpod`, `file_picker`, `crypto`, `intl`.

- [ ] **Step 2: Create BLE protocol definitions and models**
Define UUIDs, packet decoder for 24-byte telemetry stream, and `UiConfig` Dart models.

- [ ] **Step 3: Implement `ble_service.dart` and Riverpod state provider**
Handle scanning for `"OpenCyclo-GPS"`, connection lifecycle, MTU 512 negotiation, and subscriptions.

- [ ] **Step 4: Commit Task 4**
```bash
git add app/
git commit -m "feat(app): scaffold Flutter companion app with BLE service layer"
```

---

### Task 5: Flutter Visual Page Layout Builder Screen

**Files:**
- Create: `app/lib/ui/screens/tabs/layout_builder_tab.dart`
- Create: `app/lib/state/layout_provider.dart`
- Create: `app/lib/ui/widgets/grid_slot_card.dart`
- Create: `app/lib/ui/theme/app_theme.dart`

**Interfaces:**
- Produces: Visual page editor allowing users to choose template grids (`Hero 6-Grid`, `4-Grid`, `2-Grid + Chart`, `8-Grid`), assign data widgets, preview layout, and sync to OpenCyclo via BLE.

- [ ] **Step 1: Create `app_theme.dart` with Garmin-inspired dark slate theme**
- [ ] **Step 2: Implement `layout_provider.dart`**
Read current layout from OpenCyclo over BLE, mutate pages, and write updated JSON back.

- [ ] **Step 3: Implement `layout_builder_tab.dart` and `grid_slot_card.dart`**
Build interactive UI with slot dropdowns, live preview card, and "Sync to Device" button.

- [ ] **Step 4: Commit Task 5**
```bash
git add app/lib/ui/ app/lib/state/
git commit -m "feat(app): implement visual Page and Widget Layout Builder screen"
```

---

### Task 6: Flutter Live Telemetry Dashboard & OTA Flasher Tabs

**Files:**
- Create: `app/lib/ui/screens/tabs/live_dashboard_tab.dart`
- Create: `app/lib/ui/screens/tabs/ota_update_tab.dart`
- Create: `app/lib/state/ota_provider.dart`
- Create: `app/lib/ui/screens/tabs/device_tab.dart`
- Create: `app/lib/ui/screens/home_navigation_screen.dart`

**Interfaces:**
- Produces: Real-time ride metrics mirror dashboard, device connection manager, and wireless OTA firmware flasher with progress bar.

- [ ] **Step 1: Implement `device_tab.dart`**
Scan button, device RSSI indicator, connection status, battery, and firmware version card.

- [ ] **Step 2: Implement `live_dashboard_tab.dart`**
Hero Speed, Cadence, HR, Power, Grade, Ascent, and Trip distance live tiles.

- [ ] **Step 3: Implement `ota_update_tab.dart` and `ota_provider.dart`**
File picker for `firmware.bin`, chunk streaming with MD5 verification and percentage progress bar.

- [ ] **Step 4: Implement `home_navigation_screen.dart`**
Bottom navigation bar linking Device, Layout Builder, Live Dashboard, and OTA Update tabs.

- [ ] **Step 5: Commit Task 6**
```bash
git add app/lib/
git commit -m "feat(app): implement Device Manager, Live Dashboard, and OTA Flasher tabs"
```

---

### Task 7: Full System Build, Flash & Integration Verification

**Files:**
- Build and verification across firmware and Flutter companion app.

- [ ] **Step 1: Build & Flash ESP32-S3 firmware**
Run: `pio run -t upload --upload-port /dev/cu.usbmodem21242101`

- [ ] **Step 2: Verify BLE advertising**
Verify OpenCyclo advertises as `"OpenCyclo-GPS"` while maintaining sensor connections.

- [ ] **Step 3: Final commit**
```bash
git commit -m "feat: complete OpenCyclo BLE Companion App and Wireless OTA Updates"
```
