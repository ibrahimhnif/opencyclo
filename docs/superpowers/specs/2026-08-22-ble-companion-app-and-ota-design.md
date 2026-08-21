# OpenCyclo BLE Companion App & Wireless OTA Firmware Update Design

**Date:** 2026-08-22
**Status:** Approved for Implementation Planning

## 1. Overview & Purpose

This specification defines the architecture for:
1. **ESP32-S3 BLE Dual-Role GATT Server & OTA Service**: Extending the firmware's `BleTask` to advertise as a BLE Peripheral (`OpenCyclo-GPS`) alongside its Central sensor client role. It provides GATT services for live telemetry streaming, JSON layout configuration synchronization, and high-throughput Over-the-Air (OTA) binary flashing.
2. **Flutter Companion Mobile App (`/app`)**: A cross-platform mobile application (Android & iOS) designed to scan and connect to OpenCyclo, customize page layouts via a visual drag-and-drop builder, mirror telemetry in real-time, and flash new firmware updates wirelessly.

---

## 2. System Architecture & Communication Flow

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32-S3 Firmware                        │
│                                                             │
│  ┌───────────────────────────────────────────────────────┐  │
│  │           NimBLE Dual-Role Stack                      │  │
│  │                                                       │  │
│  │  [Central Client]               [GATT Peripheral]     │  │
│  │  • CSC Speed/Cadence (0x1816)   • Device Info (0x180A)│  │
│  │  • Heart Rate (0x180D)          • Custom Comm (0x1900)│  │
│  │  • Power Meter (0x1818)         • OTA Service (0x1910)│  │
│  └───────────────┬───────────────────────────▲───────────┘  │
│                  │                           │              │
│                  ▼                           ▼              │
│       [TelemetryState Fusion]       [Layout & OTA Handlers] │
└──────────────────┬───────────────────────────▲──────────────┘
                   │                           │
                   │ BLE Wireless (MTU 512)    │
                   ▼                           │
┌──────────────────────────────────────────────┴──────────────┐
│             Flutter Mobile App (/app)                       │
│                                                             │
│  • Device Discovery & Connection (`flutter_blue_plus`)      │
│  • Visual Page & Widget Layout Builder                      │
│  • Live Telemetry Stream Dashboard                          │
│  • High-Speed Wireless OTA Firmware Flasher                 │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. BLE GATT Specification

### 3.1 Device Information Service (`0x180A`)
- **Manufacturer Name String (`0x2A29`)**: `"OpenCyclo Project"`
- **Model Number String (`0x2A24`)**: `"ES3C28P-GPS"`
- **Firmware Revision String (`0x2A26`)**: `"v0.1.0-TreeUI"`

### 3.2 OpenCyclo Communication Service (`UUID: 00001900-0000-1000-8000-00805F9B34FB`)
- **Characteristic 1: Layout Config (`UUID: 00001901-...`)**:
  - **Properties**: Read, Write, Notify
  - **Payload**: JSON string describing `UiConfig` (pages, templates, assigned widget IDs).
- **Characteristic 2: Live Telemetry (`UUID: 00001902-...`)**:
  - **Properties**: Notify
  - **Payload (Binary 24-byte packet)**:
    - `uint16_t speed_x100` (e.g. 2540 = 25.40 km/h)
    - `uint16_t cadence_rpm`
    - `uint16_t heart_rate_bpm`
    - `uint16_t power_watts`
    - `int16_t altitude_m`
    - `int16_t grade_x10` (e.g. 45 = 4.5%)
    - `uint32_t distance_m`
    - `uint32_t ride_time_s`
    - `uint8_t battery_pct`
    - `uint8_t gps_fix_status` (0: None, 1: 2D, 2: 3D Fix)
    - `uint8_t satellites`
    - `uint8_t ride_state` (0: IDLE, 1: ACTIVE, 2: PAUSED)
- **Characteristic 3: Device Commands (`UUID: 00001903-...`)**:
  - **Properties**: Write
  - **Commands**: `0x01` Start Ride, `0x02` Pause Ride, `0x03` Reset Defaults, `0x04` Reboot Device.

### 3.3 OpenCyclo OTA Service (`UUID: 00001910-0000-1000-8000-00805F9B34FB`)
- **Characteristic 1: OTA Control (`UUID: 00001911-...`)**:
  - **Properties**: Write, Notify
  - **Control Commands**:
    - `0x01 [4-byte uint32 size] [16-byte MD5]`: Begin OTA update
    - `0x02`: End OTA & verify checksum
    - `0x03`: Abort OTA
  - **Status Notifications**:
    - `0x00`: Ready / Idle
    - `0x01`: In Progress (ACK chunk)
    - `0x02`: Flash Complete, Rebooting
    - `0xFF [error code]`: OTA Error (Size mismatch, Checksum fail, Write error)
