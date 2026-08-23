#include "ble_camera_remote.h"
#include "ble_task.h"        // OPENCYCLO_BLE_NAME
#include "ble_layout_sync.h" // BLE_OPENCYCLO_SERVICE_UUID
#include "storage/settings.h"
#include <string.h>

// --- Reverse-engineered Insta360 "GPS Remote" GATT profile ---
//
// Source: https://github.com/pchwalek/insta360_ble_esp32 (Patrick Chwalek,
// MIT-licensed), which reverse-engineered the official Insta360 GPS Remote
// accessory's BLE protocol by sniffing Insta360's own Android app.
//
// IMPORTANT: that project verified this protocol only against the Insta360
// X3 and RS 1-inch (the older 360-degree camera line). The Ace Pro 2 is a
// newer, different camera generation (Leica co-branded, different chipset)
// -- Insta360 officially lists the same "GPS Remote"/"GPS Action Remote"
// accessories as compatible with it, which is a good sign the same protocol
// family carries over, but nobody has publicly confirmed the exact command
// bytes below still apply. This is a best-effort port pending on-device
// testing against the real camera.
//
// The camera is the GATT *client* here, not us: we advertise as a peripheral
// impersonating the named accessory, the camera connects to us and
// subscribes to the notify characteristic below, and "pressing a button" on
// this remote means pushing a notify() packet down that subscription --
// there's no separate "send a command to the camera" direction.
//
// The reference project only demonstrated two buttons -- shutter and mode --
// because that's genuinely all the physical remote has. There is no
// confirmed separate "start video" / "stop video" byte sequence: the single
// shutter command takes a photo when the camera is in photo mode, and
// toggles record start/stop when it's in video mode, exactly like pressing
// the camera's own shutter button. Fabricating distinct start/stop commands
// with invented bytes would be a guess this project has deliberately avoided
// all session -- if that granularity turns out to matter, it needs its own
// packet capture against the Ace Pro 2 to confirm.
static const char* CAM_SERVICE_UUID  = "ce80";
static const char* CAM_CHAR_WRITE    = "ce81";
static const char* CAM_CHAR_NOTIFY   = "ce82"; // shutter/mode commands go out via notify()
static const char* CAM_CHAR_READ     = "ce83";

// Secondary service: present in the reference implementation with no
// documented purpose for basic shutter/mode control. Mirrored here
// structurally (same UUIDs/properties, no meaningful payload) purely because
// the camera's own pairing handshake might probe for it before accepting the
// connection -- keeping our GATT layout as close as possible to the one
// proven to work on X3/RS minimizes the number of new variables if this
// doesn't work on the Ace Pro 2.
static const char* CAM_SERVICE2_UUID = "0000D0FF-3C17-D293-8E48-14FE2E4DA212";
static const char* CAM_CHAR2_WRITE1  = "ffd1";
static const char* CAM_CHAR2_READ2   = "ffd2";
static const char* CAM_CHAR2_READ3   = "ffd3";
static const char* CAM_CHAR2_READ4   = "ffd4";
static const char* CAM_CHAR2_READ5   = "ffd5";
static const char* CAM_CHAR2_WRITE8  = "ffd8";
static const char* CAM_CHAR2_READ9   = "fff1";
static const char* CAM_CHAR2_WRITE10 = "fff2";
static const char* CAM_CHAR2_READ11  = "ffe0";

// OpenCyclo's own service (not part of the Insta360 protocol) for setting
// the wake beacon's camera-specific bytes -- any generic BLE GATT tool
// (nRF Connect, etc.) can write 6 raw bytes here, no phone app needed.
//
// 0x1920/0x1921 -- deliberately NOT 0x1902/0x1903: this codebase already
// uses 00001900-00001903 for the layout-sync service (ble_layout_sync.h)
// and 00001910-00001912 for OTA (ble_ota_handler.h). An earlier version of
// this file picked 1902/1903 without checking that range first, which
// collided with the existing telemetry-stream and device-command
// characteristics -- same UUID string, different service, genuinely
// confusing in any GATT browser even though BLE permits the reuse.
static const char* WAKE_CONFIG_SERVICE_UUID = "00001920-0000-1000-8000-00805F9B34FB";
static const char* WAKE_CONFIG_CHAR_UUID    = "00001921-0000-1000-8000-00805F9B34FB";

static const char* CAMERA_REMOTE_NAME = "Insta360 GPS Remote";
static const uint32_t CAMERA_PAIRING_WINDOW_MS = 30000;
// The wake beacon doesn't need the camera's own UI/user to do anything --
// it just needs to be on the air long enough for the sleeping camera's own
// periodic background scan to catch it. Shorter than the pairing window,
// which waits on a human on the camera's touchscreen.
static const uint32_t CAMERA_WAKE_WINDOW_MS = 10000;

