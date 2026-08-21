#ifndef OPENCYCLO_HARDWARE_BATTERY_H
#define OPENCYCLO_HARDWARE_BATTERY_H

#include <Arduino.h>

void initBatteryADC();
float readBatteryVoltage();
uint8_t readBatteryPercentage();

#endif // OPENCYCLO_HARDWARE_BATTERY_H
