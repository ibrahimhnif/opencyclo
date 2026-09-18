#pragma once
#include "ride_fakes.h"
#include "hardware/sensor_catalog.h"
#include "hardware/baro_calibration.h"
inline const char* gpsCacheLabel(){return "perlu sync";}
inline int& fakeCalibrationElevation(){static int value=10000;return value;}
inline float getBaroReference(){return 1013.25f;}
inline bool getBaroAutoEnabled(){return false;}
inline bool baroAutoDone(){return false;}
inline void setBaroAutoEnabled(bool){}
inline BaroCalibrationStatus getBaroCalibrationStatus(){return BaroCalibrationStatus::Ready;}
inline bool requestBaroCalibration(int elevation){fakeCalibrationElevation()=elevation;return true;}
inline SensorSnapshot& fakeSensors(){static SensorSnapshot snapshot;return snapshot;}
inline int& sensorConnectCalls(){static int count=0;return count;}
inline SensorRow& lastSensorConnect(){static SensorRow row;return row;}
inline SensorSnapshot getSensorSnapshot(){return fakeSensors();}
inline bool requestSensorConnect(const SensorRow& row){++sensorConnectCalls();lastSensorConnect()=row;return true;}
extern Canvas tft;
struct Settings {
  int units=0,brightness=100,wheel_circumference_mm=2096;
  bool sd_logging_enabled=true;
  int gps_source_mode=0;
  int baro_source_mode=0;
  int compass_source_mode=0;
  char paired_csc_mac[18]{},paired_hr_mac[18]{},paired_power_mac[18]{};
  char paired_cadence_mac[18]{};
};
extern Settings g_settings;
extern bool g_ble_scanning;
enum BleProfileType {BLE_PROFILE_CSC,BLE_PROFILE_HR,BLE_PROFILE_POWER,BLE_PROFILE_CADENCE};
inline void triggerBleScan(){}
inline void forgetSensorProfile(BleProfileType){}
inline void saveSettings(){}
inline void setGpsSourceMode(unsigned char mode){g_settings.gps_source_mode=mode;}
inline void setBaroSourceMode(unsigned char mode){g_settings.baro_source_mode=mode;}
inline void setCompassSourceMode(unsigned char mode){g_settings.compass_source_mode=mode;}
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