static NimBLECharacteristic* pCamNotifyChar = nullptr;
static bool camPairingActive = false;
static bool camWaking = false;
static uint32_t camPairingStartMs = 0;
static volatile bool camSubscribed = false;

// See settings.h: manuf_data[0..13]/[20..25] are a fixed header/trailer
// copied verbatim from the reference implementation (their exact meaning is
// unconfirmed -- possibly an Apple-manufacturer-ID-prefixed beacon format
// Insta360 reuses); manuf_data[14..19] are the camera-specific bytes this
// module fills in from g_settings.insta360_wake_bytes.
static uint8_t buildWakeManufData(uint8_t out[26]) {
  static const uint8_t kHeader[14] = {0x4c, 0x00, 0x02, 0x15, 0x09, 0x4f, 0x52, 0x42,
                                       0x49, 0x54, 0x09, 0xff, 0x0f, 0x00};
  static const uint8_t kTrailer[6] = {0x00, 0x00, 0x00, 0x00, 0xe4, 0x01};
  memcpy(out, kHeader, sizeof(kHeader));
  memcpy(out + 14, g_settings.insta360_wake_bytes, 6);
  memcpy(out + 20, kTrailer, sizeof(kTrailer));
  return 26;
}

class WakeBytesCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChr) override {
    std::string val = pChr->getValue();
    if (val.length() != 6) {
      Serial.printf("[BLE CAM] Wake-bytes write rejected: got %u bytes, need exactly 6.\n",
                    (unsigned)val.length());
      return;
    }
    memcpy(g_settings.insta360_wake_bytes, val.data(), 6);
    saveSettings();
    Serial.printf("[BLE CAM] Camera wake bytes set: %02X %02X %02X %02X %02X %02X\n",
                  g_settings.insta360_wake_bytes[0], g_settings.insta360_wake_bytes[1],
                  g_settings.insta360_wake_bytes[2], g_settings.insta360_wake_bytes[3],
                  g_settings.insta360_wake_bytes[4], g_settings.insta360_wake_bytes[5]);
  }
};
static WakeBytesCallbacks wakeBytesCallbacks;

class CamRemoteCallbacks : public NimBLECharacteristicCallbacks {
  void onSubscribe(NimBLECharacteristic* pChr, ble_gap_conn_desc* desc, uint16_t subValue) override {
    (void)pChr;
    (void)desc;
    camSubscribed = (subValue != 0);
    Serial.printf("[BLE CAM] Peer %s shutter notifications.\n",
                  camSubscribed ? "subscribed to" : "unsubscribed from");
  }
};
static CamRemoteCallbacks camRemoteCallbacks;

void initBleCameraRemoteService(NimBLEServer* pServer) {
  NimBLEService* pService = pServer->createService(CAM_SERVICE_UUID);
  pService->createCharacteristic(CAM_CHAR_WRITE, NIMBLE_PROPERTY::WRITE);

  pCamNotifyChar = pService->createCharacteristic(
    CAM_CHAR_NOTIFY, NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::INDICATE);
  pCamNotifyChar->setCallbacks(&camRemoteCallbacks);
  uint8_t zero = 0;
  pCamNotifyChar->setValue(&zero, 1);

  NimBLECharacteristic* pReadChar = pService->createCharacteristic(CAM_CHAR_READ, NIMBLE_PROPERTY::READ);
  uint16_t readVal = 0x0201; // mirrors the reference implementation's fixed value; purpose unconfirmed
  pReadChar->setValue((const uint8_t*)&readVal, sizeof(readVal));

  pService->start();

  NimBLEService* pService2 = pServer->createService(CAM_SERVICE2_UUID);
  pService2->createCharacteristic(CAM_CHAR2_WRITE1, NIMBLE_PROPERTY::WRITE);
  pService2->createCharacteristic(CAM_CHAR2_READ2, NIMBLE_PROPERTY::READ);

  NimBLECharacteristic* pChar3 = pService2->createCharacteristic(CAM_CHAR2_READ3, NIMBLE_PROPERTY::READ);
  uint32_t val3 = 0x301e9001;
  pChar3->setValue((const uint8_t*)&val3, sizeof(val3));

  NimBLECharacteristic* pChar4 = pService2->createCharacteristic(CAM_CHAR2_READ4, NIMBLE_PROPERTY::READ);
  uint32_t val4 = 0x18002001;
  pChar4->setValue((const uint8_t*)&val4, sizeof(val4));

  pService2->createCharacteristic(CAM_CHAR2_READ5, NIMBLE_PROPERTY::READ);
  pService2->createCharacteristic(CAM_CHAR2_WRITE8, NIMBLE_PROPERTY::WRITE);
  pService2->createCharacteristic(CAM_CHAR2_READ9, NIMBLE_PROPERTY::READ);
  pService2->createCharacteristic(CAM_CHAR2_WRITE10, NIMBLE_PROPERTY::WRITE);
  pService2->createCharacteristic(CAM_CHAR2_READ11, NIMBLE_PROPERTY::READ);
  pService2->start();

  NimBLEService* pWakeConfigService = pServer->createService(WAKE_CONFIG_SERVICE_UUID);
  NimBLECharacteristic* pWakeConfigChar = pWakeConfigService->createCharacteristic(
    WAKE_CONFIG_CHAR_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ);
  pWakeConfigChar->setCallbacks(&wakeBytesCallbacks);
  pWakeConfigChar->setValue(g_settings.insta360_wake_bytes, 6);
  pWakeConfigService->start();

  Serial.println("[BLE CAM] Insta360 remote-control service registered.");
}

