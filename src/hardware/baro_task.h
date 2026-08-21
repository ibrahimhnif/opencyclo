#ifndef OPENCYCLO_HARDWARE_BARO_TASK_H
#define OPENCYCLO_HARDWARE_BARO_TASK_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

struct BaroSample {
  bool isValid;
  float pressureHpa;
  float altitudeM;
  float temperatureC;
  float humidityPct;
};

extern QueueHandle_t g_baro_queue;

void startBaroTask();
void baroTaskLoop(void* pvParameters);

#endif // OPENCYCLO_HARDWARE_BARO_TASK_H
