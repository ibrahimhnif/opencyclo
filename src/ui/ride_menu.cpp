#include "ui/frame_present.h"
#include "ride_menu.h"
#include "ride_view.h"
#include "hardware/display.h"
#include <cmath>
#include "ui/actions.h"
#include "ui/components/button.h"
#include "ui/components/label.h"

namespace {
using namespace rideui;
using uitheme::StyleId;
bool opened=false,confirm=false,dirty=true;
Touch touch;
uint32_t lastDraw=0;
Screen lastScreen=Screen::Ready;
wmodel::Tone screenTone(Screen s) {
  if(s==Screen::Recording || s==Screen::Saved)return wmodel::TONE_SUCCESS;
  if(s==Screen::Error)return wmodel::TONE_DANGER;
  if(s==Screen::Paused || s==Screen::NoFile)return wmodel::TONE_WARNING;
  return wmodel::TONE_ACCENT;
}
void text(const char* s,int x,int y,StyleId style,wmodel::Tone tone=wmodel::TONE_STYLE) {
  ui::drawLabel(canvas,x,y,s,style,ui::activeTheme(),tone);
}
void button(const Button& b,const Rect& r,bool primary,Screen screen) {
  if(b.action==Action::None)return;
  ui::drawButton(canvas,{r.x,r.y,r.w,r.h},{b.label,iconFor(b,screen),true,touch.pressed()==b.action},
    primary?uitheme::STYLE_BUTTON_PRIMARY:uitheme::STYLE_BUTTON_SECONDARY,ui::activeTheme());
}
void render(const TelemetryState& s,const View& v) {
  if(!dirty && v.screen==lastScreen && millis()-lastDraw<250)return;
  dirty=false;lastDraw=millis();lastScreen=v.screen;
  const auto& t=ui::activeTheme();
  canvas.fillScreen(t.colors[uitheme::TOK_BG]);canvas.setTextSize(1);canvas.setTextPadding(0);
  if(v.back)ui::drawIcon(canvas,ui::Icon::Back,10,10,t.colors[uitheme::TOK_TEXT]);
  text(v.title,46,12,uitheme::STYLE_TEXT);
  const char* status=v.status;
  if(v.screen==Screen::Confirm)status="End ride / save GPX";
  else if(v.screen==Screen::Error)status="Ride stopped / retry";
  text(status,12,44,uitheme::STYLE_TEXT_MUTED,screenTone(v.screen));
  text("DISTANCE",16,68,uitheme::STYLE_TEXT_MUTED);
  char number[24];
  float distance=std::isfinite(s.trip_distance_km)?s.trip_distance_km:0;
  snprintf(number,sizeof(number),"%.1f",double(distance<0?0:distance>9999?9999:distance));
  text(number,14,90,uitheme::STYLE_VALUE_XL);
  text("km",205,121,uitheme::STYLE_TEXT_MUTED);
  text("TIME",16,146,uitheme::STYLE_TEXT_MUTED);text("AVG",142,146,uitheme::STYLE_TEXT_MUTED);
  unsigned long hours=s.ride_time_s/3600;
  if(hours>99)snprintf(number,sizeof(number),"%luh",hours);
  else snprintf(number,sizeof(number),"%02lu:%02lu:%02lu",hours,
    (unsigned long)(s.ride_time_s/60%60),(unsigned long)(s.ride_time_s%60));
  text(number,16,168,uitheme::STYLE_TEXT);
  float average=std::isfinite(s.avg_speed_kmh)?s.avg_speed_kmh:0;
  snprintf(number,sizeof(number),"%.1f",double(average<0?0:average>999?999:average));
  text(number,142,168,uitheme::STYLE_TEXT);
  if(v.screen==Screen::Saving) {
    canvas.fillRoundRect(12,198,216,106,t.styles[uitheme::STYLE_PANEL_TITLE].radius,t.colors[uitheme::TOK_PANEL]);
    text("Saving GPX...",28,220,uitheme::STYLE_PANEL_TITLE);
    text("Keep device on",28,266,uitheme::STYLE_PANEL_TEXT);
  } else {
    button(v.primary,primaryRect,true,v.screen);button(v.secondary,secondaryRect,false,v.screen);
  }
  const char* hint="Recording and navigation are separate";
  if(v.screen==Screen::Confirm)hint="Save ends this ride. No data deleted.";
  else if(v.screen==Screen::Ready && !s.gps_has_fix)hint="GPS points begin when a fix is ready";
  else if(v.screen==Screen::Error)hint="Check SD card and available space";
  else if(v.screen==Screen::NoFile)hint="Check SD card / enable SD logging";
  else if(v.screen==Screen::Saved)hint=s.ride_file;
  char footer[39];snprintf(footer,sizeof(footer),"%.38s",hint);
  text(footer,6,310,uitheme::STYLE_CAPTION);
  presentFrame();
}
// Menu-local actions stay here; ride commands go through dispatchAction().
void dispatch(Action action) {
  switch(action) {
    case Action::Back:opened=false;break;
    case Action::Cancel:confirm=false;break;
    case Action::Finish:confirm=true;break;
    case Action::Save:if(dispatchAction(ACT_RIDE_SAVE).ok)confirm=false;break;
    case Action::Start:dispatchAction(ACT_RIDE_START);break;
    case Action::Resume:dispatchAction(ACT_RIDE_RESUME);break;
    case Action::Pause:dispatchAction(ACT_RIDE_PAUSE);break;
    default:break;
  }
}
}
void openRideMenu(){opened=true;confirm=false;touch.cancel();dirty=true;}
void cancelRideMenuTouch(){touch.cancel();dirty=true;}
bool updateRideMenu(bool touched,int16_t x,int16_t y) {
  if(!opened)return false;
  auto state=getTelemetrySnapshot();
  auto before=touch.pressed();
  Action action=touch.update(touched,x,y,rideui::view(state,confirm));
  if(before!=touch.pressed() || action!=Action::None)dirty=true;
  dispatch(action);
  if(opened){state=getTelemetrySnapshot();render(state,rideui::view(state,confirm));}
  return true; // Consume the release that closes this overlay too.
}
