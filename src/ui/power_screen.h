#pragma once
#include "power_view.h"
#include "ui/engine/resolved_theme.h"

// Draws the whole power menu frame; `pressed` is the held button index or -1.
void drawPowerScreen(Gfx& g,const powerui::View& v,int pressed,const ui::ResolvedTheme& t);
