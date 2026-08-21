#include "ble_task.h"
#include "core/telemetry_state.h"
#include <NimBLEDevice.h>

static NimBLEScan* pBLEScan = nullptr;

static int16_t currentHrBpm = -1;
static int16_t currentCadenceRpm = -1;
static int16_t currentPowerWatts = -1;
static float currentCscSpeedKmh = -1.0f;

static uint8_t bleStatusCSC = 0;   // 0=Disconnected, 1=Scanning, 2=Connected
static uint8_t bleStatusHR = 0;
static uint8_t bleStatusPOWER = 0;

// Notification callback for Heart Rate
static void hrNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  if (length < 2) return;
  uint8_t flags = pData[0];
  uint16_t hrValue = 0;
  if (flags & 0x01) {
    hrValue = pData[1] | (pData[2] << 8);
  } else {
    hrValue = pData[1];
  }
  currentHrBpm = (int16_t)hrValue;
}

// Notification callback for Cycling Speed & Cadence
static void cscNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  if (length < 1) return;
  uint8_t flags = pData[0];
  size_t offset = 1;

  static uint32_t lastWheelRevs = 0;
  static uint16_t lastWheelTime = 0;
  static uint16_t lastCrankRevs = 0;
  static uint16_t lastCrankTime = 0;
  static bool hasPrevWheel = false;
  static bool hasPrevCrank = false;

  // Wheel Revs Present (Flags bit 0)
  if (flags & 0x01) {
    if (length >= offset + 6) {
      uint32_t wheelRevs = pData[offset] | (pData[offset+1] << 8) | (pData[offset+2] << 16) | (pData[offset+3] << 24);
      uint16_t wheelTime = pData[offset+4] | (pData[offset+5] << 8);
      offset += 6;

      if (hasPrevWheel) {
        uint32_t revDiff = wheelRevs - lastWheelRevs;
        uint16_t timeDiff = wheelTime - lastWheelTime; // 1/1024s units
        if (timeDiff > 0) {
          float timeSec = (float)timeDiff / 1024.0f;
          float wheelCircumferenceM = 2.096f; // Standard 700c x 25mm wheel
          float speedMs = (revDiff * wheelCircumferenceM) / timeSec;
          currentCscSpeedKmh = speedMs * 3.6f;
        }
      }
      lastWheelRevs = wheelRevs;
      lastWheelTime = wheelTime;
      hasPrevWheel = true;
    }
  }

  // Crank Revs Present (Flags bit 1)
  if (flags & 0x02) {
    if (length >= offset + 4) {
      uint16_t crankRevs = pData[offset] | (pData[offset+1] << 8);
      uint16_t crankTime = pData[offset+2] | (pData[offset+3] << 8);

      if (hasPrevCrank) {
        uint16_t revDiff = crankRevs - lastCrankRevs;
        uint16_t timeDiff = crankTime - lastCrankTime; // 1/1024s units
        if (timeDiff > 0) {
          float timeSec = (float)timeDiff / 1024.0f;
          currentCadenceRpm = (int16_t)((revDiff * 60.0f) / timeSec);
        }
      }
      lastCrankRevs = crankRevs;
      lastCrankTime = crankTime;
      hasPrevCrank = true;
    }
  }
}

// Notification callback for Cycling Power
static void powerNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  if (length < 4) return;
  int16_t powerVal = pData[2] | (pData[3] << 8);
  currentPowerWatts = powerVal;
}

class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
    // Check if device advertises CSC, HR, or Power
    if (advertisedDevice->haveServiceUUID()) {
      if (advertisedDevice->isAdvertisingService(NimBLEUUID((uint16_t)0x1816))) {
        Serial.printf("[BLE] Found CSC Sensor: %s [%s]\n",
                      advertisedDevice->getName().c_str(), advertisedDevice->getAddress().toString().c_str());
      } else if (advertisedDevice->isAdvertisingService(NimBLEUUID((uint16_t)0x180D))) {
        Serial.printf("[BLE] Found Heart Rate Sensor: %s [%s]\n",
                      advertisedDevice->getName().c_str(), advertisedDevice->getAddress().toString().c_str());
      } else if (advertisedDevice->isAdvertisingService(NimBLEUUID((uint16_t)0x1818))) {
        Serial.printf("[BLE] Found Power Meter: %s [%s]\n",
                      advertisedDevice->getName().c_str(), advertisedDevice->getAddress().toString().c_str());
      }
    }
  }
};

void startBleTask() {
  xTaskCreatePinnedToCore(
    bleTaskLoop,
    "BleTask",
    8192,
    NULL,
    1, // Priority 1
    NULL,
    0  // Core 0
  );
}

void bleTaskLoop(void* pvParameters) {
  NimBLEDevice::init("OpenCyclo-GPS");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Max TX Power

  pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(45);
  pBLEScan->setWindow(15);

  Serial.println("[BLE TASK] NimBLE Central initialized. Ready to scan for CSC/HR/Power sensors.");

  for (;;) {
    TelemetryState state = getTelemetrySnapshot();
    state.heart_rate_bpm = currentHrBpm;
    state.cadence_rpm = currentCadenceRpm;
    state.power_watts = currentPowerWatts;

    state.ble_connection_status[0] = bleStatusCSC;
    state.ble_connection_status[1] = bleStatusHR;
    state.ble_connection_status[2] = bleStatusPOWER;

    if (currentCscSpeedKmh >= 0.0f) {
      state.speed_kmh = currentCscSpeedKmh;
      state.speed_source = SPEED_SOURCE_BLE_CSC;
    }

    setTelemetryState(state);

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
