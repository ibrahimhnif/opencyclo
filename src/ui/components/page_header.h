#pragma once
#include "ui/engine/resolved_theme.h"

namespace ui {

// Dashboard header strip (0..headerHeight): map icon, ride icon, and the page
// title clipped to the space between them so it can never cover either target.
void drawPageHeader(Gfx& g, const char* title, const ResolvedTheme& t);
// Bottom status line at statusY: GPS, ride state, battery, page counter.
void drawStatusBar(Gfx& g, const wmodel::StatusModel& m, const ResolvedTheme& t);

}  // namespace ui