// Reverts advertising to OpenCyclo's normal identity -- shared by both the
// pairing window and the wake-beacon window below.
static void revertToNormalAdvertising() {
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->stop();
  pAdvertising->removeServices();
  pAdvertising->setManufacturerData(std::vector<uint8_t>()); // clear the wake beacon, if any
  pAdvertising->setName(OPENCYCLO_BLE_NAME);
  pAdvertising->addServiceUUID(BLE_OPENCYCLO_SERVICE_UUID);
  pAdvertising->start();
}

void startCameraPairing() {
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->stop();
  pAdvertising->removeServices();
  pAdvertising->setName(CAMERA_REMOTE_NAME);
  pAdvertising->addServiceUUID(CAM_SERVICE_UUID);
  pAdvertising->addServiceUUID(CAM_SERVICE2_UUID);
  pAdvertising->start();

  camPairingActive = true;
  camWaking = false;
  camPairingStartMs = millis();
  Serial.println("[BLE CAM] Pairing window open -- advertising as 'Insta360 GPS Remote'. "
                  "Put the camera into its own Bluetooth pairing mode now.");
}

bool hasCameraWakeBytes() {
  uint8_t zero[6] = {0};
  return memcmp(g_settings.insta360_wake_bytes, zero, 6) != 0;
}

void wakeSleepingCamera() {
  if (!hasCameraWakeBytes()) {
    Serial.println("[BLE CAM] No camera wake bytes set yet -- write 6 bytes to the wake-config "
                    "characteristic (e.g. via nRF Connect) first.");
    return;
  }

  uint8_t manufData[26];
  buildWakeManufData(manufData);

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->stop();
  pAdvertising->removeServices();
  pAdvertising->setName(CAMERA_REMOTE_NAME);
  pAdvertising->setManufacturerData(std::vector<uint8_t>(manufData, manufData + 26));
  pAdvertising->start();

  camPairingActive = true; // reuses the same revert timer/tick as pairing
  camWaking = true;
  camPairingStartMs = millis();
  Serial.println("[BLE CAM] Wake beacon on the air for 10s.");
}

void tickCameraPairing() {
  if (!camPairingActive) return;
  uint32_t window = camWaking ? CAMERA_WAKE_WINDOW_MS : CAMERA_PAIRING_WINDOW_MS;
  if (millis() - camPairingStartMs < window) return;

  revertToNormalAdvertising();
  Serial.printf("[BLE CAM] %s window closed -- reverted to 'OpenCyclo-GPS' advertising.\n",
                camWaking ? "Wake" : "Pairing");
  camPairingActive = false;
  camWaking = false;
}

bool isCameraPairing() { return camPairingActive && !camWaking; }
bool isCameraWaking() { return camPairingActive && camWaking; }
bool isCameraSubscribed() { return camSubscribed; }

// Byte 7 is the button id (0x00=power, 0x01=mode, 0x02=shutter); byte 8
// distinguishes the power button's two press lengths in the reference
// implementation (0x00=short press/screen toggle, 0x03=3s hold/power off) --
// shutter and mode don't use byte 8, so it's always 0x00 for them.
static void sendCamCommand(uint8_t button, uint8_t param) {
  if (pCamNotifyChar == nullptr) return;
  uint8_t cmd[9] = {0xfc, 0xef, 0xfe, 0x86, 0x00, 0x03, 0x01, button, param};
  pCamNotifyChar->setValue(cmd, sizeof(cmd));
  pCamNotifyChar->notify();
}

void triggerCameraShutter()       { sendCamCommand(0x02, 0x00); Serial.println("[BLE CAM] Sent shutter command."); }
void triggerCameraMode()          { sendCamCommand(0x01, 0x00); Serial.println("[BLE CAM] Sent mode command."); }
void triggerCameraPowerOff()      { sendCamCommand(0x00, 0x03); Serial.println("[BLE CAM] Sent power-off command."); }
void triggerCameraScreenToggle()  { sendCamCommand(0x00, 0x00); Serial.println("[BLE CAM] Sent screen-toggle command."); }
