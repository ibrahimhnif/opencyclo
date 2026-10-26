#include "ui/components/value_tile.h"

#include "ui/components/label.h"

namespace ui {

// Vertical budget inside the shortest (SMALL, 54px) tile: the 9pt label owns
// rows 4..21, the 12pt value rows 24..46; the 2px gap keeps the value's
// background fill off the label's descenders.
void drawValueTile(Gfx& g, const HitRect& b, const wmodel::TileModel& m, bool force, const ResolvedTheme& t) {
  const ResolvedStyle& label = t.styles[uitheme::STYLE_TILE_LABEL];
  const ResolvedStyle& value = t.styles[uitheme::STYLE_TILE_VALUE];
  if (force) {
    g.fillRect(b.x, b.y, b.w, b.h, label.bg);
    if (label.border_w) g.drawFastHLine(b.x, b.y, b.w, label.border);
  }
  drawLabel(g, b.x + label.text_dx, b.y + label.text_dy, m.label, uitheme::STYLE_TILE_LABEL, t,
            wmodel::TONE_STYLE, b.w - 8);
  drawLabel(g, b.x + value.text_dx, b.y + value.text_dy, m.value, uitheme::STYLE_TILE_VALUE, t, m.tone, b.w - 8);
}

}  // namespace ui
