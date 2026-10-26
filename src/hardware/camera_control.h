#ifndef OPENCYCLO_HARDWARE_CAMERA_CONTROL_H
#define OPENCYCLO_HARDWARE_CAMERA_CONTROL_H

#include <stddef.h>
#include <stdint.h>
#include "camera_protocol.h"

// Insta360 camera control over BLE as a *central*: the ESP32 scans for the
// camera and connects to its own control service (0000be80-.../be81/be82),
// speaking the reverse-engineered "OneDriver" protocol (see camera_protocol.h)
// -- the same approach as the insta360-ace-control app. This is the only
// camera path: OpenCyclo no longer emulates the "GPS Remote" peripheral, so
// the camera never connects to us.
//
// Runs on the shared NimBLE dual-role stack, alongside the peripheral server
// and the sensor clients (see ble_task.cpp / csc_sensors.cpp). All blocking
// work happens in tickCameraControl(), called from bleTaskLoop(); never from a
// NimBLE scan callback.

void initCameraControl();

// Polled from bleTaskLoop(): drives the pending connect, handshake keepalive,
// and scan bookkeeping. No-op while a power-off is in progress.
void tickCameraControl();

bool cameraControlIsConnected();

// --- Finding the camera ---------------------------------------------------
// Opens a ~10 s scan; every camera seen (control service or camera name) is
// listed with its RSSI. Nothing connects until the user picks one.
void cameraControlScan();
bool cameraControlIsScanning();
// Copies up to `max` cameras found by the current/last scan; returns the count.
size_t cameraControlCandidates(camera_proto::CameraCandidate* out, size_t max);
// Connects to a listed camera (in the BLE task) and remembers it in settings,
// so it reconnects on its own afterwards.
bool cameraControlConnect(const camera_proto::CameraCandidate& camera);
// True while a connect attempt (picked or automatic) is in flight.
bool cameraControlIsConnecting();
// A camera is remembered in settings.
bool cameraControlHasSaved();
// Forgets the saved camera and disconnects.
void cameraControlForget();

// --- Controlling it -------------------------------------------------------
// Each call queues one command for the BLE task and returns false when not
// connected. Results arrive through cameraControlStatus().

// Selects a capture mode: `scope` is SCOPE_VIDEO or SCOPE_PHOTO
// (camera_protocol.h) and `subMode` the VideoSubMode/PhotoSubMode value.
// Sets main_mode first so crossing the photo/video boundary works.
bool setCameraMode(int scope, int subMode);
bool cameraControlStartRecording();
bool cameraControlStopRecording();
// The photo goes PENDING at once, then SAVED (with its file name) or FAILED.
bool cameraControlTakePhoto();
// EXPERIMENTAL: 0x37, not verified on the Ace Pro 2 (camera_protocol.h).
bool cameraControlPowerOff();

// Last sub-mode value the camera acknowledged, or -1 if none this session.
int cameraControlCurrentMode();

// Copies what the camera last reported (battery, storage, capture state,
// active sub-mode, last saved file), kept current by GetOptions on connect, a
// 30 s poll, and the camera's notifications. Returns false (and an
// all-unknown status) when not connected. Safe to call from the UI task.
bool cameraControlStatus(camera_proto::CameraStatus& out);

// Human-readable connection state for logs.
const char* cameraControlStatusText();

// Called from the shared scan callback (ble_task.cpp) for every advertisement
// that looks like an Insta360 camera. Only recorded while a scan is open.
void cameraControlOnAdvertised(const char* name, const char* mac, uint8_t addrType, int8_t rssi);

#endif  // OPENCYCLO_HARDWARE_CAMERA_CONTROL_H
