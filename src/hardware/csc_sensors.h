#pragma once
#include <NimBLEDevice.h>
bool pairCscSensor(const char* mac,uint8_t addressType);
void cscSensorInfo(unsigned slot,bool& connected,uint8_t& capabilities);
void tickCscSensors(bool scanning);
void forgetCscSensor(unsigned slot);
void cscValues(float& speed,int16_t& cadence,uint8_t& connected);
String cscDebug();
