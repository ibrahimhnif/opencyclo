#pragma once
#include "geo.h"

namespace nav {
// Direction of travel, not compass orientation. Presentation-only: never
// changes the GPS fix, route matching, ride state or recorded coordinates.
class PositionHeading {
  Point anchor{};
  uint32_t anchorAt=0,updatedAt=0;
  bool anchored=false,known=false;
  double angle=0;
public:
  void update(bool fix,Point point,float speed,uint32_t now) {
    if(!fix || !valid(point)) {anchored=false;known=false;return;}
    if(!std::isfinite(speed) || speed<3) {anchored=false;return;}
    if(!anchored || now-anchorAt>10000) {anchor=point;anchorAt=now;anchored=true;return;}
    uint32_t dt=now-anchorAt;
    if(dt<500)return;
    double metres=distance(anchor,point);
    if(metres<6)return;
    // Don't derive an arrow from a positional jump or a map viewport pan.
    if(metres>speed/3.6*(dt/1000.0)*2+15) {
      anchor=point;anchorAt=now;known=false;return;
    }
    double dx=x(point)-x(anchor),dy=y(point)-y(anchor);
    double next=std::atan2(dx,-dy); // north=0, east=pi/2 on a north-up map
    if(known)angle+=0.5*std::atan2(std::sin(next-angle),std::cos(next-angle));
    else angle=next;
    known=true;updatedAt=now;anchor=point;anchorAt=now;
  }
  bool available() const{return known;}
  bool moving(float speed,uint32_t now) const{return known && speed>=3 && now-updatedAt<3000;}
  double angleRadians() const{return angle;}
};

struct MarkerPoint {int x,y;};
inline MarkerPoint markerVertex(int x,int y,double angle,double forward,double right) {
  return {int(std::lround(x+std::sin(angle)*forward+std::cos(angle)*right)),
          int(std::lround(y-std::cos(angle)*forward+std::sin(angle)*right))};
}
} // namespace nav
