#include "ble_layout_sync.h"
#include "storage/layout_config.h"
#include "storage/settings.h"
#include "core/telemetry_state.h"
#include <Arduino.h>
#include "navigation/navigation.h"
#include "gps_task.h"
#include "storage/gps_cache.h"
#include "phone_sensor_bridge.h"

class GpsCacheCallbacks : public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* c) override {
    uint8_t p[20];gpsCacheStatus(p);c->setValue(p,sizeof(p));
  }
  void onWrite(NimBLECharacteristic* c) override {
    if(getTelemetrySnapshot().ride_state!=RIDE_STATE_IDLE)return;
    const std::string p=c->getValue();gpsCacheCommand(reinterpret_cast<const uint8_t*>(p.data()),p.size());
  }
};

class GpsAssistanceCallbacks : public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* c) override {
    uint8_t status[12];gpsAssistanceStatus(status);c->setValue(status,sizeof(status));
  }
  void onWrite(NimBLECharacteristic* c) override {
    const std::string value=c->getValue();
    gpsAssistanceCommand(reinterpret_cast<const uint8_t*>(value.data()),value.size());
  }
};

static NimBLECharacteristic* pLayoutConfigChar = nullptr;
class GpsIdentityCallbacks : public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* c) override {
    uint8_t data[20];const size_t n=gpsIdentityRead(data);c->setValue(data,n);
  }
  void onWrite(NimBLECharacteristic* c) override {
    const std::string value=c->getValue();
    gpsIdentityCommand(reinterpret_cast<const uint8_t*>(value.data()),value.size());
  }
};
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
    // Bytes 11-14 (UTC epoch seconds) are optional -- older app builds send
    // only the original 11 bytes, and 0 means "not sent" either way.
    uint32_t utcEpochS = 0;
    if (v.size() >= 15) memcpy(&utcEpochS, v.data() + 11, 4);
    setPhoneGpsSample(latE7 / 1e7, lonE7 / 1e7, accCm / 100.0f, utcEpochS);
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

class PhoneBaroCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c) override {
    const std::string v = c->getValue();
    if (v.size() < 3) return;
    int16_t altitudeDm;
    memcpy(&altitudeDm, v.data() + 0, 2);
    // Byte 2 is a sequence number, informational only.
    setPhoneAltitudeSample(altitudeDm / 10.0f);
  }
};

class PhoneCompassCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c) override {
    const std::string v = c->getValue();
    if (v.size() < 4) return;
    uint16_t headingDeciDeg;
    memcpy(&headingDeciDeg, v.data() + 0, 2);
    uint8_t accuracy = (uint8_t)v[2];
    // Byte 3 is a sequence number, informational only.
    setPhoneHeadingSample((headingDeciDeg % 3600) / 10.0f, accuracy);
  }
};

static NimBLECharacteristic* pBaroSourceModeChar = nullptr;
void setBaroSourceMode(uint8_t mode) {
  g_settings.baro_source_mode = mode;
  saveSettings();
  if (pBaroSourceModeChar != nullptr) {
    pBaroSourceModeChar->setValue(&g_settings.baro_source_mode, 1);
    pBaroSourceModeChar->notify();
  }
}
class BaroSourceModeCallbacks : public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* c) override {
    c->setValue(&g_settings.baro_source_mode, 1);
  }
  void onWrite(NimBLECharacteristic* c) override {
    const std::string v = c->getValue();
    if (v.empty()) return;
    setBaroSourceMode((uint8_t)v[0]);
  }
};

