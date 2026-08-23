#include "ble_camera_remote.h"
#include "ble_task.h"        // OPENCYCLO_BLE_NAME
#include "ble_layout_sync.h" // BLE_OPENCYCLO_SERVICE_UUID

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

static const char* CAMERA_REMOTE_NAME = "Insta360 GPS Remote";
static const uint32_t CAMERA_PAIRING_WINDOW_MS = 30000;

static NimBLECharacteristic* pCamNotifyChar = nullptr;
static bool camPairingActive = false;
static uint32_t camPairingStartMs = 0;
static volatile bool camSubscribed = false;

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

  Serial.println("[BLE CAM] Insta360 remote-control service registered.");
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
  camPairingStartMs = millis();
  Serial.println("[BLE CAM] Pairing window open -- advertising as 'Insta360 GPS Remote'. "
                  "Put the camera into its own Bluetooth pairing mode now.");
}

void tickCameraPairing() {
  if (!camPairingActive) return;
  if (millis() - camPairingStartMs < CAMERA_PAIRING_WINDOW_MS) return;

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->stop();
  pAdvertising->removeServices();
  pAdvertising->setName(OPENCYCLO_BLE_NAME);
  pAdvertising->addServiceUUID(BLE_OPENCYCLO_SERVICE_UUID);
  pAdvertising->start();

  camPairingActive = false;
  Serial.println("[BLE CAM] Pairing window closed -- reverted to 'OpenCyclo-GPS' advertising.");
}

bool isCameraPairing() { return camPairingActive; }
bool isCameraSubscribed() { return camSubscribed; }

static void sendCamCommand(uint8_t button) {
  if (pCamNotifyChar == nullptr) return;
  uint8_t cmd[9] = {0xfc, 0xef, 0xfe, 0x86, 0x00, 0x03, 0x01, button, 0x00};
  pCamNotifyChar->setValue(cmd, sizeof(cmd));
  pCamNotifyChar->notify();
  Serial.printf("[BLE CAM] Sent %s command.\n", button == 0x02 ? "shutter" : "mode");
}

void triggerCameraShutter() { sendCamCommand(0x02); }
void triggerCameraMode()    { sendCamCommand(0x01); }
