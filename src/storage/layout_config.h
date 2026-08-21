#ifndef OPENCYCLO_STORAGE_LAYOUT_CONFIG_H
#define OPENCYCLO_STORAGE_LAYOUT_CONFIG_H

#include "ui/engine/layout_manager.h"

extern UiConfig g_ui_config;

void initLayoutConfig();
void saveLayoutConfig();
void resetLayoutToDefaults();
bool exportLayoutToJson(const char* filepath);
bool importLayoutFromJson(const char* filepath);

#endif // OPENCYCLO_STORAGE_LAYOUT_CONFIG_H
