#ifndef OPENCYCLO_STORAGE_LAYOUT_CONFIG_H
#define OPENCYCLO_STORAGE_LAYOUT_CONFIG_H

#include "ui/engine/layout_manager.h"
#include <stddef.h>

extern UiConfig g_ui_config;

void initLayoutConfig();
void saveLayoutConfig();
void resetLayoutToDefaults();
bool exportLayoutToJson(const char* filepath);
bool importLayoutFromJson(const char* filepath);

// BLE / App String Serialization
size_t exportLayoutToString(char* buffer, size_t maxLen);
bool importLayoutFromString(const char* jsonStr);

#endif // OPENCYCLO_STORAGE_LAYOUT_CONFIG_H
