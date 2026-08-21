#ifndef OPENCYCLO_UI_UI_TASK_H
#define OPENCYCLO_UI_UI_TASK_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

enum UiPage {
  PAGE_RIDE = 0,
  PAGE_CLIMB,
  PAGE_GPS_INFO,
  PAGE_SETTINGS,
  PAGE_COUNT
};

void startUiTask();
void uiTaskLoop(void* pvParameters);

#endif // OPENCYCLO_UI_UI_TASK_H
