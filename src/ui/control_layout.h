#pragma once
#include "icons.h"

namespace ui {
constexpr int headerHeight=44;
constexpr int statusY=302;
struct HitRect {
  int x,y,w,h;
  bool contains(int px,int py)const{return px>=x && px<x+w && py>=y && py<y+h;}
};
// Camera rectangles are relative to the FULL widget, shared by render/touch.
constexpr HitRect pageMap{0,0,44,44},pageRide{196,0,44,44};
inline HitRect captureRect(int index,int width){return {8,40+index*58,width-16,48};}
inline HitRect optionRect(int index,int width){
  const int half=(width-24)/2;
  return {8+(index%2)*(half+8),100+(index/2)*58,half,48};
}
constexpr HitRect powerRect(int index){return {12,132+index*50,216,44};}
// Map controls retain their original action order. Render and hit-test share
// geometry; attribution is below these 44px targets, never part of a button.
constexpr HitRect mapBack{0,0,48,44},mapRoutes{55,0,136,44},mapRide{195,0,45,44};
constexpr HitRect mapControls[]={{0,266,48,44},{48,266,48,44},{96,266,69,44},{165,266,75,44}};
constexpr HitRect listControls[]={{0,266,80,44},{80,266,81,44},{161,266,79,44}};
inline int mapControlHit(int x,int y,bool list) {
  const auto* controls=list?listControls:mapControls;
  for(int i=0;i<(list?3:4);i++)if(controls[i].contains(x,y))return i;
  return -1;
}
}
