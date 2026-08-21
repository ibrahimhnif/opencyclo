#ifndef OPENCYCLO_UI_UI_TASK_H
#define OPENCYCLO_UI_UI_TASK_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void startUiTask();
void uiTaskLoop(void* pvParameters);

#endif // OPENCYCLO_UI_UI_TASK_H
