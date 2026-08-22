#include "ui_config.h"

bool isUiConfigValid(const UiConfig& cfg, size_t bytesRead) {
  if (bytesRead != sizeof(UiConfig)) return false;
  if (cfg.schema_version != UI_CONFIG_SCHEMA_VERSION) return false;
  if (cfg.active_page_count == 0 || cfg.active_page_count > MAX_PAGES) return false;
  return true;
}
