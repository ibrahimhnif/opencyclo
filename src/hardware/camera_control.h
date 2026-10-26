#ifndef OPENCYCLO_HARDWARE_CAMERA_CONTROL_H
#define OPENCYCLO_HARDWARE_CAMERA_CONTROL_H

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
bool cameraControlIsPairing();

// Opens a ~10s scan window looking for a camera advertising the control
// service. On a hit the camera MAC is remembered in settings and connected.
void cameraControlPair();

// Forgets the saved camera and disconnects.
void cameraControlForget();

// Sends SetOptions to select a capture mode: `scope` is SCOPE_VIDEO or
// SCOPE_PHOTO (camera_protocol.h) and `subMode` the VideoSubMode/PhotoSubMode
// value. Sets main_mode first so crossing the photo/video boundary works.
// Returns false when not connected.
bool setCameraMode(int scope, int subMode);

// Last sub-mode value the camera acknowledged, or -1 if none this session.
int cameraControlCurrentMode();

// Copies what the camera last reported (battery, storage, capture state,
// active sub-mode), kept current by GetOptions on connect, a 30 s poll, and
// the camera's notifications. Returns false (and an all-unknown status) when
// not connected. Safe to call from the UI task.
bool cameraControlStatus(camera_proto::CameraStatus& out);

// The Shutter button: stops a running capture, takes a photo in a photo mode,
// otherwise starts recording (camera_proto::shutterCommand). False when not
// connected and synced.
bool cameraControlShutter();

// Asks the camera to power off with command 0x37. EXPERIMENTAL: not verified
// on the Ace Pro 2 (see camera_protocol.h CMD_POWER_OFF).
bool cameraControlPowerOff();

// Human-readable connection state for the camera widget header.
const char* cameraControlStatusText();

// Called from the shared scan callback (ble_task.cpp) for every advertisement.
// When `advertisesControlService` is true the device is latched as the pairing
// target; the actual connect happens later in tickCameraControl().
void cameraControlOnAdvertised(const char* name, const char* mac, uint8_t addrType,
                               bool advertisesControlService);

#endif  // OPENCYCLO_HARDWARE_CAMERA_CONTROL_H
