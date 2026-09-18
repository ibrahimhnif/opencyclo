#include "ble_task.h"
#include "ble_layout_sync.h"
#include "ble_ota_handler.h"
#include "ble_camera_remote.h"
#include "core/telemetry_state.h"
#include "storage/settings.h"
#include <NimBLEDevice.h>
#include <atomic>
#include "navigation/navigation.h"
#include "csc_sensors.h"
#include "sensor_catalog.h"
#include <mutex>
#include <algorithm>

static std::atomic<int> stopState{0};

bool prepareBleForPowerOff(uint32_t timeoutMs) {
  stopState.store(1);
  uint32_t start = millis();
  while (stopState.load() != 2) {
    if (millis() - start >= timeoutMs) {
      stopState.store(0);
      return false;
    }
    delay(10);
  }
  return true;
}

void resumeBleAfterPowerOff() { stopState.store(0); }
void stopBleForPowerOff() { detachNavigationService(); NimBLEDevice::deinit(true); }

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

// How often bleTaskLoop() retries connecting to a saved-but-not-currently-
// connected HR sensor. Longer than the 8s connect() timeout (setConnectTimeout,
// see connectToHrSensor()) so a just-finished failed attempt gets a real gap
// before the next one, rather than hammering the radio back-to-back.
static const uint32_t HR_RECONNECT_RETRY_MS = 12000;

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
  if (length < 2 || ((pData[0]&1) && length<3)) {
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
      hrTargetAddress = NimBLEAddress(std::string(g_settings.paired_hr_mac), g_settings.paired_hr_addr_type);
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
    if(!pHrClient){bleStatusHR=0;Serial.println("[BLE HR] no client capacity");return;}
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
  bleStatusHR = subOk ? 2 : 0;
  if(!subOk)pHrClient->disconnect();
  Serial.printf("[BLE HR] subscribe() returned %s -- waiting for notifications.\n", subOk ? "true" : "false");
}

