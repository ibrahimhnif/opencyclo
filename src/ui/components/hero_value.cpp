#include "ui/components/hero_value.h"

#include "ui/components/label.h"

namespace ui {

void drawHeroValue(Gfx& g, const HitRect& b, const wmodel::HeroModel& m, bool force, const ResolvedTheme& t) {
  const ResolvedStyle& label = t.styles[uitheme::STYLE_HERO_LABEL];
  const ResolvedStyle& value = t.styles[uitheme::STYLE_HERO_VALUE];
  if (force) g.fillRect(b.x, b.y, b.w, b.h, value.bg);
  drawLabel(g, b.x + label.text_dx, b.y + label.text_dy, m.source, uitheme::STYLE_HERO_LABEL, t,
            wmodel::TONE_STYLE, b.w - 8);
  // The value's erase stops 70px short of the edge so it never wipes the unit.
  drawLabel(g, b.x + value.text_dx, b.y + value.text_dy, m.value, uitheme::STYLE_HERO_VALUE, t,
            wmodel::TONE_STYLE, b.w - 70);
  g.setFont(label.font);
  g.setTextColor(label.fg, label.bg);
  g.setCursor(b.x + b.w - 44, b.y + b.h - 18);
  g.print(m.unit);
}

}  // namespace ui