static NimBLECharacteristic* pCompassSourceModeChar = nullptr;
void setCompassSourceMode(uint8_t mode) {
  g_settings.compass_source_mode = mode;
  saveSettings();
  if (pCompassSourceModeChar != nullptr) {
    pCompassSourceModeChar->setValue(&g_settings.compass_source_mode, 1);
    pCompassSourceModeChar->notify();
  }
}
class CompassSourceModeCallbacks : public NimBLECharacteristicCallbacks {
  void onRead(NimBLECharacteristic* c) override {
    c->setValue(&g_settings.compass_source_mode, 1);
  }
  void onWrite(NimBLECharacteristic* c) override {
    const std::string v = c->getValue();
    if (v.empty()) return;
    setCompassSourceMode((uint8_t)v[0]);
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

  // 4. Phone GPS Position Characteristic (Write No Response)
  auto* phoneGpsChar = pService->createCharacteristic(
    BLE_PHONE_GPS_CHAR_UUID, NIMBLE_PROPERTY::WRITE_NR);
  phoneGpsChar->setCallbacks(new PhoneGpsCallbacks());

  // 5. GPS Source Mode Characteristic (Read / Write / Notify)
  pGpsSourceModeChar = pService->createCharacteristic(
    BLE_GPS_SOURCE_MODE_CHAR_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  pGpsSourceModeChar->setValue(&g_settings.gps_source_mode, 1);
  pGpsSourceModeChar->setCallbacks(new GpsSourceModeCallbacks());

  // 6. Phone Barometric Altitude Characteristic (Write No Response)
  auto* phoneBaroChar = pService->createCharacteristic(
    BLE_PHONE_BARO_CHAR_UUID, NIMBLE_PROPERTY::WRITE_NR);
  phoneBaroChar->setCallbacks(new PhoneBaroCallbacks());

  // 7. Phone Compass Heading Characteristic (Write No Response)
  auto* phoneCompassChar = pService->createCharacteristic(
    BLE_PHONE_COMPASS_CHAR_UUID, NIMBLE_PROPERTY::WRITE_NR);
  phoneCompassChar->setCallbacks(new PhoneCompassCallbacks());

  // 8. Baro Source Mode Characteristic (Read / Write / Notify)
  pBaroSourceModeChar = pService->createCharacteristic(
    BLE_BARO_SOURCE_MODE_CHAR_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  pBaroSourceModeChar->setValue(&g_settings.baro_source_mode, 1);
  pBaroSourceModeChar->setCallbacks(new BaroSourceModeCallbacks());

  // 9. Compass Source Mode Characteristic (Read / Write / Notify)
  pCompassSourceModeChar = pService->createCharacteristic(
    BLE_COMPASS_SOURCE_MODE_CHAR_UUID,
    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
  pCompassSourceModeChar->setValue(&g_settings.compass_source_mode, 1);
  pCompassSourceModeChar->setCallbacks(new CompassSourceModeCallbacks());

  initNavigationService(pService);
  auto* assistanceChar=pService->createCharacteristic(
    "00001907-0000-1000-8000-00805F9B34FB",NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  assistanceChar->setCallbacks(new GpsAssistanceCallbacks());
  auto* identityChar=pService->createCharacteristic(
    "00001908-0000-1000-8000-00805F9B34FB",NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  identityChar->setCallbacks(new GpsIdentityCallbacks());
  auto* cacheChar=pService->createCharacteristic(
    "00001909-0000-1000-8000-00805F9B34FB",NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  cacheChar->setCallbacks(new GpsCacheCallbacks());
  pService->start();
  Serial.println("[BLE SYNC] OpenCyclo GATT Communication Service registered.");
}

void notifyBleTelemetry(const TelemetryState& state) {
  if (pTelemetryStreamChar != nullptr && pTelemetryStreamChar->getSubscribedCount() > 0) {
    // 24-byte compact binary telemetry packet
    uint8_t packet[24];
    uint16_t spd = (uint16_t)(state.display_speed_kmh * 100.0f);
    uint16_t cad = (state.cadence_rpm >= 0) ? (uint16_t)state.cadence_rpm : 0xFFFF;
    uint16_t hr  = (state.heart_rate_bpm >= 0) ? (uint16_t)state.heart_rate_bpm : 0xFFFF;
    uint16_t pwr = (state.power_watts >= 0) ? (uint16_t)state.power_watts : 0xFFFF;
    int16_t alt  = state.altitude_valid?(int16_t)state.altitude_m:INT16_MIN;
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
