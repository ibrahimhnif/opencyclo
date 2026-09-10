#include "sd_access.h"
SemaphoreHandle_t g_sd_mutex = nullptr;
bool g_sd_ready = false;
void initSdAccess() { if (!g_sd_mutex) g_sd_mutex = xSemaphoreCreateRecursiveMutex(); }
