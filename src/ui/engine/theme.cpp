#include "ui/engine/theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace uitheme {

const UiTheme& defaultTheme() {
  return kDefaultDarkTheme;
}

bool formatHexColor(Rgb888 c, char* out, size_t n) {
  if (out == nullptr || n < 8) return false;
  snprintf(out, n, "#%02X%02X%02X", c.r, c.g, c.b);
  return true;
}

bool parseHexColor(const char* s, Rgb888& out) {
  if (s == nullptr) return false;
  if (*s == '#') s++;
  if (strlen(s) != 6) return false;
  char* end = nullptr;
  unsigned long v = strtoul(s, &end, 16);
  if (end == nullptr || *end != '\0') return false;
  out.r = static_cast<uint8_t>((v >> 16) & 0xFF);
  out.g = static_cast<uint8_t>((v >> 8) & 0xFF);
  out.b = static_cast<uint8_t>(v & 0xFF);
  return true;
}

namespace {

bool readColor(const char* json, const char* key, Rgb888& out) {
  char pattern[16];
  snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
  const char* p = strstr(json, pattern);
  if (p == nullptr) return false;
  p += strlen(pattern);
  char buf[10];
  size_t i = 0;
  while (*p != '\0' && *p != '"' && i + 1 < sizeof(buf)) buf[i++] = *p++;
  buf[i] = '\0';
  if (*p != '"') return false;
  return parseHexColor(buf, out);
}

void writeColor(char* out, size_t n, size_t& pos, const char* key, Rgb888 c, bool last) {
  char hex[8];
  formatHexColor(c, hex, sizeof(hex));
  pos += snprintf(out + pos, n > pos ? n - pos : 0, "\"%s\":\"%s\"%s", key, hex, last ? "" : ",");
}

}  // namespace

size_t themeToJson(const UiTheme& t, char* out, size_t n) {
  if (out == nullptr || n == 0) return 0;
  size_t pos = 0;
  pos += snprintf(out + pos, n - pos, "{\"schema\":%u,", (unsigned)t.schema_version);
  writeColor(out, n, pos, "bg", t.bg, false);
  writeColor(out, n, pos, "panel", t.panel, false);
  writeColor(out, n, pos, "text", t.text, false);
  writeColor(out, n, pos, "label", t.label, false);
  writeColor(out, n, pos, "hairline", t.hairline, false);
  writeColor(out, n, pos, "accent", t.accent, false);
  writeColor(out, n, pos, "success", t.success, false);
  writeColor(out, n, pos, "warning", t.warning, false);
  writeColor(out, n, pos, "danger", t.danger, true);
  pos += snprintf(out + pos, n > pos ? n - pos : 0, ",\"font_scale\":%u}", t.font_scale_pct);
  return (pos > 0 && pos < n) ? pos : 0;
}

bool themeFromJson(const char* json, UiTheme& out) {
  if (json == nullptr || strlen(json) < 10) return false;
  UiTheme staged = kDefaultDarkTheme;
  const char* pSchema = strstr(json, "\"schema\":");
  if (pSchema == nullptr) return false;
  staged.schema_version = static_cast<uint32_t>(atoi(pSchema + 9));
  if (staged.schema_version != UI_THEME_SCHEMA_VERSION) return false;
  if (!readColor(json, "bg", staged.bg)) return false;
  if (!readColor(json, "panel", staged.panel)) return false;
  if (!readColor(json, "text", staged.text)) return false;
  if (!readColor(json, "label", staged.label)) return false;
  if (!readColor(json, "hairline", staged.hairline)) return false;
  if (!readColor(json, "accent", staged.accent)) return false;
  if (!readColor(json, "success", staged.success)) return false;
  if (!readColor(json, "warning", staged.warning)) return false;
  if (!readColor(json, "danger", staged.danger)) return false;
  const char* pScale = strstr(json, "\"font_scale\":");
  if (pScale == nullptr) return false;
  int scale = atoi(pScale + 13);
  if (scale < 50 || scale > 200) return false;
  staged.font_scale_pct = static_cast<uint8_t>(scale);
  if (!isThemeValid(staged)) return false;
  out = staged;
  return true;
}

}  // namespace uitheme

// Active-theme storage owned here (not in storage/theme_config.cpp) so host
// unit tests and page-preview builds link without the Arduino Preferences
// dependency; the NVS layer in theme_config.cpp operates on this same global
// on-device.
#include "storage/theme_config.h"
uitheme::UiTheme g_theme = uitheme::kDefaultDarkTheme;
