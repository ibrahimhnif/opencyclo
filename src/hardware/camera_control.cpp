#include "camera_control.h"
#include "camera_protocol.h"
#include "storage/settings.h"
#include <NimBLEDevice.h>
#include <atomic>
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
constexpr size_t RX_CAPACITY = 512;
constexpr size_t MAX_PENDING = 8;

NimBLEClient* camClient = nullptr;
NimBLERemoteCharacteristic* writeChar = nullptr;
bool subscribed = false;

// Scan list + connect request, guarded by mtx (written from the NimBLE scan
// callback and the UI task, consumed by tickCameraControl()).
bool scanRequested = false;
bool scanStarted = false;
uint32_t scanStartMs = 0;
camera_proto::CameraCandidate candidates[camera_proto::CAMERA_MAX_CANDIDATES];
size_t candidateCount = 0;
bool pendingConnect = false;
char pendingMac[18] = {};
uint8_t pendingType = 0;
std::atomic<bool> connecting{false};

// One queued capture command from the UI, sent by the BLE task so a blocking
// GATT write never stalls the UI task. Guarded by mtx.
enum QueuedOp : uint8_t { OP_NONE = 0, OP_MODE, OP_COMMAND };
QueuedOp queuedOp = OP_NONE;
uint16_t queuedCmd = 0;
int queuedScope = 0;
int queuedSubMode = 0;

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
    camera_proto::applyCommandFailed(match->cmd, camStatus);
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
      camera_proto::applyCommandOk(match->cmd, body, len, camStatus);
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
    for (PendingRequest& p : pending) {
      if (!p.used || now - p.sentMs <= REQUEST_TIMEOUT_MS) continue;
      p.used = false;
      camera_proto::applyCommandFailed(p.cmd, camStatus);
    }
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
  QueuedOp op;
  uint16_t cmd;
  int scope, subMode;
  {
    std::lock_guard<std::mutex> lock(mtx);
    op = queuedOp;
    cmd = queuedCmd;
    scope = queuedScope;
    subMode = queuedSubMode;
    queuedOp = OP_NONE;
  }
  if (op == OP_MODE) {
    const int mainMode = scope == camera_proto::SCOPE_PHOTO ? camera_proto::MAIN_MODE_PHOTO
                                                            : camera_proto::MAIN_MODE_VIDEO;
    const uint16_t subOption = scope == camera_proto::SCOPE_PHOTO ? camera_proto::OPT_PHOTO_SUB_MODE
                                                                  : camera_proto::OPT_VIDEO_SUB_MODE;
    // main_mode first, then the sub-mode -- matches observed camera behaviour.
    // The camera confirms with a 0x2053 notification.
    sendSetOption(camera_proto::OPT_MAIN_MODE, mainMode, -1);
    sendSetOption(subOption, subMode, subMode);
  } else if (op == OP_COMMAND) {
    Serial.printf("[CAM] cmd %u\n", unsigned(cmd));
    if (!sendRequest(cmd, nullptr, 0)) {
      std::lock_guard<std::mutex> lock(stateMtx);
      camera_proto::applyCommandFailed(cmd, camStatus);
    }
  }
}

// Queues `cmd` for the BLE task. A photo shows "saving" from the tap on.
bool queueCommand(uint16_t cmd) {
  if (!cameraControlIsConnected()) return false;
  {
    std::lock_guard<std::mutex> lock(mtx);
    queuedOp = OP_COMMAND;
    queuedCmd = cmd;
  }
  std::lock_guard<std::mutex> lock(stateMtx);
  camera_proto::applyCommandSent(cmd, camStatus);
  return true;
}

}  // namespace

void initCameraControl() { setStatus("Not connected"); }

void cameraControlOnAdvertised(const char* name, const char* mac, uint8_t addrType, int8_t rssi) {
  std::lock_guard<std::mutex> lock(mtx);
  if (!scanRequested || !mac) return;
  camera_proto::CameraCandidate* slot = nullptr;
  for (size_t i = 0; i < candidateCount; ++i)
    if (!strcmp(candidates[i].mac, mac)) slot = &candidates[i];
  if (!slot) {
    if (candidateCount >= camera_proto::CAMERA_MAX_CANDIDATES) return;
    slot = &candidates[candidateCount++];
    Serial.printf("[CAM] Found camera %s (%s)\n", name && name[0] ? name : "?", mac);
  }
  snprintf(slot->name, sizeof(slot->name), "%s", name && name[0] ? name : "Insta360 camera");
  snprintf(slot->mac, sizeof(slot->mac), "%s", mac);
  slot->addr_type = addrType;
  slot->rssi = rssi;
}

void cameraControlScan() {
  std::lock_guard<std::mutex> lock(mtx);
  scanRequested = true;
  scanStarted = false;
  candidateCount = 0;
  Serial.println("[CAM] Scanning for cameras.");
}

bool cameraControlIsScanning() {
  std::lock_guard<std::mutex> lock(mtx);
  return scanRequested;
}

