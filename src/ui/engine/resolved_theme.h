#ifndef OPENCYCLO_UI_ENGINE_RESOLVED_THEME_H
#define OPENCYCLO_UI_ENGINE_RESOLVED_THEME_H

#include <stdint.h>

#include "ui/engine/theme.h"
#include "ui/engine/widget_model.h"
#include "ui/gfx.h"

namespace ui {

// A ComponentStyle with its tokens turned into RGB565 and its font into a
// pointer, ready to hand to the draw target.
struct ResolvedStyle {
  uint16_t bg;
  uint16_t fg;
  uint16_t icon;
  uint16_t border;
  uint8_t border_w;
  uint8_t radius;
  uint8_t text_size;
  GfxFont font;
  int8_t inset;
  int8_t icon_dx;
  int8_t icon_dy;
  int8_t text_dx;
  int8_t text_dy;
};

struct ResolvedTheme {
  uint16_t colors[uitheme::TOK_COUNT];
  ResolvedStyle styles[uitheme::STYLE_COUNT];
  // TONE_STYLE returns styles[fallback].fg.
  uint16_t toneColor(wmodel::Tone tone, uitheme::StyleId fallback) const;
};

GfxFont fontFor(uitheme::FontId id);
void resolveTheme(const uitheme::UiTheme& t, const uitheme::StyleSheet& s, ResolvedTheme& out);

// Seqlock reader. Re-resolves only when `version` is even (no write in
// progress), equals `versionAfter` (the counter read again after copying `t`)
// and differs from the last resolved one. Returns true when it re-resolved.
bool refreshActiveTheme(const uitheme::UiTheme& t, uint32_t version, uint32_t versionAfter);
// Resolved from the compiled defaults until the first refresh.
const ResolvedTheme& activeTheme();

}  // namespace ui

#endif  // OPENCYCLO_UI_ENGINE_RESOLVED_THEME_H
