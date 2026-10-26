#pragma once
#include <cstdint>
#include <cstdio>
#include "control_layout.h"
#include "icons.h"

// What the power menu shows, computed from battery/USB readings with no
// drawing and no hardware access. Host-tested; power_screen.cpp draws it.
namespace powerui {
struct View {
  char battery[48];
  const char* usb;
  const char* charge;
  const char* warning;
  const char* footer;
};
inline View view(uint8_t pct,float volts,bool hasUsbSense,bool usbConnected,const char* message) {
  View v;
  snprintf(v.battery,sizeof(v.battery),"Battery estimate: %u%%  %.2f V",unsigned(pct),double(volts));
  v.usb=!hasUsbSense?"USB charging is automatic.":usbConnected?"USB power connected":"USB power disconnected";
  v.charge="Charge status unavailable.";
  v.warning="Power off / restart ends this ride.";
  v.footer=message?message:"Wake after power off: press BOOT";
  return v;
}
// Buttons: 0 Power off, 1 Restart, 2 Back.
const int kButtons=3;
inline const char* buttonLabel(int i) {
  static const char* const labels[kButtons]={"Power off","Restart","Back"};
  return i>=0 && i<kButtons?labels[i]:"";
}
inline ui::Icon buttonIcon(int i) {
  return i==0?ui::Icon::Power:i==1?ui::Icon::Retry:ui::Icon::Back;
}
inline int hit(int x,int y) {
  for(int i=0;i<kButtons;i++)if(ui::powerRect(i).contains(x,y))return i;
  return -1;
}
}
