#ifndef OPENCYCLO_UI_ENGINE_THEME_H
#define OPENCYCLO_UI_ENGINE_THEME_H

#include <stddef.h>
#include <stdint.h>

#define UI_THEME_SCHEMA_VERSION 1u

namespace uitheme {

struct Rgb888 {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

constexpr uint16_t toRgb565(Rgb888 c) {
  return static_cast<uint16_t>(((c.r >> 3) << 11) | ((c.g >> 2) << 5) | (c.b >> 3));
}

// Default dark tokens. RGB888 values match the approved design-system sRGB
// tokens previously hardcoded in ui/colors.h, so the default render is
// pixel-identical; the web editor and firmware now share this single source.
constexpr Rgb888 kBg{0, 0, 0};
constexpr Rgb888 kPanel{22, 26, 32};
constexpr Rgb888 kText{255, 255, 255};
constexpr Rgb888 kLabel{168, 177, 189};
constexpr Rgb888 kHairline{28, 28, 28};
constexpr Rgb888 kAccent{0, 210, 255};
constexpr Rgb888 kSuccess{46, 213, 115};
constexpr Rgb888 kWarning{255, 171, 0};
constexpr Rgb888 kDanger{255, 105, 116};

struct UiTheme {
  uint32_t schema_version;
  Rgb888 bg;
  Rgb888 panel;
  Rgb888 text;
  Rgb888 label;
  Rgb888 hairline;
  Rgb888 accent;
  Rgb888 success;
  Rgb888 warning;
  Rgb888 danger;
  uint8_t font_scale_pct;  // 50..200, 100 = current sizes
};

constexpr UiTheme kDefaultDarkTheme{UI_THEME_SCHEMA_VERSION,
                                    kBg,
                                    kPanel,
                                    kText,
                                    kLabel,
                                    kHairline,
                                    kAccent,
                                    kSuccess,
                                    kWarning,
                                    kDanger,
                                    100};

const UiTheme& defaultTheme();

// ---- Style sheet --------------------------------------------------------
// Components never name a color, font or offset: they draw from a named
// style, and a style references color tokens. The default sheet reproduces
// the literals the screens used before components, so the default render is
// unchanged. Kept separate from UiTheme so stored NVS themes stay valid.

enum ColorToken : uint8_t {
  TOK_BG = 0,
  TOK_PANEL,
  TOK_TEXT,
  TOK_LABEL,
  TOK_HAIRLINE,
  TOK_ACCENT,
  TOK_SUCCESS,
  TOK_WARNING,
  TOK_DANGER,
  TOK_PRESSED,  // StyleSheet::pressed
  TOK_ACTIVE,   // StyleSheet::active
  TOK_COUNT
};

// The compiled-in GFX fonts a style may pick.
enum FontId : uint8_t {
  FONT_GLCD = 0,
  FONT_SANS_BOLD_9,
  FONT_SANS_BOLD_12,
  FONT_SANS_BOLD_24,
  FONT_ID_COUNT
};

enum StyleId : uint8_t {
  STYLE_TEXT = 0,
  STYLE_TEXT_MUTED,
  STYLE_CAPTION,
  STYLE_VALUE_XL,
  STYLE_TILE_LABEL,
  STYLE_TILE_VALUE,
  STYLE_HERO_LABEL,
  STYLE_HERO_VALUE,
  STYLE_HEADER,
  STYLE_PANEL_TITLE,
  STYLE_PANEL_TEXT,
  STYLE_SCREEN_TITLE,
  STYLE_SCREEN_TEXT,
  STYLE_BUTTON_PRIMARY,
  STYLE_BUTTON_SECONDARY,
  STYLE_BUTTON_PAGE_PRIMARY,
  STYLE_BUTTON_PAGE_SECONDARY,
  STYLE_BUTTON_POWER,
  STYLE_BUTTON_MAP,
  STYLE_BUTTON_MAP_ACTIVE,
  STYLE_BUTTON_DANGER,  // record / stop (compact, like the two below)
  STYLE_BUTTON_COMPACT_PRIMARY,    // icon + label in half-width camera buttons
  STYLE_BUTTON_COMPACT_SECONDARY,
  STYLE_COUNT
};

// icon_dx value: center the icon horizontally (used when a button has no text).
constexpr int8_t ICON_CENTER = -1;

// Offsets are relative to the component rect after `inset` is applied on the
// left and right.
struct ComponentStyle {
  ColorToken bg;
  ColorToken fg;
  ColorToken icon;
  ColorToken border;
  uint8_t border_w;
  uint8_t radius;
  FontId font;
  uint8_t text_size;
  int8_t inset;
  int8_t icon_dx;
  int8_t icon_dy;
  int8_t text_dx;
  int8_t text_dy;
};

#define UI_STYLE_SCHEMA_VERSION 1u

struct StyleSheet {
  uint32_t schema_version;
  Rgb888 pressed;  // held-button fill
  Rgb888 active;   // toggled-on control fill (map follow)
  ComponentStyle styles[STYLE_COUNT];
};

const StyleSheet& defaultStyleSheet();

// Schema matches, every token/font is in range, text_size is 1..4.
bool isStyleSheetValid(const StyleSheet& s);

inline bool isThemeValid(const UiTheme& t) {
  if (t.schema_version != UI_THEME_SCHEMA_VERSION) return false;
  if (t.font_scale_pct < 50 || t.font_scale_pct > 200) return false;
  return true;
}

// Hex colors: "#RRGGBB" on the wire (CSS-compatible for the web editor),
// Rgb888 in firmware. Pure and host-testable.
bool formatHexColor(Rgb888 c, char* out, size_t n);
bool parseHexColor(const char* s, Rgb888& out);

// Full theme document: {"schema":1,"bg":"#000000",...,"font_scale":100}.
// themeFromJson stages into a local and only writes out on full success.
size_t themeToJson(const UiTheme& t, char* out, size_t n);
bool themeFromJson(const char* json, UiTheme& out);

}  // namespace uitheme

#endif  // OPENCYCLO_UI_ENGINE_THEME_H