static void scanSensorsNow() {
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

static void forgetSensorNow(BleProfileType profile) {
  if (profile == BLE_PROFILE_CSC) {
    forgetCscSensor(0);
    g_settings.paired_csc_mac[0] = '\0';
    bleStatusCSC = 0;
    currentCadenceRpm = -1;
    currentCscSpeedKmh = -1.0f;
  } else if(profile==BLE_PROFILE_CADENCE) {
    forgetCscSensor(1);
    g_settings.paired_cadence_mac[0]='\0';
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

namespace {
std::mutex catalogMutex;
SensorCatalog catalog;
int sensorAction=0;
SensorRow selectedSensor;
BleProfileType selectedForget=BLE_PROFILE_CSC;
void sensorStatus(const char* text){std::lock_guard<std::mutex> lock(catalogMutex);snprintf(catalog.snapshot.status,64,"%s",text);}
}
SensorSnapshot getSensorSnapshot(){std::lock_guard<std::mutex> lock(catalogMutex);return catalog.snapshot;}
void triggerBleScan(){std::lock_guard<std::mutex> lock(catalogMutex);if(!catalog.snapshot.busy && !catalog.snapshot.scanning){sensorAction=1;catalog.snapshot.busy=true;}}
bool requestSensorConnect(const SensorRow& row){std::lock_guard<std::mutex> lock(catalogMutex);if(catalog.snapshot.busy)return false;selectedSensor=row;sensorAction=2;catalog.snapshot.busy=true;snprintf(catalog.snapshot.status,64,"Connecting...");return true;}
void forgetSensorProfile(BleProfileType profile){std::lock_guard<std::mutex> lock(catalogMutex);if(!catalog.snapshot.busy){selectedForget=profile;sensorAction=3;catalog.snapshot.busy=true;}}

static NimBLEClient* powerClient=nullptr;
static uint32_t powerReceived=0,lastPowerAttempt=0;
static bool powerSubscribed=false;
static void powerNotify(NimBLERemoteCharacteristic*,uint8_t* data,size_t n,bool){
  if(n<4)return;currentPowerWatts=int16_t(uint16_t(data[2])|(uint16_t(data[3])<<8));powerReceived=millis();
}
static bool connectPower(const char* mac,uint8_t type){
  if(!powerClient){powerClient=NimBLEDevice::createClient();if(!powerClient)return false;powerClient->setConnectTimeout(3);}
  powerSubscribed=false;
  if(!powerClient->isConnected() && !powerClient->connect(NimBLEAddress(std::string(mac),type)))return false;
  auto service=powerClient->getService(NimBLEUUID(uint16_t(0x1818)));
  auto measurement=service?service->getCharacteristic(NimBLEUUID(uint16_t(0x2a63))):nullptr;
  bool ok=measurement && measurement->canNotify() && measurement->subscribe(true,powerNotify);
  if(!ok)powerClient->disconnect();powerSubscribed=ok;return ok;
}
static void refreshSensorSnapshot(){
  SensorRow rows[4];unsigned count=0;
  for(unsigned slot=0;slot<2;slot++){
    const char* mac=slot?g_settings.paired_cadence_mac:g_settings.paired_csc_mac;if(!mac[0])continue;
    auto& row=rows[count++];snprintf(row.mac,18,"%s",mac);row.profile=slot?BLE_PROFILE_CADENCE:BLE_PROFILE_CSC;
    uint8_t capabilities;cscSensorInfo(slot,row.connected,capabilities);
    row.kind=capabilities==1?SensorKind::Speed:capabilities==2?SensorKind::Cadence:SensorKind::Csc;
  }
  if(g_settings.paired_hr_mac[0]){auto& row=rows[count++];snprintf(row.mac,18,"%s",g_settings.paired_hr_mac);row.kind=SensorKind::Heart;row.profile=BLE_PROFILE_HR;row.connected=pHrClient && pHrClient->isConnected() && bleStatusHR==2;}
  if(g_settings.paired_power_mac[0]){auto& row=rows[count++];snprintf(row.mac,18,"%s",g_settings.paired_power_mac);row.kind=SensorKind::Power;row.profile=BLE_PROFILE_POWER;row.connected=powerSubscribed && powerClient && powerClient->isConnected();}
  std::lock_guard<std::mutex> lock(catalogMutex);catalog.snapshot.pairedCount=count;
  for(unsigned i=0;i<count;i++)catalog.snapshot.paired[i]=rows[i];catalog.snapshot.scanning=g_ble_scanning;
}
static void processSensorAction(){
  int action;SensorRow row;BleProfileType profile;
  {std::lock_guard<std::mutex> lock(catalogMutex);action=sensorAction;sensorAction=0;row=selectedSensor;profile=selectedForget;}
  if(!action)return;
  if(action==1){
    {std::lock_guard<std::mutex> lock(catalogMutex);catalog.clearFound();}
    scanSensorsNow();sensorStatus("Spin sensors to wake");
  } else if(action==3){
    forgetSensorNow(profile);
    if(profile==BLE_PROFILE_POWER && powerClient){powerSubscribed=false;powerClient->disconnect();}
    tickCscSensors(true);sensorStatus("Removed");
  } else {
    if(pBLEScan && pBLEScan->isScanning())pBLEScan->stop();g_ble_scanning=false;
    bool ok=false;
    if(row.kind==SensorKind::Csc)ok=pairCscSensor(row.mac,row.addressType);
    else if(row.kind==SensorKind::Heart && !g_settings.paired_hr_mac[0]){
      hrTargetAddress=NimBLEAddress(std::string(row.mac),row.addressType);connectToHrSensor();
      ok=pHrClient && pHrClient->isConnected() && bleStatusHR==2;
      if(ok){snprintf(g_settings.paired_hr_mac,18,"%s",row.mac);g_settings.paired_hr_addr_type=row.addressType;saveSettings();}
    } else if(row.kind==SensorKind::Power && !g_settings.paired_power_mac[0]){
      ok=connectPower(row.mac,row.addressType);
      if(ok){snprintf(g_settings.paired_power_mac,18,"%s",row.mac);g_settings.paired_power_addr_type=row.addressType;saveSettings();}
    }
    sensorStatus(ok?"Paired":"Failed / slot full. Retry or forget.");
  }
  refreshSensorSnapshot();
  {std::lock_guard<std::mutex> lock(catalogMutex);catalog.snapshot.busy=false;}
}
class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* device) override {
    const uint16_t services[]={0x1816,0x180d,0x1818};
    const SensorKind kinds[]={SensorKind::Csc,SensorKind::Heart,SensorKind::Power};
    for(unsigned i=0;i<3;i++)if(device->isAdvertisingService(NimBLEUUID(services[i]))){
      SensorRow row;row.kind=kinds[i];row.addressType=device->getAddress().getType();row.rssi=device->getRSSI();
      snprintf(row.name,32,"%s",device->getName().c_str());snprintf(row.mac,18,"%s",device->getAddress().toString().c_str());
      std::lock_guard<std::mutex> lock(catalogMutex);catalog.discover(row);
    }
  }
};

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) override {
    Serial.println("[BLE SERVER] Mobile App connected!");
  }

  void onDisconnect(NimBLEServer* pServer) override {
    abortBleOtaOnDisconnect();
    abortRouteTransfer();
    if (stopState.load() != 0) return;
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
  NimBLEDevice::init(OPENCYCLO_BLE_NAME);
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
  initBleCameraRemoteService(pBLEServer);

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
    hrTargetAddress = NimBLEAddress(std::string(g_settings.paired_hr_mac), g_settings.paired_hr_addr_type);
    hrConnectPending = true;
    bleStatusHR = 1; // reconnecting
  }

  uint32_t lastHrConnectAttemptMs = 0;

  for (;;) {
    // Power-off owns the radio once requested. Acknowledge before starting
    // any potentially blocking reconnect/pairing work so shutdown does not
    // have to wait through a BLE timeout.
    if (stopState.load() != 0) {
      int expected = 1;
      stopState.compare_exchange_strong(expected, 2);
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }

    processSensorAction();
    if (hrConnectPending && !pBLEScan->isScanning()) {
      hrConnectPending = false;
      lastHrConnectAttemptMs = millis();
      connectToHrSensor();
    }

    // connectToHrSensor() failing (not "disconnecting after having
    // connected" -- that path already retries via HrClientCallbacks::
    // onDisconnect(), this is the case where connect() itself never
    // succeeded in the first place) used to just give up silently:
    // bleStatusHR dropped to 0 and nothing ever set hrConnectPending again
    // until a user manually opened the Sensors page and tapped scan. Retry
    // periodically in the background instead, so a paired sensor that
    // simply wasn't ready yet at boot (or missed one connection window)
    // gets picked up on its own, matching "auto-reconnect on boot" as an
    // ongoing behavior rather than a single attempt.
    if (!pBLEScan->isScanning() && bleStatusHR != 2 && g_settings.paired_hr_mac[0] != '\0' &&
        (millis() - lastHrConnectAttemptMs) > HR_RECONNECT_RETRY_MS) {
      lastHrConnectAttemptMs = millis();
      hrTargetAddress = NimBLEAddress(std::string(g_settings.paired_hr_mac), g_settings.paired_hr_addr_type);
      connectToHrSensor();
    }

    if(!pBLEScan->isScanning() && g_settings.paired_power_mac[0] &&
       (!powerClient || !powerClient->isConnected()) && millis()-lastPowerAttempt>12000){
      lastPowerAttempt=millis();connectPower(g_settings.paired_power_mac,g_settings.paired_power_addr_type);
    }
    if(millis()-powerReceived>5000)currentPowerWatts=-1;
    tickCameraPairing();

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

    tickCscSensors(pBLEScan->isScanning());
    uint8_t cscConnected=0;
    cscValues(currentCscSpeedKmh,currentCadenceRpm,cscConnected);
    setCscTelemetry(currentCscSpeedKmh,currentCadenceRpm,cscConnected);
    TelemetryState state = getTelemetrySnapshot();
    state.heart_rate_bpm = currentHrBpm;
    state.cadence_rpm = currentCadenceRpm;
    state.power_watts = currentPowerWatts;

    // Subscription state, not saved-MAC presence, defines a live connection.
    state.ble_connection_status[0] = cscConnected ? 2 : (g_ble_scanning ? 1 : 0);
    state.ble_connection_status[1] = bleStatusHR;
    state.ble_connection_status[2] = powerSubscribed && powerClient && powerClient->isConnected() ? 2 : 0;

    if (currentCscSpeedKmh >= 0.0f) {
      state.speed_kmh = currentCscSpeedKmh;
      state.speed_source = SPEED_SOURCE_BLE_CSC;
    } else if(state.speed_source==SPEED_SOURCE_BLE_CSC) {
      state.speed_source=SPEED_SOURCE_NONE;state.speed_kmh=0;
    }

    setTelemetryState(state);

    // Stream live telemetry to connected smartphone app
    notifyBleTelemetry(getTelemetrySnapshot());
    refreshSensorSnapshot();

    vTaskDelay(pdMS_TO_TICKS(500)); // 2Hz Telemetry stream
  }
}
