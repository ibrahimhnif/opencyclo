#pragma once
#include "ui/control_layout.h"
#include "ui/engine/resolved_theme.h"
#include "ui/icons.h"

namespace ui {

struct ButtonProps {
  const char* text;  // empty or null: icon only, centered
  Icon icon;
  bool has_icon;
  bool pressed;  // drawn with the sheet's pressed fill while held
};

// Rounded fill, optional 24px icon and one line of text, all from `style`.
// Hit testing stays with the caller: `r` is the same rect it tests against.
void drawButton(Gfx& g, const HitRect& r, const ButtonProps& p, uitheme::StyleId style, const ResolvedTheme& t);

}  // namespace ui
