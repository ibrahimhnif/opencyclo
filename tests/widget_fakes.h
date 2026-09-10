#pragma once
#include "ride_fakes.h"
extern Canvas tft;
struct Settings {
  int units=0,brightness=100,wheel_circumference_mm=2096;
  bool sd_logging_enabled=true;
  char paired_csc_mac[18]{},paired_hr_mac[18]{},paired_power_mac[18]{};
};
extern Settings g_settings;
extern bool g_ble_scanning;
enum BleProfileType {BLE_PROFILE_CSC,BLE_PROFILE_HR,BLE_PROFILE_POWER};
inline void triggerBleScan(){}
inline void forgetSensorProfile(BleProfileType){}
inline void saveSettings(){}
inline void setDisplayBrightness(int){}
inline float readBatteryVoltage(){return 4.0f;}
inline void openPowerMenu(){}
struct FakeSerial {void println(const char*){}};
extern FakeSerial Serial;
extern bool pairing,waking,subscribed,wakeReady;
extern int cameraCalls[6];
inline bool isCameraPairing(){return pairing;}
inline bool isCameraWaking(){return waking;}
inline bool isCameraSubscribed(){return subscribed;}
inline bool hasCameraWakeBytes(){return wakeReady;}
inline void startCameraPairing(){cameraCalls[0]++;}
inline void triggerCameraShutter(){cameraCalls[1]++;}
inline void triggerCameraMode(){cameraCalls[2]++;}
inline void triggerCameraScreenToggle(){cameraCalls[3]++;}
inline void wakeSleepingCamera(){cameraCalls[4]++;}
inline void triggerCameraPowerOff(){cameraCalls[5]++;}
