#include "ble_task.h"
#include "ble_layout_sync.h"
#include "ble_ota_handler.h"
#include "core/telemetry_state.h"
#include "storage/settings.h"
#include <NimBLEDevice.h>

bool g_ble_scanning = false;
static NimBLEScan* pBLEScan = nullptr;
static NimBLEServer* pBLEServer = nullptr;

static int16_t currentHrBpm = -1;
static int16_t currentCadenceRpm = -1;
static int16_t currentPowerWatts = -1;
static float currentCscSpeedKmh = -1.0f;

static uint8_t bleStatusCSC = 0;   // 0=Disconnected, 1=Scanning, 2=Connected
static uint8_t bleStatusHR = 0;
static uint8_t bleStatusPOWER = 0;

void triggerBleScan() {
  if (pBLEScan != nullptr && !g_ble_scanning) {
    Serial.println("[BLE] Triggering 10s active BLE scan for sensors...");
    g_ble_scanning = true;
    pBLEScan->start(10, false /* is_continue */);
  }
}

void forgetSensorProfile(BleProfileType profile) {
  if (profile == BLE_PROFILE_CSC) {
    g_settings.paired_csc_mac[0] = '\0';
    bleStatusCSC = 0;
    currentCadenceRpm = -1;
    currentCscSpeedKmh = -1.0f;
  } else if (profile == BLE_PROFILE_HR) {
    g_settings.paired_hr_mac[0] = '\0';
    bleStatusHR = 0;
    currentHrBpm = -1;
  } else if (profile == BLE_PROFILE_POWER) {
    g_settings.paired_power_mac[0] = '\0';
    bleStatusPOWER = 0;
    currentPowerWatts = -1;
  }
  saveSettings();
  Serial.printf("[BLE] Forgotten sensor profile %d\n", profile);
}

class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
    if (advertisedDevice->haveServiceUUID()) {
      if (advertisedDevice->isAdvertisingService(NimBLEUUID((uint16_t)0x1816))) {
        Serial.printf("[BLE FOUND] CSC Sensor: %s [%s]\n",
                      advertisedDevice->getName().c_str(), advertisedDevice->getAddress().toString().c_str());
        snprintf(g_settings.paired_csc_mac, sizeof(g_settings.paired_csc_mac), "%s", advertisedDevice->getAddress().toString().c_str());
        saveSettings();
        bleStatusCSC = 2; // Connected/Paired
      } else if (advertisedDevice->isAdvertisingService(NimBLEUUID((uint16_t)0x180D))) {
        Serial.printf("[BLE FOUND] Heart Rate Sensor: %s [%s]\n",
                      advertisedDevice->getName().c_str(), advertisedDevice->getAddress().toString().c_str());
        snprintf(g_settings.paired_hr_mac, sizeof(g_settings.paired_hr_mac), "%s", advertisedDevice->getAddress().toString().c_str());
        saveSettings();
        bleStatusHR = 2;
      } else if (advertisedDevice->isAdvertisingService(NimBLEUUID((uint16_t)0x1818))) {
        Serial.printf("[BLE FOUND] Power Meter: %s [%s]\n",
                      advertisedDevice->getName().c_str(), advertisedDevice->getAddress().toString().c_str());
        snprintf(g_settings.paired_power_mac, sizeof(g_settings.paired_power_mac), "%s", advertisedDevice->getAddress().toString().c_str());
        saveSettings();
        bleStatusPOWER = 2;
      }
    }
  }
};

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) override {
    Serial.println("[BLE SERVER] Mobile App connected!");
  }

  void onDisconnect(NimBLEServer* pServer) override {
    Serial.println("[BLE SERVER] Mobile App disconnected. Restarting advertising...");
    NimBLEDevice::startAdvertising();
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
  // 1. Initialize NimBLE Dual Role
  NimBLEDevice::init("OpenCyclo-GPS");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9); // Max TX Power
  NimBLEDevice::setMTU(512);

  // 2. Create GATT Peripheral Server for Smartphone App & OTA
  pBLEServer = NimBLEDevice::createServer();
  pBLEServer->setCallbacks(new ServerCallbacks());

  // Register Device Information Service (0x180A)
  NimBLEService* pDevInfo = pBLEServer->createService(NimBLEUUID((uint16_t)0x180A));
  NimBLECharacteristic* pMfgChar = pDevInfo->createCharacteristic((uint16_t)0x2A29, NIMBLE_PROPERTY::READ);
  pMfgChar->setValue("OpenCyclo Project");
  NimBLECharacteristic* pModelChar = pDevInfo->createCharacteristic((uint16_t)0x2A24, NIMBLE_PROPERTY::READ);
  pModelChar->setValue("ES3C28P-GPS");
  NimBLECharacteristic* pVerChar = pDevInfo->createCharacteristic((uint16_t)0x2A26, NIMBLE_PROPERTY::READ);
  pVerChar->setValue("v0.1.0-TreeUI");
  pDevInfo->start();

  // Register OpenCyclo Communication & OTA Services
  initBleLayoutSyncService(pBLEServer);
  initBleOtaService(pBLEServer);

  // Start BLE Peripheral Advertising
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(BLE_OPENCYCLO_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->start();
  Serial.println("[BLE SERVER] Advertising as 'OpenCyclo-GPS' with MTU 512.");

  // 3. Setup Central Scanner for Bike Sensors (CSC, HR, Power)
  pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(45);
  pBLEScan->setWindow(15);

  Serial.println("[BLE TASK] Dual-Role Central & Peripheral initialized.");

  for (;;) {
    if (g_ble_scanning) {
      if (!pBLEScan->isScanning()) {
        g_ble_scanning = false;
        Serial.println("[BLE] Scan complete.");
      }
    }

    TelemetryState state = getTelemetrySnapshot();
    state.heart_rate_bpm = currentHrBpm;
    state.cadence_rpm = currentCadenceRpm;
    state.power_watts = currentPowerWatts;

    state.ble_connection_status[0] = (g_settings.paired_csc_mac[0] != '\0') ? 2 : (g_ble_scanning ? 1 : 0);
    state.ble_connection_status[1] = (g_settings.paired_hr_mac[0] != '\0') ? 2 : (g_ble_scanning ? 1 : 0);
    state.ble_connection_status[2] = (g_settings.paired_power_mac[0] != '\0') ? 2 : (g_ble_scanning ? 1 : 0);

    if (currentCscSpeedKmh >= 0.0f) {
      state.speed_kmh = currentCscSpeedKmh;
      state.speed_source = SPEED_SOURCE_BLE_CSC;
    }

    setTelemetryState(state);

    // Stream live telemetry to connected smartphone app
    notifyBleTelemetry(state);

    vTaskDelay(pdMS_TO_TICKS(500)); // 2Hz Telemetry stream
  }
}
