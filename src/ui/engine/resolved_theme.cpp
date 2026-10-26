#include "ui/engine/resolved_theme.h"

namespace ui {

namespace {

ResolvedTheme makeDefault() {
  ResolvedTheme r;
  resolveTheme(uitheme::kDefaultDarkTheme, uitheme::defaultStyleSheet(), r);
  return r;
}

ResolvedTheme s_active = makeDefault();
// No real version is ever this, so the first refresh (version 0 at boot) applies.
uint32_t s_activeVersion = 0xffffffffu;

}  // namespace

GfxFont fontFor(uitheme::FontId id) {
  switch (id) {
    case uitheme::FONT_SANS_BOLD_9: return &fonts::FreeSansBold9pt7b;
    case uitheme::FONT_SANS_BOLD_12: return &fonts::FreeSansBold12pt7b;
    case uitheme::FONT_SANS_BOLD_24: return &fonts::FreeSansBold24pt7b;
    default: return &fonts::Font0;
  }
}

uint16_t ResolvedTheme::toneColor(wmodel::Tone tone, uitheme::StyleId fallback) const {
  switch (tone) {
    case wmodel::TONE_TEXT: return colors[uitheme::TOK_TEXT];
    case wmodel::TONE_ACCENT: return colors[uitheme::TOK_ACCENT];
    case wmodel::TONE_SUCCESS: return colors[uitheme::TOK_SUCCESS];
    case wmodel::TONE_WARNING: return colors[uitheme::TOK_WARNING];
    case wmodel::TONE_DANGER: return colors[uitheme::TOK_DANGER];
    case wmodel::TONE_MUTED: return colors[uitheme::TOK_LABEL];
    default: return styles[fallback].fg;
  }
}

void resolveTheme(const uitheme::UiTheme& t, const uitheme::StyleSheet& s, ResolvedTheme& out) {
  using namespace uitheme;
  const Rgb888 tokens[TOK_COUNT] = {t.bg, t.panel, t.text, t.label, t.hairline, t.accent,
                                    t.success, t.warning, t.danger, s.pressed, s.active};
  for (int i = 0; i < TOK_COUNT; i++) out.colors[i] = toRgb565(tokens[i]);
  for (int i = 0; i < STYLE_COUNT; i++) {
    const ComponentStyle& c = s.styles[i];
    ResolvedStyle& r = out.styles[i];
    r.bg = out.colors[c.bg];
    r.fg = out.colors[c.fg];
    r.icon = out.colors[c.icon];
    r.border = out.colors[c.border];
    r.border_w = c.border_w;
    r.radius = c.radius;
    r.text_size = c.text_size;
    r.font = fontFor(c.font);
    r.inset = c.inset;
    r.icon_dx = c.icon_dx;
    r.icon_dy = c.icon_dy;
    r.text_dx = c.text_dx;
    r.text_dy = c.text_dy;
  }
}

bool refreshActiveTheme(const uitheme::UiTheme& t, uint32_t version, uint32_t versionAfter) {
  // Odd = a writer is mid-copy; different = a write landed during our copy.
  if ((version & 1u) || version != versionAfter || version == s_activeVersion) return false;
  resolveTheme(t, uitheme::defaultStyleSheet(), s_active);
  s_activeVersion = version;
  return true;
}

const ResolvedTheme& activeTheme() {
  return s_active;
}

}  // namespace ui
