#ifndef OPENCYCLO_HARDWARE_GPS_TASK_H
#define OPENCYCLO_HARDWARE_GPS_TASK_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "core/telemetry_state.h"

extern QueueHandle_t g_gps_queue;

void startGpsTask();
void gpsTaskLoop(void* pvParameters);
// Bounded synchronous handshake; called only after logger/BLE quiesce.
// False leaves normal GPS processing enabled and cancels power off.
bool prepareGpsForPowerOff();
void gpsAssistanceCommand(const uint8_t* bytes, size_t length);
void gpsAssistanceStatus(uint8_t out[12]);
void gpsIdentityCommand(const uint8_t* bytes, size_t length);
size_t gpsIdentityRead(uint8_t out[20]);

#endif // OPENCYCLO_HARDWARE_GPS_TASK_H
