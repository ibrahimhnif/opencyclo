#include "ui/components/button.h"

namespace ui {

void drawButton(Gfx& g, const HitRect& r, const ButtonProps& p, uitheme::StyleId style, const ResolvedTheme& t) {
  const ResolvedStyle& s = t.styles[style];
  const int x = r.x + s.inset;
  const int w = r.w - 2 * s.inset;
  const uint16_t bg = p.pressed ? t.colors[uitheme::TOK_PRESSED] : s.bg;
  g.fillRoundRect(x, r.y, w, r.h, s.radius, bg);
  const bool hasText = p.text != nullptr && p.text[0] != '\0';
  if (p.has_icon) {
    const int iconX = (!hasText || s.icon_dx == uitheme::ICON_CENTER) ? x + (w - 24) / 2 : x + s.icon_dx;
    drawIcon(g, p.icon, iconX, r.y + s.icon_dy, s.icon);
  }
  if (!hasText) return;
  g.setFont(s.font);
  g.setTextSize(s.text_size);
  g.setTextColor(s.fg, bg);
  g.drawString(p.text, x + s.text_dx, r.y + s.text_dy);
  if (s.text_size != 1) g.setTextSize(1);
}

}  // namespace ui
