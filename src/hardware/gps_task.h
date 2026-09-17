#ifndef OPENCYCLO_HARDWARE_GPS_TASK_H
#define OPENCYCLO_HARDWARE_GPS_TASK_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "core/telemetry_state.h"

extern QueueHandle_t g_gps_queue;
extern QueueHandle_t g_phone_gps_queue;

void startGpsTask();
void gpsTaskLoop(void* pvParameters);
// Bounded synchronous handshake; called only after logger/BLE quiesce.
// False leaves normal GPS processing enabled and cancels power off.
bool prepareGpsForPowerOff();
// Called from the 0x190A BLE write handler (ble_layout_sync.cpp) whenever
// the app reports a new phone position. Non-blocking, overwrite semantics --
// only the latest sample matters.
void setPhoneGpsSample(double lat, double lon, float accuracyM);

#endif // OPENCYCLO_HARDWARE_GPS_TASK_H
