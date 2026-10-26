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
