#include "navigation/position_heading.h"
#include <cassert>
#include <cstdio>
int main() {
  nav::PositionHeading h;
  nav::Point p{0,0};
  h.update(true,p,20,0);assert(!h.available());
  h.update(true,{50,0},20,1000);assert(!h.available()); // sub-metre jitter
  h.update(true,{1000,0},20,2000);assert(h.available());
  assert(std::abs(h.angleRadians())<0.001); // north
  auto tip=nav::markerVertex(120,150,h.angleRadians(),11,0);assert(tip.x==120 && tip.y==139);
  h.update(true,{1200,4000},0,2500);assert(std::abs(h.angleRadians())<0.001);
  assert(!h.moving(0,2500)); // hold, never follow stationary jitter
  h.update(false,p,20,3000);assert(!h.available());
  h.update(true,p,20,3100);h.update(true,{0,1000},20,5100);
  assert(std::abs(h.angleRadians()-nav::pi/2)<0.001); // east
  tip=nav::markerVertex(120,150,h.angleRadians(),11,0);assert(tip.x==131 && tip.y==150);
  h.update(true,{10000000,10000000},20,6100);assert(!h.available()); // teleport
  nav::PositionHeading wrap;
  wrap.update(true,p,20,UINT32_MAX-1000);wrap.update(true,{-1000,0},20,1000);
  assert(wrap.available() && std::abs(std::abs(wrap.angleRadians())-nav::pi)<0.001);
  puts("Position heading: cardinal directions, jitter, stop, loss, teleport and clock wrap passed");
}
