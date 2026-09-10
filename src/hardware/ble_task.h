#ifndef OPENCYCLO_HARDWARE_BLE_TASK_H
#define OPENCYCLO_HARDWARE_BLE_TASK_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Shared with ble_camera_remote.cpp, which temporarily swaps the advertised
// name to impersonate an Insta360 camera remote during pairing, then must
// revert to exactly this identity afterward.
#define OPENCYCLO_BLE_NAME "OpenCyclo-GPS"

enum BleProfileType {
  BLE_PROFILE_CSC = 0,
  BLE_PROFILE_HR,
  BLE_PROFILE_POWER
};

struct DiscoveredDevice {
  char name[32];
  char mac[18];
  uint8_t profileType;
  int rssi;
};

extern bool g_ble_scanning;

void startBleTask();
void bleTaskLoop(void* pvParameters);
void triggerBleScan();
void forgetSensorProfile(BleProfileType profile);
bool prepareBleForPowerOff(uint32_t timeoutMs);
void resumeBleAfterPowerOff();
void stopBleForPowerOff();

#endif // OPENCYCLO_HARDWARE_BLE_TASK_H
