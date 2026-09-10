#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
// One recursive lock covers SD I/O for maps, routes, ride logs and layouts.
extern SemaphoreHandle_t g_sd_mutex;
extern bool g_sd_ready;
void initSdAccess();
struct SdGuard {
  bool locked;
  SdGuard() : locked(g_sd_mutex && xSemaphoreTakeRecursive(g_sd_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {}
  ~SdGuard() { if (locked) xSemaphoreGiveRecursive(g_sd_mutex); }
};
