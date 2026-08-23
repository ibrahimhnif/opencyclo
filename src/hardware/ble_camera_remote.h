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
// what a 3-second hold of the power button does (a short press instead
// toggles the camera's screen, a separate, unimplemented command). See
// ble_camera_remote.cpp.
void triggerCameraPowerOff();

#endif // OPENCYCLO_HARDWARE_BLE_CAMERA_REMOTE_H
