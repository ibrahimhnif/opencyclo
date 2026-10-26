#include "camera_control.h"
#include "camera_protocol.h"
#include "ble_camera_remote.h"
#include "storage/settings.h"
#include <NimBLEDevice.h>
#include <mutex>
#include <string>
#include <string.h>

// See camera_control.h for the design. The wire format is in camera_protocol.h.

namespace {

// Camera's own control service -- distinct from the ce80 "GPS Remote" service
// this firmware emulates in ble_camera_remote.cpp (there the camera is the
// central; here we are).
const NimBLEUUID CAM_SERVICE_UUID("0000be80-0000-1000-8000-00805f9b34fb");
const NimBLEUUID CAM_WRITE_UUID("0000be81-0000-1000-8000-00805f9b34fb");
const NimBLEUUID CAM_NOTIFY_UUID("0000be82-0000-1000-8000-00805f9b34fb");

constexpr uint32_t KEEPALIVE_MS = 2000;
constexpr uint32_t CONNECT_RETRY_MS = 12000;
constexpr uint32_t SCAN_SECONDS = 10;
constexpr uint32_t PAIR_TIMEOUT_MS = 30000;
constexpr size_t RX_CAPACITY = 512;

NimBLEClient* camClient = nullptr;
NimBLERemoteCharacteristic* writeChar = nullptr;
bool subscribed = false;

bool pairRequested = false;
bool pairPrepared = false;
uint32_t pairStartMs = 0;
bool pendingConnect = false;
char pendingMac[18] = {};
uint8_t pendingType = 0;

uint32_t seqCounter = 0;
uint32_t lastKeepaliveMs = 0;
uint32_t lastConnectAttemptMs = 0;
uint32_t awaitingSeq = 0;
int awaitingValue = -1;
bool awaitingAck = false;
int currentMode = -1;
char statusText[32] = "Not connected";

uint8_t rxBuffer[RX_CAPACITY];
size_t rxLen = 0;

std::mutex mtx;

void setStatus(const char* text) { snprintf(statusText, sizeof(statusText), "%s", text); }

bool writeFrame(const uint8_t* data, size_t length) {
  if (!writeChar) return false;
  const uint16_t mtu = camClient ? camClient->getMTU() : 23;
  const size_t chunk = mtu > 3 ? size_t(mtu - 3) : 20;
  for (size_t offset = 0; offset < length; offset += chunk) {
    size_t n = length - offset;
    if (n > chunk) n = chunk;
    if (!writeChar->writeValue(data + offset, n, true)) return false;
  }
  return true;
}

void sendSync() {
  uint8_t buf[32];
  size_t n = camera_proto::buildSync(buf, sizeof(buf));
  if (n) writeFrame(buf, n);
}

void sendKeepalive() {
  uint8_t buf[16];
  size_t n = camera_proto::buildKeepalive(buf, sizeof(buf));
  if (n) writeFrame(buf, n);
}

// SetOptions{option_types=[optionType], value{optionType=value}}.
bool sendSetOption(uint16_t optionType, int value) {
  if (!writeChar) return false;
  uint8_t pb[32];
  size_t pbLen = camera_proto::pbSetOption(pb, sizeof(pb), optionType, value);
  uint8_t frame[64];
  const uint32_t seq = seqCounter++;
  size_t n = camera_proto::buildCommand(frame, sizeof(frame), camera_proto::CMD_SET_OPTIONS, seq, pb, pbLen);
  if (!n || !writeFrame(frame, n)) return false;
  awaitingSeq = seq;
  awaitingValue = value;
  awaitingAck = true;
  return true;
}

// Accumulates notifications and consumes whole [u32 len][payload] frames.
void processRx() {
  while (rxLen >= 4) {
    uint32_t total = 0;
    if (!camera_proto::parseFrameLength(rxBuffer, rxLen, total) || total > RX_CAPACITY) {
      rxLen = 0;
      return;
    }
    if (rxLen < total) return;
    camera_proto::ResponseHeader header{};
    if (camera_proto::parseResponseHeader(rxBuffer + 4, total - 4, header) &&
        header.code == camera_proto::RESP_OK && awaitingAck && header.seq == awaitingSeq) {
      currentMode = awaitingValue;
      awaitingAck = false;
    }
    memmove(rxBuffer, rxBuffer + total, rxLen - total);
    rxLen -= total;
  }
}

void notifyCallback(NimBLERemoteCharacteristic*, uint8_t* data, size_t length, bool) {
  if (rxLen + length > RX_CAPACITY) rxLen = 0;  // drop a desynced partial frame
  memcpy(rxBuffer + rxLen, data, length);
  rxLen += length;
  processRx();
}

class CameraClientCallbacks : public NimBLEClientCallbacks {
  void onDisconnect(NimBLEClient*) override {
    writeChar = nullptr;
    subscribed = false;
    rxLen = 0;
    awaitingAck = false;
    currentMode = -1;
    setStatus("Not connected");
  }
};
CameraClientCallbacks camCallbacks;

bool attemptConnect(const char* mac, uint8_t addrType) {
  if (!mac || !mac[0]) return false;
  if (!camClient) {
    camClient = NimBLEDevice::createClient();
    if (!camClient) {
      setStatus("No BLE capacity");
      return false;
    }
    camClient->setConnectTimeout(5);
    camClient->setClientCallbacks(&camCallbacks, false);
  }
  writeChar = nullptr;
  subscribed = false;
  rxLen = 0;
  if (camClient->isConnected()) camClient->disconnect();
  if (!camClient->connect(NimBLEAddress(std::string(mac), addrType))) {
    setStatus("Not connected");
    return false;
  }
  NimBLERemoteService* service = camClient->getService(CAM_SERVICE_UUID);
  NimBLERemoteCharacteristic* w = service ? service->getCharacteristic(CAM_WRITE_UUID) : nullptr;
  NimBLERemoteCharacteristic* n = service ? service->getCharacteristic(CAM_NOTIFY_UUID) : nullptr;
  if (!w || !n || !n->canNotify()) {
    camClient->disconnect();
    setStatus("Not connected");
    return false;
  }
  writeChar = w;
  subscribed = n->subscribe(true, notifyCallback);
  if (!subscribed) {
    camClient->disconnect();
    writeChar = nullptr;
    setStatus("Not connected");
    return false;
  }
  sendSync();
  sendKeepalive();
  lastKeepaliveMs = millis();
  setStatus("Camera connected");
  Serial.printf("[CAM] Connected to %s\n", mac);
  return true;
}

}  // namespace

