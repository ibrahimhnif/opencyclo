#include "ui/engine/theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace uitheme {

const UiTheme& defaultTheme() {
  return kDefaultDarkTheme;
}

namespace {

// {bg, fg, icon, border, border_w, radius, font, text_size,
//  inset, icon_dx, icon_dy, text_dx, text_dy}
constexpr ComponentStyle kText{TOK_BG, TOK_TEXT, TOK_TEXT, TOK_BG, 0, 0, FONT_SANS_BOLD_9, 1, 0, 0, 0, 0, 0};
constexpr ComponentStyle kTextMuted{TOK_BG, TOK_LABEL, TOK_LABEL, TOK_BG, 0, 0, FONT_SANS_BOLD_9, 1, 0, 0, 0, 0, 0};
constexpr ComponentStyle kCaption{TOK_BG, TOK_LABEL, TOK_LABEL, TOK_BG, 0, 0, FONT_GLCD, 1, 0, 0, 0, 0, 0};
constexpr ComponentStyle kValueXl{TOK_BG, TOK_TEXT, TOK_TEXT, TOK_BG, 0, 0, FONT_SANS_BOLD_24, 1, 0, 0, 0, 0, 0};
constexpr ComponentStyle kTileLabel{TOK_BG, TOK_LABEL, TOK_LABEL, TOK_HAIRLINE, 1, 0, FONT_SANS_BOLD_9, 1, 0, 0, 0, 4, 4};
constexpr ComponentStyle kTileValue{TOK_BG, TOK_TEXT, TOK_TEXT, TOK_HAIRLINE, 1, 0, FONT_SANS_BOLD_12, 1, 0, 0, 0, 4, 24};
constexpr ComponentStyle kHeroLabel{TOK_BG, TOK_LABEL, TOK_LABEL, TOK_BG, 0, 0, FONT_SANS_BOLD_9, 1, 0, 0, 0, 4, 4};
constexpr ComponentStyle kHeroValue{TOK_BG, TOK_TEXT, TOK_TEXT, TOK_BG, 0, 0, FONT_SANS_BOLD_24, 1, 0, 0, 0, 4, 24};
constexpr ComponentStyle kHeader{TOK_BG, TOK_TEXT, TOK_ACCENT, TOK_BG, 0, 0, FONT_SANS_BOLD_9, 1, 0, 0, 0, 48, 12};
constexpr ComponentStyle kPanelTitle{TOK_PANEL, TOK_TEXT, TOK_TEXT, TOK_PANEL, 0, 8, FONT_SANS_BOLD_12, 1, 0, 0, 0, 0, 0};
constexpr ComponentStyle kPanelText{TOK_PANEL, TOK_TEXT, TOK_TEXT, TOK_PANEL, 0, 8, FONT_SANS_BOLD_9, 1, 0, 0, 0, 0, 0};
constexpr ComponentStyle kScreenTitle{TOK_BG, TOK_TEXT, TOK_TEXT, TOK_BG, 0, 0, FONT_GLCD, 2, 0, 0, 0, 0, 0};
constexpr ComponentStyle kScreenText{TOK_BG, TOK_TEXT, TOK_TEXT, TOK_BG, 0, 0, FONT_GLCD, 1, 0, 0, 0, 0, 0};
constexpr ComponentStyle kButtonPrimary{TOK_ACCENT, TOK_BG, TOK_BG, TOK_ACCENT, 0, 8, FONT_SANS_BOLD_12, 1, 0, 12, 12, 48, 13};
constexpr ComponentStyle kButtonSecondary{TOK_PANEL, TOK_TEXT, TOK_TEXT, TOK_PANEL, 0, 8, FONT_SANS_BOLD_12, 1, 0, 12, 12, 48, 13};
constexpr ComponentStyle kButtonPagePrimary{TOK_ACCENT, TOK_BG, TOK_BG, TOK_ACCENT, 0, 8, FONT_SANS_BOLD_9, 1, 0, 8, 13, 40, 16};
constexpr ComponentStyle kButtonPageSecondary{TOK_PANEL, TOK_TEXT, TOK_TEXT, TOK_PANEL, 0, 8, FONT_SANS_BOLD_9, 1, 0, 8, 13, 40, 16};
constexpr ComponentStyle kButtonPower{TOK_PANEL, TOK_TEXT, TOK_TEXT, TOK_PANEL, 0, 8, FONT_GLCD, 2, 0, 12, 10, 48, 14};
constexpr ComponentStyle kButtonMap{TOK_PANEL, TOK_TEXT, TOK_ACCENT, TOK_PANEL, 0, 8, FONT_GLCD, 1, 2, 5, 10, 34, 18};
constexpr ComponentStyle kButtonMapActive{TOK_ACTIVE, TOK_TEXT, TOK_ACCENT, TOK_ACTIVE, 0, 8, FONT_GLCD, 1, 2, 5, 10, 34, 18};
constexpr ComponentStyle kButtonDanger{TOK_DANGER, TOK_BG, TOK_BG, TOK_DANGER, 0, 8, FONT_SANS_BOLD_9, 1, 0, 6, 12, 34, 16};
constexpr ComponentStyle kButtonCompactPrimary{TOK_ACCENT, TOK_BG, TOK_BG, TOK_ACCENT, 0, 8, FONT_SANS_BOLD_9, 1, 0, 6, 12, 34, 16};
constexpr ComponentStyle kButtonCompactSecondary{TOK_PANEL, TOK_TEXT, TOK_TEXT, TOK_PANEL, 0, 8, FONT_SANS_BOLD_9, 1, 0, 6, 12, 34, 16};

// Order must match StyleId.
constexpr StyleSheet kDefaultStyleSheet{
    UI_STYLE_SCHEMA_VERSION,
    Rgb888{128, 128, 128},  // == RGB565 0x8410, the legacy pressed gray
    Rgb888{0, 68, 128},     // == RGB565 0x0230, the legacy map-follow fill
    {kText, kTextMuted, kCaption, kValueXl, kTileLabel, kTileValue, kHeroLabel, kHeroValue, kHeader,
     kPanelTitle, kPanelText, kScreenTitle, kScreenText, kButtonPrimary, kButtonSecondary,
     kButtonPagePrimary, kButtonPageSecondary, kButtonPower, kButtonMap, kButtonMapActive,
     kButtonDanger, kButtonCompactPrimary, kButtonCompactSecondary}};

}  // namespace

const StyleSheet& defaultStyleSheet() {
  return kDefaultStyleSheet;
}

bool isStyleSheetValid(const StyleSheet& s) {
  if (s.schema_version != UI_STYLE_SCHEMA_VERSION) return false;
  for (const ComponentStyle& c : s.styles) {
    if (c.bg >= TOK_COUNT || c.fg >= TOK_COUNT || c.icon >= TOK_COUNT || c.border >= TOK_COUNT) return false;
    if (c.font >= FONT_ID_COUNT) return false;
    if (c.text_size < 1 || c.text_size > 4) return false;
  }
  return true;
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
std::atomic<uint32_t> g_theme_version{0};
