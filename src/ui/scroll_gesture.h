#pragma once
#include <algorithm>
#include <cstdlib>
namespace ui {
class ScrollGesture {
  int startX=0,startY=0,lastX=0,lastY=0,startOffset=0;
  bool down=false,moved=false;
  int axis=0;
public:
  int offset=0;
  enum Result { None, Tap, PreviousPage, NextPage };
  bool active() const{return down;}
  void cancel(){down=false;moved=false;axis=0;}
  void clamp(int maximum){offset=std::max(0,std::min(offset,maximum));}
  Result update(bool touched,int x,int y,int maximum,bool scrollable=true) {
    if(touched){
      if(!down){down=true;moved=false;axis=0;startX=lastX=x;startY=lastY=y;startOffset=offset;return None;}
      lastX=x;lastY=y;int dx=x-startX,dy=y-startY;
      if(std::abs(dx)>8 || std::abs(dy)>8){moved=true;if(!axis)axis=std::abs(dy)>=std::abs(dx)?1:2;}
      if(axis==1 && scrollable){offset=startOffset-dy;clamp(maximum);}
      return None;
    }
    if(!down)return None;
    down=false;
    if(!moved)return Tap;
    if(axis==2 && std::abs(lastX-startX)>35)return lastX>startX?PreviousPage:NextPage;
    return None;
  }
  int tapX() const{return startX;}
  int tapY() const{return startY;}
};
}
