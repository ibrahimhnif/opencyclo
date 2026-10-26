#pragma once
#include "ui/engine/resolved_theme.h"

namespace ui {

// One line of text at (x, y) in `style`'s font and colors. `tone` overrides
// the style's foreground; `padding` erases that many pixels behind the text so
// a shorter value overwrites a longer one without a full redraw.
void drawLabel(Gfx& g, int x, int y, const char* text, uitheme::StyleId style,
               const ResolvedTheme& t, wmodel::Tone tone = wmodel::TONE_STYLE, int padding = 0);

}  // namespace ui
