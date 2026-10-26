#ifndef OPENCYCLO_STORAGE_THEME_CONFIG_H
#define OPENCYCLO_STORAGE_THEME_CONFIG_H

#include <stddef.h>

#include "ui/engine/theme.h"

// Active theme. Init before the UI task starts; the renderer reads it every
// frame, the web serial task writes it. Same lifecycle as g_ui_config.
extern uitheme::UiTheme g_theme;

void initThemeConfig();
void saveThemeConfig();
void resetThemeToDefault();

// JSON document for the browser editor (see ui/engine/theme.h wire format).
size_t exportThemeToString(char* buffer, size_t maxLen);
// Stages and validates first: a rejected import never touches g_theme and is
// never persisted.
bool importThemeFromString(const char* jsonStr);

#endif  // OPENCYCLO_STORAGE_THEME_CONFIG_H
