#pragma once
#include "ui/control_layout.h"
#include "ui/engine/resolved_theme.h"

namespace ui {

// "Label above, value below, hairline on top" dashboard tile. `force` repaints
// the background and hairline; otherwise only the two padded text lines update.
void drawValueTile(Gfx& g, const HitRect& b, const wmodel::TileModel& m, bool force, const ResolvedTheme& t);

}  // namespace ui
