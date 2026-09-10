#include "layout_manager.h"
#include <stdio.h>
#include "ui/ride_menu.h"
#include "ui/control_layout.h"
#include "navigation/navigation.h"

namespace {
constexpr uint16_t bg=TFT_BLACK,muted=ui::muted,cyan=ui::accent,panel=ui::panel;
void header(const PageConfig& page,const TelemetryState& state,uint8_t pageIdx,uint8_t totalPages) {
  canvas.fillRect(0,0,240,ui::headerHeight,bg);
  ui::drawIcon(canvas,ui::Icon::Map,10,10,cyan);
  ui::drawIcon(canvas,ui::Icon::Ride,206,10,cyan);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(TFT_WHITE,bg);
  canvas.setTextPadding(0);
  // Clip the actual text, not padded erase: long custom titles cannot paint
  // over either navigation control. No saved title is modified.
  canvas.setClipRect(48,0,144,44);
  canvas.drawString(page.title,48,12);
  canvas.clearClipRect();
  canvas.fillRect(0,ui::statusY,240,18,bg);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  char status[32];
  if(state.gps_has_fix)snprintf(status,sizeof(status),"GPS %u",unsigned(state.satellites>99?99:state.satellites));
  else snprintf(status,sizeof(status),"GPS --");
  canvas.setTextColor(state.gps_has_fix?ui::success:ui::warning,bg);
  canvas.drawString(status,4,ui::statusY);
  canvas.setTextColor(state.ride_state==RIDE_STATE_ACTIVE?ui::success:
    state.ride_state==RIDE_STATE_PAUSED?ui::warning:muted,bg);
  canvas.drawString(state.ride_state==RIDE_STATE_ACTIVE?"REC":
    state.ride_state==RIDE_STATE_PAUSED?"PAUSE":"IDLE",80,ui::statusY);
  snprintf(status,sizeof(status),"%u%%",unsigned(state.battery_pct>100?100:state.battery_pct));
  canvas.setTextColor(muted,bg);canvas.drawString(status,150,ui::statusY);
  canvas.setFont(&fonts::Font0);
  snprintf(status,sizeof(status),"%u/%u",unsigned(pageIdx+1),unsigned(totalPages));
  canvas.drawString(status,216,ui::statusY+5);
}
}

void renderPage(const PageConfig& page,uint8_t pageIdx,uint8_t totalPages,const TelemetryState& state,bool forceFullRedraw) {
  const auto& slots=getTemplateDefinition(page.template_id);
  if(forceFullRedraw)canvas.fillScreen(bg);
  canvas.setTextSize(1);canvas.setTextPadding(0);
  const uint8_t count=page.widget_count<slots.max_slots?page.widget_count:slots.max_slots;
  for(uint8_t i=0;i<count;i++)if(page.widgets[i]!=WIDGET_NONE)
    renderWidget(page.widgets[i],slots.slots[i],state,forceFullRedraw);
  if(slots.has_action_button) {
    const auto& r=slots.action_button_rect;
    const int half=(r.w-8)/2;
    for(int i=0;i<2;i++){
      const int x=r.x+i*(half+8);
      canvas.fillRoundRect(x,r.y,half,r.h,8,i?cyan:panel);
      const uint16_t fg=i?TFT_BLACK:TFT_WHITE;
      ui::drawIcon(canvas,i?ui::Icon::Ride:ui::Icon::Map,x+8,r.y+13,fg);
      canvas.setFont(&fonts::FreeSansBold9pt7b);canvas.setTextColor(fg,i?cyan:panel);
      canvas.drawString(i?"Ride":"Map",x+40,r.y+16);
    }
  }
  header(page,state,pageIdx,totalPages);
}

bool handlePageTouch(const PageConfig& page,int16_t x,int16_t y) {
  if(ui::pageMap.contains(x,y)){openNavigation();return true;}
  if(ui::pageRide.contains(x,y)){openRideMenu();return true;}
  // The title and live status are informational, never invisible buttons.
  if(y<ui::headerHeight||y>=ui::statusY)return false;
  const auto& slots=getTemplateDefinition(page.template_id);
  if(slots.has_action_button){
    const auto& r=slots.action_button_rect;const int half=(r.w-8)/2;
    if(y>=r.y&&y<r.y+r.h){
      if(x>=r.x&&x<r.x+half){openNavigation();return true;}
      if(x>=r.x+half+8&&x<r.x+r.w){openRideMenu();return true;}
    }
  }
  const uint8_t count=page.widget_count<slots.max_slots?page.widget_count:slots.max_slots;
  for(uint8_t i=0;i<count;i++){
    const auto& r=slots.slots[i].rect;
    if(x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h&&
      handleWidgetTouch(page.widgets[i],slots.slots[i],x,y))return true;
  }
  return false;
}
