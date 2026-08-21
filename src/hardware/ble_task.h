#ifndef OPENCYCLO_HARDWARE_BLE_TASK_H
#define OPENCYCLO_HARDWARE_BLE_TASK_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void startBleTask();
void bleTaskLoop(void* pvParameters);

#endif // OPENCYCLO_HARDWARE_BLE_TASK_H
