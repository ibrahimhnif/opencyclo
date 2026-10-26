#include "power_screen.h"
#include "ui/components/button.h"
#include "ui/components/label.h"

void drawPowerScreen(Gfx& g,const powerui::View& v,int pressed,const ui::ResolvedTheme& t) {
  using namespace uitheme;
  g.fillScreen(t.colors[TOK_BG]);
  g.setTextPadding(0);
  ui::drawLabel(g,12,14,"Power",STYLE_SCREEN_TITLE,t);
  ui::drawLabel(g,12,50,v.battery,STYLE_SCREEN_TEXT,t);
  ui::drawLabel(g,12,72,v.usb,STYLE_SCREEN_TEXT,t);
  ui::drawLabel(g,12,90,v.charge,STYLE_SCREEN_TEXT,t);
  ui::drawLabel(g,12,116,v.warning,STYLE_SCREEN_TEXT,t);
  for(int i=0;i<powerui::kButtons;i++)
    ui::drawButton(g,ui::powerRect(i),{powerui::buttonLabel(i),powerui::buttonIcon(i),true,pressed==i},
      STYLE_BUTTON_POWER,t);
  ui::drawLabel(g,12,282,v.footer,STYLE_SCREEN_TEXT,t);
}
