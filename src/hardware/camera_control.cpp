#include "camera_control.h"
#include "camera_protocol.h"
#include "storage/settings.h"
#include <NimBLEDevice.h>
#include <mutex>
#include <string>
#include <string.h>

// See camera_control.h for the design. The wire format is in camera_protocol.h.

namespace {

// Camera's own control service; OpenCyclo is the central.
const NimBLEUUID CAM_SERVICE_UUID("0000be80-0000-1000-8000-00805f9b34fb");
const NimBLEUUID CAM_WRITE_UUID("0000be81-0000-1000-8000-00805f9b34fb");
const NimBLEUUID CAM_NOTIFY_UUID("0000be82-0000-1000-8000-00805f9b34fb");

// Session timing follows insta360-ace-control (docs/ble-protocol.md there):
// wait for the camera's first heartbeat (or 1.5 s) before SYNC, then
// heartbeat every 0.5-1 s -- the camera drops an idle link after ~30 s.
constexpr uint32_t KEEPALIVE_MS = 1000;
constexpr uint32_t SYNC_WAIT_MS = 1500;
constexpr uint32_t STATUS_DELAY_MS = 300;      // after SYNC, before the first GetOptions
constexpr uint32_t STATUS_POLL_MS = 30000;     // battery has no reliable notification
constexpr uint32_t REFRESH_MIN_GAP_MS = 1000;  // coalesce storage-update bursts
constexpr uint32_t REQUEST_TIMEOUT_MS = 15000; // photos take 4-8 s to answer
constexpr uint32_t CONNECT_RETRY_MS = 12000;
constexpr uint32_t SCAN_SECONDS = 10;
constexpr uint32_t PAIR_TIMEOUT_MS = 30000;
constexpr size_t RX_CAPACITY = 512;
constexpr size_t MAX_PENDING = 8;

NimBLEClient* camClient = nullptr;
NimBLERemoteCharacteristic* writeChar = nullptr;
bool subscribed = false;

bool pairRequested = false;
bool pairPrepared = false;
uint32_t pairStartMs = 0;
bool pendingConnect = false;
char pendingMac[18] = {};
uint8_t pendingType = 0;

uint32_t seqCounter = 1;
uint32_t lastKeepaliveMs = 0;
uint32_t lastConnectAttemptMs = 0;
uint32_t connectedAtMs = 0;
bool synced = false;
uint32_t nextStatusMs = 0;
uint32_t lastRefreshMs = 0;
char statusText[32] = "Not connected";

uint8_t rxBuffer[RX_CAPACITY];
size_t rxLen = 0;

std::mutex mtx;

// Shared between the NimBLE host task (notifyCallback) and the BLE task
// (tick / setCameraMode) and read by the UI task -- guarded by stateMtx.
// SetOptions of the sub-mode carries `value` (the requested sub-mode); every
// other request stores -1.
struct PendingRequest {
  bool used;
  uint32_t seq;
  uint16_t cmd;
  int value;
  uint32_t sentMs;
};
PendingRequest pending[MAX_PENDING] = {};
camera_proto::CameraStatus camStatus;
bool sawFirstFrame = false;
bool refreshRequested = false;
int currentMode = -1;
std::mutex stateMtx;

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

void clearSessionState() {
  std::lock_guard<std::mutex> lock(stateMtx);
  for (PendingRequest& p : pending) p.used = false;
  camStatus = camera_proto::CameraStatus();
  sawFirstFrame = false;
  refreshRequested = false;
  currentMode = -1;
}

// Frames one command, records it as pending, and writes it. Sequences stay in
// 1..254 like insta360-ace-control; a full table drops the oldest request.
bool sendRequest(uint16_t cmd, const uint8_t* pb, size_t pbLen, int value = -1) {
  if (!writeChar) return false;
  uint8_t frame[64];
  uint32_t seq;
  {
    std::lock_guard<std::mutex> lock(stateMtx);
    seq = seqCounter;
    seqCounter = seqCounter >= 254 ? 1 : seqCounter + 1;
    PendingRequest* slot = &pending[0];
    for (PendingRequest& p : pending) {
      if (!p.used) { slot = &p; break; }
      if (p.sentMs - slot->sentMs > 0x7fffffffu) slot = &p;  // older
    }
    *slot = PendingRequest{true, seq, cmd, value, millis()};
  }
  const size_t n = camera_proto::buildCommand(frame, sizeof(frame), cmd, seq, pb, pbLen);
  if (n && writeFrame(frame, n)) return true;
  std::lock_guard<std::mutex> lock(stateMtx);
  for (PendingRequest& p : pending)
    if (p.used && p.seq == seq) p.used = false;
  return false;
}

// SetOptions{option_types=[optionType], value{optionType=value}}. `ackValue`
// becomes currentMode when the camera answers 200.
bool sendSetOption(uint16_t optionType, int value, int ackValue) {
  uint8_t pb[32];
  const size_t pbLen = camera_proto::pbSetOption(pb, sizeof(pb), optionType, value);
  return pbLen && sendRequest(camera_proto::CMD_SET_OPTIONS, pb, pbLen, ackValue);
}

void requestStatus() {
  uint8_t pb[16];
  const size_t pbLen = camera_proto::pbGetOptions(pb, sizeof(pb), camera_proto::STATUS_OPTIONS,
                                                   camera_proto::STATUS_OPTION_COUNT);
  if (pbLen) sendRequest(camera_proto::CMD_GET_OPTIONS, pb, pbLen);
  sendRequest(camera_proto::CMD_GET_CAPTURE_STATUS, nullptr, 0);
  lastRefreshMs = millis();
}

// Caller holds stateMtx. Responses match by sequence; if the camera answers
// with an unknown sequence while exactly one request is pending, that one is
// taken (some firmware replies with seq 0).
void handleResponse(const camera_proto::ResponseHeader& header, const uint8_t* body, size_t len) {
  PendingRequest* match = nullptr;
  PendingRequest* only = nullptr;
  int used = 0;
  for (PendingRequest& p : pending) {
    if (!p.used) continue;
    ++used;
    only = &p;
    if (p.seq == header.seq) match = &p;
  }
  if (!match && used == 1) match = only;
  if (!match) return;
  match->used = false;
  if (header.code != camera_proto::RESP_OK) {
    Serial.printf("[CAM] cmd %u -> %u\n", unsigned(match->cmd), unsigned(header.code));
    return;
  }
  switch (match->cmd) {
    case camera_proto::CMD_SET_OPTIONS:
      if (match->value >= 0) currentMode = match->value;
      break;
    case camera_proto::CMD_GET_OPTIONS:
      camera_proto::parseOptionsResponse(body, len, camStatus);
      break;
    case camera_proto::CMD_GET_CAPTURE_STATUS:
      camera_proto::parseCaptureStatusResponse(body, len, camStatus);
      break;
    default:
      camera_proto::applyCommandOk(match->cmd, camStatus);
      break;
  }
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
    const uint8_t* payload = rxBuffer + 4;
    const size_t payloadLen = total - 4;
    {
      std::lock_guard<std::mutex> lock(stateMtx);
      sawFirstFrame = true;  // heartbeat / sync echo / message: the camera is talking
      camera_proto::ResponseHeader header{};
      if (camera_proto::parseResponseHeader(payload, payloadLen, header) &&
          header.type == camera_proto::CMD_HEADER_TYPE_MESSAGE) {
        const uint8_t* body = payload + 12;
        const size_t bodyLen = payloadLen - 12;
        if (header.code < camera_proto::NOTIFY_FIRST) {
          handleResponse(header, body, bodyLen);
        } else if (camera_proto::applyNotification(header.code, body, bodyLen, camStatus) ==
                   camera_proto::NOTIFY_REFRESH_OPTIONS) {
          refreshRequested = true;
        }
      }
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
    synced = false;
    rxLen = 0;
    clearSessionState();
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
  synced = false;
  rxLen = 0;
  clearSessionState();
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
  // SYNC waits for the camera's first heartbeat -- see tickSession().
  connectedAtMs = millis();
  setStatus("Camera connected");
  Serial.printf("[CAM] Connected to %s\n", mac);
  return true;
}

// Connected-session upkeep: SYNC once the camera has spoken (or after
// SYNC_WAIT_MS), then heartbeat, status polling, refreshes requested by
// storage/battery notifications, and expiry of unanswered requests.
void tickSession() {
  const uint32_t now = millis();
  bool heard, refresh;
  {
    std::lock_guard<std::mutex> lock(stateMtx);
    heard = sawFirstFrame;
    refresh = refreshRequested;
    for (PendingRequest& p : pending)
      if (p.used && now - p.sentMs > REQUEST_TIMEOUT_MS) p.used = false;
  }
  if (!synced) {
    if (!heard && now - connectedAtMs < SYNC_WAIT_MS) return;
    sendSync();
    sendKeepalive();
    lastKeepaliveMs = now;
    synced = true;
    nextStatusMs = now + STATUS_DELAY_MS;
    return;
  }
  if (now - lastKeepaliveMs >= KEEPALIVE_MS) {
    sendKeepalive();
    lastKeepaliveMs = now;
  }
  const bool poll = int32_t(now - nextStatusMs) >= 0;
  if (poll || (refresh && now - lastRefreshMs >= REFRESH_MIN_GAP_MS)) {
    {
      std::lock_guard<std::mutex> lock(stateMtx);
      refreshRequested = false;
    }
    requestStatus();
    nextStatusMs = now + STATUS_POLL_MS;
  }
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

int cameraControlCurrentMode() {
  std::lock_guard<std::mutex> lock(stateMtx);
  return currentMode;
}

bool cameraControlShutter() {
  if (!cameraControlIsConnected() || !synced) return false;
  uint16_t cmd;
  {
    std::lock_guard<std::mutex> lock(stateMtx);
    cmd = camera_proto::shutterCommand(camStatus);
  }
  Serial.printf("[CAM] Shutter -> cmd %u\n", unsigned(cmd));
  return sendRequest(cmd, nullptr, 0);
}

bool cameraControlPowerOff() {
  if (!cameraControlIsConnected() || !synced) return false;
  Serial.println("[CAM] Power off (experimental 0x37)");
  return sendRequest(camera_proto::CMD_POWER_OFF, nullptr, 0);
}

bool cameraControlStatus(camera_proto::CameraStatus& out) {
  if (!cameraControlIsConnected()) {
    out = camera_proto::CameraStatus();
    return false;
  }
  std::lock_guard<std::mutex> lock(stateMtx);
  out = camStatus;
  return true;
}

const char* cameraControlStatusText() { return statusText; }

bool setCameraMode(int scope, int subMode) {
  if (!cameraControlIsConnected() || !writeChar) return false;
  const int mainMode = scope == camera_proto::SCOPE_PHOTO ? camera_proto::MAIN_MODE_PHOTO
                                                          : camera_proto::MAIN_MODE_VIDEO;
  const uint16_t subOption = scope == camera_proto::SCOPE_PHOTO ? camera_proto::OPT_PHOTO_SUB_MODE
                                                                : camera_proto::OPT_VIDEO_SUB_MODE;
  // main_mode first, then the sub-mode -- matches observed camera behaviour.
  // The camera confirms the switch with a 0x2053 notification, which updates
  // cameraControlStatus().
  bool ok = sendSetOption(camera_proto::OPT_MAIN_MODE, mainMode, -1);
  ok = sendSetOption(subOption, subMode, subMode) && ok;
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
      if (camClient && camClient->isConnected()) camClient->disconnect();
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
      setStatus("Not connected");
      Serial.println("[CAM] Pairing timed out.");
    }
    return;
  }

  if (havePending) {
    lastConnectAttemptMs = millis();
    pairPrepared = false;
    const bool ok = attemptConnect(mac, type);
    if (ok) {
      snprintf(g_settings.camera_mac, sizeof(g_settings.camera_mac), "%s", mac);
      g_settings.camera_addr_type = type;
      saveSettings();
    }
    return;
  }

  if (cameraControlIsConnected()) {
    tickSession();
    return;
  }

  if (g_settings.camera_mac[0] != '\0' && millis() - lastConnectAttemptMs >= CONNECT_RETRY_MS) {
    if (NimBLEDevice::getScan()->isScanning()) return;
    lastConnectAttemptMs = millis();
    attemptConnect(g_settings.camera_mac, g_settings.camera_addr_type);
  }
}
