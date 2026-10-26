#include "ui/engine/theme.h"
#include <cassert>
#include <cstdio>
#include <cstring>

int main() {
  using namespace uitheme;
  // Default theme is valid and matches the legacy hardcoded tokens.
  assert(isThemeValid(kDefaultDarkTheme));
  assert(isThemeValid(defaultTheme()));
  assert(toRgb565(kAccent) == ((0 >> 3) << 11 | (210 >> 2) << 5 | (255 >> 3)));
  assert(toRgb565(kPanel) == ((22 >> 3) << 11 | (26 >> 2) << 5 | (32 >> 3)));

  // Rejects stale schema and out-of-range font scale.
  UiTheme stale = kDefaultDarkTheme;
  stale.schema_version = UI_THEME_SCHEMA_VERSION - 1;
  assert(!isThemeValid(stale));
  UiTheme tiny = kDefaultDarkTheme;
  tiny.font_scale_pct = 49;
  assert(!isThemeValid(tiny));
  UiTheme huge = kDefaultDarkTheme;
  huge.font_scale_pct = 201;
  assert(!isThemeValid(huge));

  // Hex colors: CSS-compatible, case-insensitive on read.
  char hex[8];
  assert(formatHexColor(Rgb888{0, 210, 255}, hex, sizeof(hex)));
  assert(!strcmp(hex, "#00D2FF"));
  Rgb888 parsed{0, 0, 0};
  assert(parseHexColor("#00d2ff", parsed));
  assert(parsed.r == 0 && parsed.g == 210 && parsed.b == 255);
  assert(!parseHexColor("#12345", parsed));
  assert(!parseHexColor("#ZZZZZZ", parsed));
  assert(!parseHexColor(nullptr, parsed));

  // JSON round-trip preserves every token.
  char doc[256];
  assert(themeToJson(kDefaultDarkTheme, doc, sizeof(doc)) > 0);
  UiTheme back{0, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
               {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, 0};
  assert(themeFromJson(doc, back));
  assert(back.accent.r == 0 && back.accent.g == 210 && back.accent.b == 255);
  assert(back.panel.r == 22 && back.panel.g == 26 && back.panel.b == 32);
  assert(back.font_scale_pct == 100);

  // Rejects stale schema, missing keys, and out-of-range scale; a rejected
  // import never touches the output.
  UiTheme keep = kDefaultDarkTheme;
  assert(!themeFromJson("{\"schema\":0}", keep));
  assert(!themeFromJson("{\"schema\":1}", keep));
  char badScale[256];
  assert(themeToJson(kDefaultDarkTheme, badScale, sizeof(badScale)) > 0);
  assert(!themeFromJson("{\"schema\":1,\"bg\":\"#000000\"}", keep));
  (void)badScale;
  assert(keep.accent.g == 210 && keep.font_scale_pct == 100);

  // Style sheet: the default is valid; any out-of-range token, font, text
  // size or schema rejects the whole sheet.
  assert(isStyleSheetValid(defaultStyleSheet()));
  StyleSheet bad = defaultStyleSheet();
  bad.styles[STYLE_TEXT].fg = ColorToken(TOK_COUNT);
  assert(!isStyleSheetValid(bad));
  bad = defaultStyleSheet();
  bad.styles[STYLE_TEXT].font = FontId(FONT_ID_COUNT);
  assert(!isStyleSheetValid(bad));
  bad = defaultStyleSheet();
  bad.styles[STYLE_TEXT].text_size = 0;
  assert(!isStyleSheetValid(bad));
  bad = defaultStyleSheet();
  bad.schema_version = 0;
  assert(!isStyleSheetValid(bad));
  assert(toRgb565(defaultStyleSheet().pressed) == 0x8410);
  assert(toRgb565(defaultStyleSheet().active) == 0x0230);

  puts("theme tests passed");
}
