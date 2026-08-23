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

// Hold the last HR reading through brief disconnect/reconnect blips instead
// of blanking to "--" the instant the connection drops -- this sensor's
// connection is intermittent enough in practice that resetting immediately
// made the Ride page tile flicker "--"/number/"--" every time it bounced.
// Only actually go stale (and show "--" again) if no fresh notification
// arrives for HR_STALE_TIMEOUT_MS -- long enough to ride out a reconnect,
// short enough that a genuinely removed/powered-off strap doesn't leave a
// frozen, increasingly-wrong number on screen forever.
static uint32_t lastHrUpdateMs = 0;
static const uint32_t HR_STALE_TIMEOUT_MS = 15000;

static uint8_t bleStatusCSC = 0;   // 0=Disconnected, 1=Scanning, 2=Connected
static uint8_t bleStatusHR = 0;
static uint8_t bleStatusPOWER = 0;

// --- Heart Rate GATT client ---
// Pairing used to mean "we saw this device advertise and remembered its MAC"
// -- it never actually connected as a GATT client or subscribed to receive
// live readings, so currentHrBpm stayed -1 forever regardless of pairing
// status. bleTaskLoop() is what actually performs the connect()+subscribe()
// call (never from inside a NimBLE scan-result callback, which isn't a safe
// place to make a blocking connect() call) -- onResult() and the
// reconnect-on-disconnect path below just set hrConnectPending +
// hrTargetAddress, and the main loop picks it up on its next iteration.
static NimBLEClient* pHrClient = nullptr;
static NimBLEAddress hrTargetAddress;
static volatile bool hrConnectPending = false;

static const NimBLEUUID HR_SERVICE_UUID((uint16_t)0x180D);
static const NimBLEUUID HR_MEASUREMENT_UUID((uint16_t)0x2A37);

// Bluetooth SIG Heart Rate Measurement characteristic (0x2A37): byte 0 is a
// flags field whose bit 0 selects whether the heart rate value is UINT8
// (byte 1) or UINT16 little-endian (bytes 1-2). Energy Expended and
// RR-Interval fields may follow but aren't needed for a bpm display.
static void hrNotifyCallback(NimBLERemoteCharacteristic* chr, uint8_t* pData, size_t length, bool isNotify) {
  if (length < 2) {
    Serial.printf("[BLE HR] Notification too short (%u bytes), ignoring.\n", (unsigned)length);
    return;
  }
  uint8_t flags = pData[0];
  uint16_t hr = (flags & 0x01) ? (uint16_t)(pData[1] | (pData[2] << 8)) : (uint16_t)pData[1];
  currentHrBpm = (int16_t)hr;
  lastHrUpdateMs = millis();
  bleStatusHR = 2;
  Serial.printf("[BLE HR] %u bpm\n", hr);
}

class HrClientCallbacks : public NimBLEClientCallbacks {
  void onDisconnect(NimBLEClient* pClient) override {
    Serial.println("[BLE HR] Disconnected.");
    // Deliberately NOT resetting currentHrBpm here -- see the staleness
    // comment above. bleTaskLoop() blanks it on its own once
    // HR_STALE_TIMEOUT_MS has genuinely elapsed with no new notification.
    // Retry against the same saved address rather than dropping the pairing
    // on a transient disconnect (sensor briefly out of range, low battery
    // blip, etc) -- matches the spec's "reconnect in the background against
    // the last known MAC" design.
    if (g_settings.paired_hr_mac[0] != '\0') {
      bleStatusHR = 1; // reconnecting
      hrTargetAddress = NimBLEAddress(std::string(g_settings.paired_hr_mac));
      hrConnectPending = true;
    } else {
      bleStatusHR = 0;
    }
  }
};
static HrClientCallbacks hrClientCallbacks;

// Connects to hrTargetAddress, discovers the Heart Rate service, and
// subscribes to live measurements. Called only from bleTaskLoop()'s own
// task context -- connect() blocks, which is fine here (BleTask has its own
// core/task, unlike the UI task that used to call the blocking scan
// overload).
static void connectToHrSensor() {
  if (pHrClient == nullptr) {
    pHrClient = NimBLEDevice::createClient();
    pHrClient->setClientCallbacks(&hrClientCallbacks, false);
    // Default connect timeout is 30s, and connect() blocks this whole task
    // -- scan-completion polling, CSC/Power status, and the phone-app
    // telemetry stream all stall with it. A sensor that isn't reachable
    // (out of range, chest strap not worn) doesn't need 30s to find that
    // out.
    pHrClient->setConnectTimeout(8);
  }
  Serial.printf("[BLE HR] Connecting to %s...\n", hrTargetAddress.toString().c_str());
  if (pHrClient->isConnected()) {
    // A prior attempt already succeeded (e.g. the boot-time reconnect
    // resolved after a user-triggered scan discovered and re-triggered the
    // same device) -- nothing to do.
    Serial.println("[BLE HR] Already connected, skipping duplicate connect.");
    return;
  }
  if (!pHrClient->connect(hrTargetAddress)) {
    Serial.println("[BLE HR] Connect failed.");
    bleStatusHR = 0;
    return;
  }
  NimBLERemoteService* pSvc = pHrClient->getService(HR_SERVICE_UUID);
  if (pSvc == nullptr) {
    Serial.println("[BLE HR] Heart Rate service not found on this device.");
    pHrClient->disconnect();
    return;
  }
  NimBLERemoteCharacteristic* pChar = pSvc->getCharacteristic(HR_MEASUREMENT_UUID);
  if (pChar == nullptr || !pChar->canNotify()) {
    Serial.println("[BLE HR] Heart Rate Measurement characteristic missing or not notifiable.");
    pHrClient->disconnect();
    return;
  }
  bool subOk = pChar->subscribe(true, hrNotifyCallback);
  bleStatusHR = 2;
  Serial.printf("[BLE HR] subscribe() returned %s -- waiting for notifications.\n", subOk ? "true" : "false");
}

