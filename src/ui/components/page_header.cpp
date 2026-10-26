#include "ui/components/page_header.h"

#include "ui/components/label.h"
#include "ui/control_layout.h"
#include "ui/icons.h"

namespace ui {

void drawPageHeader(Gfx& g, const char* title, const ResolvedTheme& t) {
  const ResolvedStyle& s = t.styles[uitheme::STYLE_HEADER];
  g.fillRect(0, 0, 240, headerHeight, s.bg);
  drawIcon(g, Icon::Map, 10, 10, s.icon);
  drawIcon(g, Icon::Ride, 206, 10, s.icon);
  // Clip the text itself, not a padded erase: no saved title is modified.
  g.setClipRect(s.text_dx, 0, 144, headerHeight);
  drawLabel(g, s.text_dx, s.text_dy, title, uitheme::STYLE_HEADER, t);
  g.clearClipRect();
}

void drawStatusBar(Gfx& g, const wmodel::StatusModel& m, const ResolvedTheme& t) {
  g.fillRect(0, statusY, 240, 18, t.colors[uitheme::TOK_BG]);
  drawLabel(g, 4, statusY, m.gps, uitheme::STYLE_TEXT_MUTED, t, m.gps_tone);
  drawLabel(g, 80, statusY, m.ride, uitheme::STYLE_TEXT_MUTED, t, m.ride_tone);
  drawLabel(g, 150, statusY, m.battery, uitheme::STYLE_TEXT_MUTED, t);
  drawLabel(g, 216, statusY + 5, m.page, uitheme::STYLE_CAPTION, t);
}

}  // namespace ui