void initCameraControl() { setStatus("Not connected"); }

void cameraControlOnAdvertised(const char* name, const char* mac, uint8_t addrType,
                               bool advertisesControlService) {
  if (!advertisesControlService) return;
  std::lock_guard<std::mutex> lock(mtx);
  if (!pairRequested) return;
  snprintf(pendingMac, sizeof(pendingMac), "%s", mac);
  pendingType = addrType;
  pendingConnect = true;
  pairRequested = false;
  Serial.printf("[CAM] Found camera %s (%s)\n", name ? name : "?", mac);
}

void cameraControlPair() {
  std::lock_guard<std::mutex> lock(mtx);
  pairRequested = true;
  Serial.println("[CAM] Pairing requested -- scanning for camera.");
}

void cameraControlForget() {
  g_settings.camera_mac[0] = '\0';
  g_settings.camera_addr_type = 0;
  saveSettings();
  {
    std::lock_guard<std::mutex> lock(mtx);
    pendingConnect = false;
    pairRequested = false;
  }
  if (camClient && camClient->isConnected()) camClient->disconnect();
  setStatus("Not connected");
}

bool cameraControlIsConnected() { return camClient && camClient->isConnected() && subscribed; }

bool cameraControlIsPairing() {
  std::lock_guard<std::mutex> lock(mtx);
  return pairRequested;
}

int cameraControlCurrentMode() { return currentMode; }

const char* cameraControlStatusText() { return statusText; }

bool setCameraMode(int scope, int subMode) {
  if (!cameraControlIsConnected() || !writeChar) return false;
  const int mainMode = scope == camera_proto::SCOPE_PHOTO ? camera_proto::MAIN_MODE_PHOTO
                                                          : camera_proto::MAIN_MODE_VIDEO;
  const uint16_t subOption = scope == camera_proto::SCOPE_PHOTO ? camera_proto::OPT_PHOTO_SUB_MODE
                                                                : camera_proto::OPT_VIDEO_SUB_MODE;
  // main_mode first, then the sub-mode -- matches observed camera behaviour.
  bool ok = sendSetOption(camera_proto::OPT_MAIN_MODE, mainMode);
  ok = sendSetOption(subOption, subMode) && ok;
  return ok;
}

void tickCameraControl() {
  bool wantPair;
  bool havePending;
  char mac[18];
  uint8_t type;
  {
    std::lock_guard<std::mutex> lock(mtx);
    wantPair = pairRequested;
    havePending = pendingConnect;
    memcpy(mac, pendingMac, sizeof(mac));
    type = pendingType;
    pendingConnect = false;
  }

  if (wantPair) {
    if (!pairPrepared) {
      pairPrepared = true;
      pairStartMs = millis();
      // The camera stops advertising its own control service while it is linked
      // to us as the GPS remote, so drop that link and pause our advertising --
      // otherwise the camera reconnects to us instead of advertising be80.
      NimBLEDevice::getAdvertising()->stop();
      disconnectCameraRemotePeer();
      Serial.println("[CAM] Paused advertising + dropped remote link for pairing.");
    }
    NimBLEScan* scan = NimBLEDevice::getScan();
    if (scan && !scan->isScanning()) {
      setStatus("Pairing (10s)");
      scan->start(SCAN_SECONDS, nullptr, false);
    }
    if (millis() - pairStartMs > PAIR_TIMEOUT_MS) {
      pairPrepared = false;
      {
        std::lock_guard<std::mutex> lock(mtx);
        pairRequested = false;
      }
      NimBLEDevice::getAdvertising()->start();
      setStatus("Not connected");
      Serial.println("[CAM] Pairing timed out; advertising resumed.");
    }
    return;
  }

  if (havePending) {
    lastConnectAttemptMs = millis();
    pairPrepared = false;
    bool ok = attemptConnect(mac, type);
    NimBLEDevice::getAdvertising()->start();  // phone (and the remote) can find us again
    if (ok) {
      snprintf(g_settings.camera_mac, sizeof(g_settings.camera_mac), "%s", mac);
      g_settings.camera_addr_type = type;
      saveSettings();
    }
    return;
  }

  if (cameraControlIsConnected()) {
    if (millis() - lastKeepaliveMs >= KEEPALIVE_MS) {
      sendKeepalive();
      lastKeepaliveMs = millis();
    }
    return;
  }

  if (g_settings.camera_mac[0] != '\0' && millis() - lastConnectAttemptMs >= CONNECT_RETRY_MS) {
    if (NimBLEDevice::getScan()->isScanning()) return;
    lastConnectAttemptMs = millis();
    attemptConnect(g_settings.camera_mac, g_settings.camera_addr_type);
  }
}