void triggerBleScan() {
  if (pBLEScan != nullptr && !g_ble_scanning) {
    Serial.println("[BLE] Triggering 10s active BLE scan for sensors...");
    g_ble_scanning = true;
    // NimBLEScan::start() has two overloads with similar signatures: a
    // 2-arg (duration, is_continue) form that BLOCKS THE CALLING TASK until
    // the scan completes, and a 3-arg (duration, callback, is_continue) form
    // that starts the scan and returns immediately. This function runs on
    // the UI task's touch-handler call stack (touchWidgetBleManager ->
    // handleWidgetTouch -> handlePageTouch), so calling the blocking
    // overload here froze the entire touchscreen — rendering and touch
    // input alike — for the full 10-second scan. The non-blocking overload
    // is what bleTaskLoop()'s own polling loop (isScanning()) was already
    // written to expect; passing nullptr for the completion callback since
    // that loop's polling is how scan-complete is detected, not a callback.
    pBLEScan->start(10, nullptr /* scanCompleteCB */, false /* is_continue */);
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
    hrConnectPending = false;
    if (pHrClient != nullptr && pHrClient->isConnected()) {
      pHrClient->disconnect();
    }
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
      } else if (advertisedDevice->isAdvertisingService(HR_SERVICE_UUID)) {
        if (bleStatusHR == 2) return; // already connected, ignore further discoveries
        Serial.printf("[BLE FOUND] Heart Rate Sensor: %s [%s]\n",
                      advertisedDevice->getName().c_str(), advertisedDevice->getAddress().toString().c_str());
        snprintf(g_settings.paired_hr_mac, sizeof(g_settings.paired_hr_mac), "%s", advertisedDevice->getAddress().toString().c_str());
        saveSettings();
        // Hand off to bleTaskLoop() to make the actual connect()+subscribe()
        // call -- a blocking connect() from inside this scan callback isn't
        // safe. Stop the scan first; NimBLE can't connect while scanning.
        NimBLEDevice::getScan()->stop();
        hrTargetAddress = advertisedDevice->getAddress();
        hrConnectPending = true;
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

  // Auto-reconnect to a previously-paired HR sensor on boot, without waiting
  // for a fresh scan -- matches the spec's "auto-reconnects on boot" design.
  if (g_settings.paired_hr_mac[0] != '\0') {
    hrTargetAddress = NimBLEAddress(std::string(g_settings.paired_hr_mac));
    hrConnectPending = true;
    bleStatusHR = 1; // reconnecting
  }

  for (;;) {
    if (hrConnectPending) {
      hrConnectPending = false;
      connectToHrSensor();
    }

    if (g_ble_scanning) {
      if (!pBLEScan->isScanning()) {
        g_ble_scanning = false;
        Serial.println("[BLE] Scan complete.");
      }
    }

    // Only actually blank the reading once it's genuinely stale -- a brief
    // disconnect/reconnect (this sensor's connection is intermittent) holds
    // the last known value instead of flickering the tile back to "--".
    if (currentHrBpm >= 0 && (millis() - lastHrUpdateMs) > HR_STALE_TIMEOUT_MS) {
      currentHrBpm = -1;
      bleStatusHR = (g_settings.paired_hr_mac[0] != '\0') ? 1 : 0;
    }

    TelemetryState state = getTelemetrySnapshot();
    state.heart_rate_bpm = currentHrBpm;
    state.cadence_rpm = currentCadenceRpm;
    state.power_watts = currentPowerWatts;

    // CSC/Power still report "paired == connected" (a MAC is saved but,
    // like HR used to be, nothing actually connects to them yet -- out of
    // scope for this fix). HR now reports its real, live-tracked state
    // instead of the same MAC-presence heuristic, since "paired" and
    // "actually connected and streaming" are genuinely different states now.
    state.ble_connection_status[0] = (g_settings.paired_csc_mac[0] != '\0') ? 2 : (g_ble_scanning ? 1 : 0);
    state.ble_connection_status[1] = bleStatusHR;
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
