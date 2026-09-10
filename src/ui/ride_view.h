#pragma once
#include "core/telemetry_state.h"
#include <cstdlib>
#include "icons.h"
namespace rideui {
enum class Screen { Ready, Recording, Paused, Confirm, Saving, Saved, NoFile, Error };
enum class Action { None, Back, Start, Pause, Resume, Finish, Save, Cancel };
struct Button {
  Action action;const char* label;
  Button(Action a=Action::None,const char* l=""):action(a),label(l){}
};
struct View {
  Screen screen;const char* title;const char* status;
  Button primary,secondary;bool back=true;
};
inline View view(const TelemetryState& s,bool confirm) {
  View v;
  switch(s.ride_save) {
    case RIDE_SAVE_PENDING:
      v.screen=Screen::Saving;v.title="Saving ride";v.status="Keep SD card inserted";v.back=false;return v;
    case RIDE_SAVE_OK:
      v.screen=Screen::Saved;v.title="Ride saved";v.status="GPX saved on SD card";
      v.primary={Action::Back,"Done"};v.secondary={Action::Start,"New ride"};return v;
    case RIDE_SAVE_NO_FILE:
      v.screen=Screen::NoFile;v.title="Ride ended";v.status="No GPX file saved";
      v.primary={Action::Back,"Done"};v.secondary={Action::Start,"New ride"};return v;
    case RIDE_SAVE_ERROR:
      v.screen=Screen::Error;v.title="Save failed";v.status="Ride stopped - retry safe";
      v.primary={Action::Save,"Retry save"};v.secondary={Action::Back,"Back"};return v;
    default:break;
  }
  if(confirm && s.ride_state!=RIDE_STATE_IDLE) {
    v.screen=Screen::Confirm;v.title="Finish this ride?";v.status="End recording and save GPX";
    v.primary={Action::Save,"Save GPX"};
    v.secondary={Action::Cancel,"Back"};
  } else if(s.ride_state==RIDE_STATE_ACTIVE) {
    v.screen=Screen::Recording;v.title="Current ride";v.status="RECORDING";
    v.primary={Action::Pause,"Pause"};v.secondary={Action::Finish,"Finish"};
  } else if(s.ride_state==RIDE_STATE_PAUSED) {
    v.screen=Screen::Paused;v.title="Ride paused";v.status=s.ride_auto_allowed?"AUTO PAUSED":"PAUSED";
    v.primary={Action::Resume,"Resume"};v.secondary={Action::Finish,"Finish"};
  } else {
    v.screen=Screen::Ready;v.title="Ready to ride";v.status=s.gps_has_fix?"GPS ready":"Waiting for GPS";
    v.primary={Action::Start,"Start"};v.secondary={Action::Back,"Back"};
  }
  return v;
}
inline ui::Icon iconFor(const Button& b,Screen screen) {
  switch(b.action) {
    case Action::Start:case Action::Resume:return ui::Icon::Play;
    case Action::Pause:return ui::Icon::Pause;
    case Action::Finish:return ui::Icon::Finish;
    case Action::Save:return screen==Screen::Error?ui::Icon::Retry:ui::Icon::Save;
    case Action::Back:return screen==Screen::Saved||screen==Screen::NoFile?ui::Icon::Check:ui::Icon::Back;
    default:return ui::Icon::Back;
  }
}
struct Rect {
  int x,y,w,h;
  bool contains(int px,int py)const{return px>=x && px<x+w && py>=y && py<y+h;}
};
constexpr Rect backRect{0,0,44,44};
constexpr Rect primaryRect{12,198,216,48};
constexpr Rect secondaryRect{12,256,216,48};
inline Action hit(const View& v,int x,int y) {
  if(v.back && backRect.contains(x,y))return v.screen==Screen::Confirm?Action::Cancel:Action::Back;
  if(primaryRect.contains(x,y))return v.primary.action;
  if(secondaryRect.contains(x,y))return v.secondary.action;
  return Action::None;
}
class Touch {
  bool down=false,cancelled=false;int x0=0,y0=0;
  Action held=Action::None;
public:
  void cancel(){down=false;cancelled=false;held=Action::None;}
  Action pressed()const{return down && !cancelled?held:Action::None;}
  Action update(bool touched,int x,int y,const View& v) {
    if(touched) {
      if(!down){down=true;cancelled=false;x0=x;y0=y;held=hit(v,x,y);}
      if(abs(x-x0)>6 || abs(y-y0)>6 || hit(v,x,y)!=held)cancelled=true;
      return Action::None;
    }
    // State changes during a press must not turn it into a different command.
    Action result=down && !cancelled && held==hit(v,x0,y0)?held:Action::None;
    cancel();return result;
  }
};
}
