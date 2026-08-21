#ifndef OPENCYCLO_HARDWARE_GPS_TASK_H
#define OPENCYCLO_HARDWARE_GPS_TASK_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "core/telemetry_state.h"

extern QueueHandle_t g_gps_queue;

void startGpsTask();
void gpsTaskLoop(void* pvParameters);

#endif // OPENCYCLO_HARDWARE_GPS_TASK_H
