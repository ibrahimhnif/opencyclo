#ifndef OPENCYCLO_STORAGE_LOGGER_TASK_H
#define OPENCYCLO_STORAGE_LOGGER_TASK_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void startLoggerTask();
void loggerTaskLoop(void* pvParameters);
bool prepareLoggerForPowerOff(uint32_t timeoutMs);
void resumeLoggerAfterPowerOff();

#endif // OPENCYCLO_STORAGE_LOGGER_TASK_H
