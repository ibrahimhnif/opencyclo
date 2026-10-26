#pragma once
#include "ride_fakes.h"
#include "hardware/sensor_catalog.h"
#include "hardware/baro_calibration.h"
#include "hardware/camera_protocol.h"
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
extern bool pairing,subscribed;
// Direct camera-control channel (hardware/camera_control.h), faked here.
// Accessor statics (not externs) so every UI test TU can include this header
// without needing to define the counters itself.
inline int& cameraModeCalls(){static int calls=0;return calls;}
inline int& cameraPairCalls(){static int calls=0;return calls;}
inline int& cameraShutterCalls(){static int calls=0;return calls;}
inline int& cameraPowerOffCalls(){static int calls=0;return calls;}
inline int& lastCameraScope(){static int scope=-1;return scope;}
inline int& lastCameraSubMode(){static int sub=-1;return sub;}
inline bool setCameraMode(int scope,int subMode){++cameraModeCalls();lastCameraScope()=scope;lastCameraSubMode()=subMode;return true;}
inline bool cameraControlIsConnected(){return subscribed;}
inline bool cameraControlIsPairing(){return pairing;}
inline void cameraControlPair(){++cameraPairCalls();}
inline int cameraControlCurrentMode(){return lastCameraSubMode();}
inline const char* cameraControlStatusText(){return "Not connected";}
inline bool cameraControlShutter(){++cameraShutterCalls();return subscribed;}
inline bool cameraControlPowerOff(){++cameraPowerOffCalls();return subscribed;}
inline camera_proto::CameraStatus& fakeCameraStatus(){static camera_proto::CameraStatus status;return status;}
inline bool cameraControlStatus(camera_proto::CameraStatus& out){
  out=subscribed?fakeCameraStatus():camera_proto::CameraStatus();return subscribed;}
