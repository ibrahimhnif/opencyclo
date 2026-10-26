#include "layout_manager.h"
#include <stdio.h>
#include "ui/ride_menu.h"
#include "ui/control_layout.h"
#include "navigation/navigation.h"
#include "ui/components/button.h"
#include "ui/components/page_header.h"

namespace {
void header(const PageConfig& page,const TelemetryState& state,uint8_t pageIdx,uint8_t totalPages) {
  const auto& t=ui::activeTheme();
  ui::drawPageHeader(canvas,page.title,t);
  wmodel::StatusModel status;
  wmodel::modelStatus(state.gps_has_fix,state.gps_fix_quality,state.satellites,
    state.ride_state==RIDE_STATE_ACTIVE?wmodel::RIDE_HINT_ACTIVE:
    state.ride_state==RIDE_STATE_PAUSED?wmodel::RIDE_HINT_PAUSED:wmodel::RIDE_HINT_IDLE,
    state.battery_pct,pageIdx,totalPages,status);
  ui::drawStatusBar(canvas,status,t);
}
}

void renderPage(const PageConfig& page,uint8_t pageIdx,uint8_t totalPages,const TelemetryState& state,bool forceFullRedraw) {
  const auto& slots=getTemplateDefinition(page.template_id);
  const auto& t=ui::activeTheme();
  if(forceFullRedraw)canvas.fillScreen(t.colors[uitheme::TOK_BG]);
  canvas.setTextSize(1);canvas.setTextPadding(0);
  const uint8_t count=page.widget_count<slots.max_slots?page.widget_count:slots.max_slots;
  for(uint8_t i=0;i<count;i++)if(page.widgets[i]!=WIDGET_NONE)
    renderWidget(page.widgets[i],slots.slots[i],state,forceFullRedraw);
  if(slots.has_action_button) {
    const auto& r=slots.action_button_rect;
    const int half=(r.w-8)/2;
    ui::drawButton(canvas,{r.x,r.y,half,r.h},{"Map",ui::Icon::Map,true,false},uitheme::STYLE_BUTTON_PAGE_SECONDARY,t);
    ui::drawButton(canvas,{r.x+half+8,r.y,half,r.h},{"Ride",ui::Icon::Ride,true,false},uitheme::STYLE_BUTTON_PAGE_PRIMARY,t);
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
bool handleSensorPageStream(const PageConfig& page,bool touched,int16_t x,int16_t y,int& pageSwipe) {
  const auto& slots=getTemplateDefinition(page.template_id);
  const uint8_t count=page.widget_count<slots.max_slots?page.widget_count:slots.max_slots;
  for(uint8_t i=0;i<count;i++)if(page.widgets[i]==WIDGET_BLE_MANAGER && slots.slots[i].size_class==SIZE_FULL)
    return handleSensorWidgetStream(slots.slots[i].rect,touched,x,y,pageSwipe);
  cancelSensorWidgetTouch();return false;
}
