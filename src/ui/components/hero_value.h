#pragma once
#include "ui/control_layout.h"
#include "ui/engine/resolved_theme.h"

namespace ui {

// The single headline value (speed): source label, large value, unit in the
// bottom-right corner.
void drawHeroValue(Gfx& g, const HitRect& b, const wmodel::HeroModel& m, bool force, const ResolvedTheme& t);

}  // namespace ui
