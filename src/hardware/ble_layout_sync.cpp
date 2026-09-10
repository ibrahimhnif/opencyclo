#include "ble_layout_sync.h"
#include "storage/layout_config.h"
#include "core/telemetry_state.h"
#include <Arduino.h>
#include "navigation/navigation.h"

static NimBLECharacteristic* pLayoutConfigChar = nullptr;
static NimBLECharacteristic* pTelemetryStreamChar = nullptr;
static NimBLECharacteristic* pDeviceCommandChar = nullptr;

class LayoutConfigCallbacks : public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* pChar) override {
    char buf[1024];
    size_t len = exportLayoutToString(buf, sizeof(buf));
    if (len > 0) {
      pChar->setValue((const uint8_t*)buf, len);
      Serial.printf("[BLE SYNC] Layout JSON read requested (%u bytes).\n", (unsigned int)len);
    }
  }

  void onWrite(NimBLECharacteristic* pChar) override {
    std::string val = pChar->getValue();
    if (!val.empty()) {
      Serial.printf("[BLE SYNC] Received layout JSON update (%u bytes)...\n", (unsigned int)val.length());
      if (importLayoutFromString(val.c_str())) {
        Serial.println("[BLE SYNC] Layout updated successfully from smartphone app!");
        pChar->setValue((const uint8_t*)"OK", 2);
        pChar->notify();
      } else {
        Serial.println("[BLE SYNC] Failed to parse layout JSON from app.");
        pChar->setValue((const uint8_t*)"ERR", 3);
        pChar->notify();
      }
    }
  }
};

class DeviceCommandCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar) override {
    std::string val = pChar->getValue();
    if (val.empty()) return;

    uint8_t cmd = (uint8_t)val[0];
    if (cmd == 0x01) { // Start Ride
      setManualRideState(RIDE_STATE_ACTIVE);
      Serial.println("[BLE CMD] Ride STARTED via App command.");
    } else if (cmd == 0x02) { // Pause Ride
      setManualRideState(RIDE_STATE_PAUSED);
      Serial.println("[BLE CMD] Ride PAUSED via App command.");
    } else if (cmd == 0x03) { // Reset Defaults
      resetLayoutToDefaults();
      saveLayoutConfig();
      Serial.println("[BLE CMD] Layout reset to defaults via App command.");
    } else if (cmd == 0x04) { // Reboot Device
      Serial.println("[BLE CMD] Rebooting ESP32-S3 via App command...");
      delay(500);
      ESP.restart();
    }
  }
};

void initBleLayoutSyncService(NimBLEServer* pServer) {
  NimBLEService* pService = pServer->createService(BLE_OPENCYCLO_SERVICE_UUID);

  // 1. Layout Config Characteristic (Read / Write / Notify)
  pLayoutConfigChar = pService->createCharacteristic(
    BLE_LAYOUT_CONFIG_CHAR_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY
  );
  pLayoutConfigChar->setCallbacks(new LayoutConfigCallbacks());

  // 2. Live Telemetry Stream Characteristic (Notify)
  pTelemetryStreamChar = pService->createCharacteristic(
    BLE_TELEMETRY_STREAM_CHAR_UUID,
    NIMBLE_PROPERTY::NOTIFY
  );

  // 3. Device Command Characteristic (Write)
  pDeviceCommandChar = pService->createCharacteristic(
    BLE_DEVICE_COMMAND_CHAR_UUID,
    NIMBLE_PROPERTY::WRITE
  );
  pDeviceCommandChar->setCallbacks(new DeviceCommandCallbacks());

  initNavigationService(pService);
  pService->start();
  Serial.println("[BLE SYNC] OpenCyclo GATT Communication Service registered.");
}

void notifyBleTelemetry(const TelemetryState& state) {
  if (pTelemetryStreamChar != nullptr && pTelemetryStreamChar->getSubscribedCount() > 0) {
    // 24-byte compact binary telemetry packet
    uint8_t packet[24];
    uint16_t spd = (uint16_t)(state.speed_kmh * 100.0f);
    uint16_t cad = (state.cadence_rpm >= 0) ? (uint16_t)state.cadence_rpm : 0xFFFF;
    uint16_t hr  = (state.heart_rate_bpm >= 0) ? (uint16_t)state.heart_rate_bpm : 0xFFFF;
    uint16_t pwr = (state.power_watts >= 0) ? (uint16_t)state.power_watts : 0xFFFF;
    int16_t alt  = (int16_t)state.altitude_m;
    int16_t grd  = (int16_t)(state.grade_pct * 10.0f);
    uint32_t dist = (uint32_t)(state.trip_distance_km * 1000.0f);
    uint32_t time = state.ride_time_s;
    uint8_t bat  = state.battery_pct;
    uint8_t fix  = state.gps_has_fix ? 2 : 0;
    uint8_t sat  = (uint8_t)state.satellites;
    uint8_t ride = (uint8_t)state.ride_state;

    memcpy(packet + 0, &spd, 2);
    memcpy(packet + 2, &cad, 2);
    memcpy(packet + 4, &hr, 2);
    memcpy(packet + 6, &pwr, 2);
    memcpy(packet + 8, &alt, 2);
    memcpy(packet + 10, &grd, 2);
    memcpy(packet + 12, &dist, 4);
    memcpy(packet + 16, &time, 4);
    packet[20] = bat;
    packet[21] = fix;
    packet[22] = sat;
    packet[23] = ride;

    pTelemetryStreamChar->setValue(packet, sizeof(packet));
    pTelemetryStreamChar->notify();
  }
}
