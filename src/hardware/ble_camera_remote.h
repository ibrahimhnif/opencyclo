#ifndef OPENCYCLO_HARDWARE_BLE_CAMERA_REMOTE_H
#define OPENCYCLO_HARDWARE_BLE_CAMERA_REMOTE_H

#include <NimBLEDevice.h>

// Adds the reverse-engineered Insta360 "GPS Remote" GATT service to the
// shared peripheral server (see ble_camera_remote.cpp for the protocol
// source and its Ace Pro 2 compatibility caveat). Call once from
// bleTaskLoop()'s setup, alongside initBleLayoutSyncService/initBleOtaService.
void initBleCameraRemoteService(NimBLEServer* pServer);

// Opens a ~30s window where OpenCyclo advertises as "Insta360 GPS Remote"
// instead of "OpenCyclo-GPS", so the camera's own Bluetooth settings screen
// can discover and pair with it. Must be followed by regular tickCameraPairing()
// polling to revert afterward.
void startCameraPairing();

// Polled from bleTaskLoop()'s main loop -- reverts advertising back to the
// normal OpenCyclo identity once the pairing window has elapsed. A no-op
// when no pairing window is open.
void tickCameraPairing();

bool isCameraPairing();

// True while a wake beacon (see wakeSleepingCamera() below) is on the air.
// Mutually exclusive with isCameraPairing() -- they share one revert timer.
bool isCameraWaking();

// True once a peer (expected to be the camera, though this can't be verified
// beyond "something subscribed to our shutter-notify characteristic") has
// subscribed to receive commands.
bool isCameraSubscribed();

// Sends the verified shutter command: takes a photo in photo mode, toggles
// record start/stop in video mode -- this is the camera's own single
// physical shutter button, not two separate commands. See
// ble_camera_remote.cpp for why there's no distinct start/stop.
void triggerCameraShutter();

// Sends the verified mode-cycle command (same as the physical remote's mode
// button).
void triggerCameraMode();

// Sends the verified power-off command -- on the physical remote this is
// what a 3-second hold of the power button does.
void triggerCameraPowerOff();

// Sends the verified screen-toggle command -- a short press of the power
// button; turns the camera's screen on/off without powering it down.
void triggerCameraScreenToggle();

// True once a 6-byte camera-specific wake code has been written (via the
// dedicated GATT characteristic -- any generic BLE tool, e.g. nRF Connect,
// can write it; no phone app required) and persisted to settings.
bool hasCameraWakeBytes();

// Briefly advertises the reference protocol's wake beacon -- a raw
// manufacturer-data payload containing the camera-specific bytes above --
// so a sleeping/powered-off camera that previously bonded with this remote
// notices it and reconnects on its own. No-op (logs and returns) if
// hasCameraWakeBytes() is false. Shares tickCameraPairing()'s revert timer
// with startCameraPairing() (the two are mutually exclusive -- whichever
// was started most recently owns the current window).
void wakeSleepingCamera();

#endif // OPENCYCLO_HARDWARE_BLE_CAMERA_REMOTE_H
