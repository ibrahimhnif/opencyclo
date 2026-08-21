#ifndef OPENCYCLO_CORE_FUSION_TASK_H
#define OPENCYCLO_CORE_FUSION_TASK_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "telemetry_state.h"

void startFusionTask();
void fusionTaskLoop(void* pvParameters);

#endif // OPENCYCLO_CORE_FUSION_TASK_H