size_t cameraControlCandidates(camera_proto::CameraCandidate* out, size_t max) {
  std::lock_guard<std::mutex> lock(mtx);
  const size_t n = candidateCount < max ? candidateCount : max;
  for (size_t i = 0; i < n; ++i) out[i] = candidates[i];
  return n;
}

bool cameraControlConnect(const camera_proto::CameraCandidate& camera) {
  if (!camera.mac[0]) return false;
  std::lock_guard<std::mutex> lock(mtx);
  snprintf(pendingMac, sizeof(pendingMac), "%s", camera.mac);
  pendingType = camera.addr_type;
  pendingConnect = true;
  scanRequested = false;
  connecting = true;
  return true;
}

bool cameraControlIsConnecting() { return connecting.load(); }

bool cameraControlHasSaved() { return g_settings.camera_mac[0] != '\0'; }

void cameraControlForget() {
  g_settings.camera_mac[0] = '\0';
  g_settings.camera_addr_type = 0;
  saveSettings();
  {
    std::lock_guard<std::mutex> lock(mtx);
    pendingConnect = false;
    queuedOp = OP_NONE;
  }
  if (camClient && camClient->isConnected()) camClient->disconnect();
  setStatus("Not connected");
}

bool cameraControlIsConnected() { return camClient && camClient->isConnected() && subscribed; }

int cameraControlCurrentMode() {
  std::lock_guard<std::mutex> lock(stateMtx);
  return currentMode;
}

bool cameraControlStartRecording() { return queueCommand(camera_proto::CMD_START_CAPTURE); }
bool cameraControlStopRecording() { return queueCommand(camera_proto::CMD_STOP_CAPTURE); }
bool cameraControlTakePhoto() { return queueCommand(camera_proto::CMD_TAKE_PICTURE); }
bool cameraControlPowerOff() { return queueCommand(camera_proto::CMD_POWER_OFF); }

bool cameraControlStatus(camera_proto::CameraStatus& out) {
  if (!cameraControlIsConnected()) {
    out = camera_proto::CameraStatus();
    return false;
  }
  std::lock_guard<std::mutex> lock(stateMtx);
  // Re-stamp the timer base whenever the camera's report (or our own
  // start/stop) changes it, then tick it locally so the REC timer counts
  // every second between the camera's sparse 0x2010 reports.
  static uint32_t reportMs = 0, reportedSecs = 0;
  static int8_t reportedState = -1;
  const uint32_t now = millis();
  if (camStatus.capture_secs != reportedSecs || camStatus.capture_state != reportedState) {
    reportMs = now;
    reportedSecs = camStatus.capture_secs;
    reportedState = camStatus.capture_state;
  }
  out = camStatus;
  out.capture_secs = camera_proto::liveCaptureSecs(camStatus, reportMs, now);
  return true;
}

const char* cameraControlStatusText() { return statusText; }

bool setCameraMode(int scope, int subMode) {
  if (!cameraControlIsConnected()) return false;
  std::lock_guard<std::mutex> lock(mtx);
  queuedOp = OP_MODE;
  queuedScope = scope;
  queuedSubMode = subMode;
  return true;
}

void tickCameraControl() {
  bool wantScan, startScan, havePending;
  uint32_t scanAge = 0;
  char mac[18];
  uint8_t type;
  {
    std::lock_guard<std::mutex> lock(mtx);
    wantScan = scanRequested;
    startScan = scanRequested && !scanStarted;
    if (startScan) {
      scanStarted = true;
      scanStartMs = millis();
    }
    scanAge = millis() - scanStartMs;
    havePending = pendingConnect;
    memcpy(mac, pendingMac, sizeof(mac));
    type = pendingType;
    pendingConnect = false;
  }

  if (wantScan) {
    NimBLEScan* scan = NimBLEDevice::getScan();
    if (startScan) {
      if (camClient && camClient->isConnected()) camClient->disconnect();
      if (scan && !scan->isScanning()) scan->start(SCAN_SECONDS, nullptr, false);
      setStatus("Scanning");
    } else if (scanAge > SCAN_SECONDS * 1000 + 500 || (scan && !scan->isScanning() && scanAge > 1000)) {
      std::lock_guard<std::mutex> lock(mtx);
      scanRequested = false;
      setStatus("Not connected");
    }
    return;
  }

  if (havePending) {
    NimBLEScan* scan = NimBLEDevice::getScan();
    if (scan && scan->isScanning()) scan->stop();
    lastConnectAttemptMs = millis();
    const bool ok = attemptConnect(mac, type);
    if (ok) {
      snprintf(g_settings.camera_mac, sizeof(g_settings.camera_mac), "%s", mac);
      g_settings.camera_addr_type = type;
      saveSettings();
    }
    connecting = false;
    return;
  }

  if (cameraControlIsConnected()) {
    tickSession();
    return;
  }

  if (g_settings.camera_mac[0] != '\0' && millis() - lastConnectAttemptMs >= CONNECT_RETRY_MS) {
    if (NimBLEDevice::getScan()->isScanning()) return;
    lastConnectAttemptMs = millis();
    connecting = true;
    attemptConnect(g_settings.camera_mac, g_settings.camera_addr_type);
    connecting = false;
  }
}
