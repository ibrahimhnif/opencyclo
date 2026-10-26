#include "ui/components/label.h"

namespace ui {

void drawLabel(Gfx& g, int x, int y, const char* text, uitheme::StyleId style,
               const ResolvedTheme& t, wmodel::Tone tone, int padding) {
  const ResolvedStyle& s = t.styles[style];
  g.setFont(s.font);
  g.setTextSize(s.text_size);
  g.setTextColor(t.toneColor(tone, style), s.bg);
  g.setTextPadding(padding);
  g.drawString(text, x, y);
  g.setTextPadding(0);
  if (s.text_size != 1) g.setTextSize(1);
}

}  // namespace ui