- **Characteristic 2: OTA Data (`UUID: 00001912-...`)**:
  - **Properties**: Write Without Response
  - **Payload**: Binary firmware chunk (up to 490 bytes per packet under MTU 512).

---

## 4. ESP32-S3 Flash Partitioning for Safe OTA

The PlatformIO configuration uses a dual OTA partition table (`default_16MB.csv` / custom 8MB OTA partition):
- `nvs`: 0x9000, size 0x5000 (Settings & Layout persistence)
- `otadata`: 0xe000, size 0x2000 (Active boot partition selector)
- `app0` (`ota_0`): 0x10000, size 0x300000 (3MB Primary App)
- `app1` (`ota_1`): 0x310000, size 0x300000 (3MB Secondary OTA Target App)

---

## 5. Flutter Companion App Architecture (`/app`)

### 5.1 Project Layout
```
app/
  pubspec.yaml
  android/
  ios/
  lib/
    main.dart
    core/
      ble/
        ble_service.dart          -- flutter_blue_plus controller
        ble_protocol.dart         -- packet encoder/decoder & UUID constants
      models/
        telemetry_model.dart      -- Live ride data model
        layout_config_model.dart  -- Page & Widget configuration model
    state/
      ble_provider.dart           -- Riverpod connection state
      telemetry_provider.dart     -- Riverpod live stream provider
      layout_provider.dart        -- Riverpod layout state & sync engine
      ota_provider.dart           -- Riverpod OTA flashing progress engine
    ui/
      theme/
        app_theme.dart            -- Garmin-inspired dark slate styling
      screens/
        home_navigation_screen.dart
        tabs/
          device_tab.dart         -- BLE Scanner & connection manager
          layout_builder_tab.dart -- Drag-and-drop visual page configurator
          live_dashboard_tab.dart -- Real-time ride metrics mirror
          ota_update_tab.dart     -- Wireless firmware file flasher
```

### 5.2 Key Features
1. **Device Discovery & Auto-Reconnect**: Discovers `"OpenCyclo-GPS"` devices and establishes MTU 512 connection.
2. **Visual Page Builder**: Select page, choose grid template (`Hero 6-Grid`, `4-Grid`, `2-Grid + Chart`, `8-Grid`), customize slots with 18 data widgets, and tap **"Sync to OpenCyclo"**.
3. **Live Dashboard**: Real-time telemetry monitoring with high-contrast widgets.
4. **Wireless OTA Flasher**: File picker to select `firmware.bin`, streams chunks with progress percentage, verifies MD5, and reboots ESP32-S3 upon completion.

---

## 6. Implementation Milestones

1. **Milestone 1: ESP32-S3 BLE GATT Peripheral & Dual-Role Integration**
   - Add BLE Peripheral Server advertising `"OpenCyclo-GPS"` to `src/hardware/ble_task.cpp`.
   - Implement Device Info (`0x180A`) and Custom Service (`0x1900`) with Layout & Telemetry characteristics.
2. **Milestone 2: ESP32-S3 Wireless OTA Update Handler**
   - Implement `src/hardware/ble_ota_handler.{h,cpp}` using ESP32 `Update` library.
   - Configure OTA partition table in `platformio.ini`.
3. **Milestone 3: Flutter Companion App Scaffolding & BLE Engine**
   - Scaffold Flutter project in `/app`.
   - Implement `ble_service.dart` with auto-scan, MTU negotiation, and packet parsers.
4. **Milestone 4: Visual Page Layout Builder Screen**
   - Build interactive Flutter UI to customize pages, templates, and widgets, syncing JSON to OpenCyclo.
5. **Milestone 5: Live Telemetry Mirror & OTA Flasher UI**
   - Build live ride telemetry dashboard and OTA file streaming flasher with progress bar.
6. **Milestone 6: End-to-End Hardware Verification**
   - Test BLE connection, live layout syncing, and wireless OTA firmware upload.

---

## 7. Spec Self-Review Checklist
- [x] **Placeholder Scan:** No "TBD" or "TODO" items.
- [x] **Internal Consistency:** UUIDs, binary telemetry packet structures, and MTU sizes align between firmware and Flutter app.
- [x] **Scope Check:** Clear division across 6 implementable milestones.
- [x] **Ambiguity Check:** Explicit byte offsets, GATT UUIDs, and folder structure defined.
