#include "ride_menu.h"
#include "hardware/display.h"
#include "core/telemetry_state.h"
#include <cstdlib>

namespace {
bool opened=false,confirm=false,down=false,moved=false;
int startX=0,startY=0;
uint32_t lastDraw=0;
int actionAt(int x,int y) {
  if(x<12 || x>=228)return -1;
  if(y>=110 && y<152)return 0;
  if(y>=170 && y<212)return 1;
  if(y>=260 && y<302)return 2;
  return -1;
}
void text(const String& s,int x,int y,uint16_t color=TFT_WHITE) {
  canvas.setTextColor(color,TFT_BLACK);canvas.drawString(s,x,y);
}
void button(int y,const char* title,uint16_t color) {
  canvas.fillRoundRect(12,y,216,42,6,color);
  canvas.setTextColor(TFT_BLACK,color);canvas.drawString(title,24,y+16);
}
void draw(const TelemetryState& s) {
  if(lastDraw && millis()-lastDraw<100)return;
  lastDraw=millis();
  canvas.fillScreen(TFT_BLACK);canvas.setFont(&fonts::Font0);
  canvas.setTextSize(1);canvas.setTextPadding(0);
  text(confirm?"FINISH RIDE?":"RIDE CONTROLS",12,18,TFT_CYAN);
  char summary[64];
  snprintf(summary,sizeof(summary),"%.2f km   %lu:%02lu:%02lu",s.trip_distance_km,
    (unsigned long)(s.ride_time_s/3600),(unsigned long)(s.ride_time_s/60%60),
    (unsigned long)(s.ride_time_s%60));
  text(summary,12,52);
  if(s.ride_save==RIDE_SAVE_PENDING) {
    text("Saving GPX to SD...",12,112,TFT_ORANGE);
    text("Please keep the SD card inserted.",12,140);
  } else if(s.ride_save==RIDE_SAVE_ERROR) {
    text("Save failed. Check SD / free space.",12,88,TFT_ORANGE);
    text("Ride stopped. File kept for retry.",12,112);
    button(170,"RETRY SAVE GPX",TFT_ORANGE);button(260,"BACK",TFT_CYAN);
  } else if(confirm) {
    text("Stop recording and save this ride?",12,88);
    button(170,"SAVE GPX & FINISH",TFT_GREEN);button(260,"CANCEL",TFT_CYAN);
  } else {
    if(s.ride_save==RIDE_SAVE_OK) {
      text("GPX saved",12,80,TFT_GREEN);
      text(String(s.ride_file).substring(0,36),12,222);
      text(String(s.ride_file).substring(36),12,236);
    } else if(s.ride_save==RIDE_SAVE_NO_FILE) {
      text("Ride ended - no open GPX to save.",12,80,TFT_ORANGE);
      text("Check SD and enable SD logging.",12,222);
    }
    button(110,s.ride_state==RIDE_STATE_ACTIVE?"PAUSE RIDE":
      s.ride_state==RIDE_STATE_PAUSED?"RESUME RIDE":"START NEW RIDE",TFT_CYAN);
    if(s.ride_state!=RIDE_STATE_IDLE)button(170,"FINISH RIDE",TFT_ORANGE);
    button(260,"BACK",TFT_CYAN);
  }
  canvas.pushSprite(0,0);
}
}
void openRideMenu(){opened=true;confirm=false;down=false;moved=false;lastDraw=0;}
void cancelRideMenuTouch(){down=false;moved=false;lastDraw=0;}
bool updateRideMenu(bool touched,int16_t x,int16_t y) {
  if(!opened)return false;
  if(touched) {
    if(!down){down=true;moved=false;startX=x;startY=y;}
    if(abs(x-startX)>6 || abs(y-startY)>6)moved=true;
  } else if(down) {
    down=false;
    int action=moved?-1:actionAt(startX,startY);
    auto s=getTelemetrySnapshot();
    if(s.ride_save!=RIDE_SAVE_PENDING) {
      if(action==2){if(confirm)confirm=false;else opened=false;}
      else if(action==1) {
        if(confirm || s.ride_save==RIDE_SAVE_ERROR) {
          if(requestFinishRide())confirm=false;
        } else if(s.ride_state!=RIDE_STATE_IDLE)confirm=true;
      } else if(action==0 && !confirm && s.ride_save!=RIDE_SAVE_ERROR) {
        setManualRideState(s.ride_state==RIDE_STATE_ACTIVE?RIDE_STATE_PAUSED:RIDE_STATE_ACTIVE);
      }
    }
    lastDraw=0;
  }
  if(opened)draw(getTelemetrySnapshot());
  return true;
}
