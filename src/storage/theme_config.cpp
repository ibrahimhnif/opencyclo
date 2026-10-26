#include "theme_config.h"

#include <Preferences.h>
#include <string.h>

static Preferences themePrefs;

void resetThemeToDefault() {
  g_theme_version++;  // odd: write in progress
  g_theme = uitheme::kDefaultDarkTheme;
  g_theme_version++;
}

void initThemeConfig() {
  themePrefs.begin("opencyclo_ui", false);
  size_t readBytes = themePrefs.getBytes("theme", &g_theme, sizeof(uitheme::UiTheme));
  themePrefs.end();

  // Same migration contract as the layout config: exact byte count plus
  // schema match, otherwise factory default. v1 theme docs stay valid; a
  // future UI_THEME_SCHEMA_VERSION bump resets to defaults here, exactly
  // like UiConfig does today.
  if (readBytes != sizeof(uitheme::UiTheme) || !uitheme::isThemeValid(g_theme)) {
    Serial.println("[THEME CONFIG] NVS theme missing, invalid, or from an older schema — resetting to defaults.");
    resetThemeToDefault();
    saveThemeConfig();
  } else {
    Serial.printf("[THEME CONFIG] Loaded custom theme from NVS Flash (schema v%u).\n",
                  (unsigned)g_theme.schema_version);
  }
}

void saveThemeConfig() {
  themePrefs.begin("opencyclo_ui", false);
  themePrefs.putBytes("theme", &g_theme, sizeof(uitheme::UiTheme));
  themePrefs.end();
  Serial.println("[THEME CONFIG] Saved UiTheme to NVS Flash.");
}

size_t exportThemeToString(char* buffer, size_t maxLen) {
  return uitheme::themeToJson(g_theme, buffer, maxLen);
}

bool importThemeFromString(const char* jsonStr) {
  if (jsonStr == nullptr) return false;
  uitheme::UiTheme staged = uitheme::kDefaultDarkTheme;
  if (!uitheme::themeFromJson(jsonStr, staged)) {
    Serial.println("[THEME CONFIG] Import rejected: invalid theme document.");
    return false;
  }
  g_theme_version++;  // odd: write in progress
  g_theme = staged;
  g_theme_version++;
  saveThemeConfig();
  Serial.println("[THEME CONFIG] Imported custom theme from companion.");
  return true;
}
