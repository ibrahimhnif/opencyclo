#include "ride_menu.h"
#include "ride_view.h"
#include "hardware/display.h"
#include <cmath>

namespace {
using namespace rideui;
bool opened=false,confirm=false,dirty=true;
Touch touch;
uint32_t lastDraw=0;
Screen lastScreen=Screen::Ready;
constexpr uint16_t panel=ui::panel,muted=ui::muted,accent=ui::accent;
uint16_t screenColor(Screen s) {
  if(s==Screen::Recording || s==Screen::Saved)return ui::success;
  if(s==Screen::Error)return ui::danger;
  if(s==Screen::Paused || s==Screen::NoFile)return ui::warning;
  return accent;
}
void small(const char* text,int x,int y,uint16_t color=muted) {
  canvas.setFont(&fonts::FreeSansBold9pt7b);canvas.setTextColor(color,TFT_BLACK);
  canvas.drawString(text,x,y);
}
void button(const Button& b,const Rect& r,bool primary,Screen screen) {
  if(b.action==Action::None)return;
  const uint16_t bg=touch.pressed()==b.action?0x8410:(primary?accent:panel);
  canvas.fillRoundRect(r.x,r.y,r.w,r.h,8,bg);
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextColor(primary?TFT_BLACK:TFT_WHITE,bg);
  ui::drawIcon(canvas,iconFor(b,screen),r.x+12,r.y+12,primary?TFT_BLACK:TFT_WHITE);
  canvas.drawString(b.label,r.x+48,r.y+13);
}
void render(const TelemetryState& s,const View& v) {
  if(!dirty && v.screen==lastScreen && millis()-lastDraw<250)return;
  dirty=false;lastDraw=millis();lastScreen=v.screen;
  canvas.fillScreen(TFT_BLACK);canvas.setTextSize(1);canvas.setTextPadding(0);
  canvas.setFont(&fonts::FreeSansBold9pt7b);canvas.setTextColor(TFT_WHITE,TFT_BLACK);
  if(v.back)ui::drawIcon(canvas,ui::Icon::Back,10,10,TFT_WHITE);
  canvas.drawString(v.title,46,12);
  const char* status=v.status;
  if(v.screen==Screen::Confirm)status="End ride / save GPX";
  else if(v.screen==Screen::Error)status="Ride stopped / retry";
  small(status,12,44,screenColor(v.screen));
  small("DISTANCE",16,68);
  char number[24];
  float distance=std::isfinite(s.trip_distance_km)?s.trip_distance_km:0;
  snprintf(number,sizeof(number),"%.1f",double(distance<0?0:distance>9999?9999:distance));
  canvas.setFont(&fonts::FreeSansBold24pt7b);canvas.setTextColor(TFT_WHITE,TFT_BLACK);
  canvas.drawString(number,14,90);
  small("km",205,121);
  small("TIME",16,146);small("AVG",142,146);
  unsigned long hours=s.ride_time_s/3600;
  if(hours>99)snprintf(number,sizeof(number),"%luh",hours);
  else snprintf(number,sizeof(number),"%02lu:%02lu:%02lu",hours,
    (unsigned long)(s.ride_time_s/60%60),(unsigned long)(s.ride_time_s%60));
  canvas.setFont(&fonts::FreeSansBold9pt7b);canvas.setTextColor(TFT_WHITE,TFT_BLACK);
  canvas.drawString(number,16,168);
  float average=std::isfinite(s.avg_speed_kmh)?s.avg_speed_kmh:0;
  snprintf(number,sizeof(number),"%.1f",double(average<0?0:average>999?999:average));
  canvas.drawString(number,142,168);
  if(v.screen==Screen::Saving) {
    canvas.fillRoundRect(12,198,216,106,8,panel);
    canvas.setFont(&fonts::FreeSansBold12pt7b);canvas.setTextColor(TFT_WHITE,panel);
    canvas.drawString("Saving GPX...",28,220);
    canvas.setFont(&fonts::FreeSansBold9pt7b);canvas.drawString("Keep device on",28,266);
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
  canvas.setFont(&fonts::Font0);canvas.setTextColor(muted,TFT_BLACK);
  canvas.drawString(footer,6,310);
  canvas.pushSprite(0,0);
}
void dispatch(Action action) {
  switch(action) {
    case Action::Back:opened=false;break;
    case Action::Cancel:confirm=false;break;
    case Action::Finish:confirm=true;break;
    case Action::Save:if(requestFinishRide())confirm=false;break;
    case Action::Start:
    case Action::Resume:setManualRideState(RIDE_STATE_ACTIVE);break;
    case Action::Pause:setManualRideState(RIDE_STATE_PAUSED);break;
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
