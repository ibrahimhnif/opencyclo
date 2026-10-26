#include "ride_fakes.h"
#include "ui/engine/resolved_theme.h"
#include "ui/colors.h"
#include <cassert>
#include <cstdio>

uint32_t fakeMillis = 0;
Canvas canvas;

int main() {
  using namespace uitheme;
  ui::ResolvedTheme r;
  resolveTheme(kDefaultDarkTheme, defaultStyleSheet(), r);
  assert(r.colors[TOK_ACCENT] == ui::accent && r.colors[TOK_PANEL] == ui::panel);
  assert(r.colors[TOK_TEXT] == 0xffff && r.colors[TOK_BG] == 0);
  assert(r.styles[STYLE_BUTTON_PRIMARY].bg == ui::accent && r.styles[STYLE_BUTTON_PRIMARY].fg == 0);
  assert(*r.styles[STYLE_TILE_VALUE].font == fonts::FreeSansBold12pt7b);
  assert(r.toneColor(wmodel::TONE_DANGER, STYLE_TEXT) == ui::danger);
  assert(r.toneColor(wmodel::TONE_STYLE, STYLE_TEXT_MUTED) == ui::muted);

  // Version rules (seqlock): writers bump the counter to odd before writing
  // and to even after. Re-resolve only on a new, even, stable version.
  // Boot: a theme loaded from NVS has version 0 and must still be applied.
  assert(ui::refreshActiveTheme(kDefaultDarkTheme, 0, 0));
  assert(!ui::refreshActiveTheme(kDefaultDarkTheme, 0, 0));  // unchanged -> no work
  UiTheme red = kDefaultDarkTheme;
  red.accent = Rgb888{255, 0, 0};
  assert(!ui::refreshActiveTheme(red, 2, 4));  // torn: version moved during copy
  assert(!ui::refreshActiveTheme(red, 5, 5));  // odd: a write is in progress
  assert(ui::activeTheme().colors[TOK_ACCENT] == ui::accent);
  assert(ui::refreshActiveTheme(red, 6, 6) && ui::activeTheme().colors[TOK_ACCENT] == 0xf800);
  puts("resolved theme tests passed");
}
