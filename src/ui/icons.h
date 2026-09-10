#pragma once
#include <cstdint>
#include "colors.h"

// Code-native 24px icons: no font glyph dependency, heap allocation or assets.
// The same paths work on the display, sprite and host layout-test canvas.
namespace ui {
enum class Icon { Back, Play, Pause, Finish, Save, Check, Retry, Minus, Plus,
  Center, List, Map, Ride, Camera, Mode, Screen, Wake, Power, Search, Next };
template<class Surface>
void drawIcon(Surface& dst,Icon icon,int x,int y,uint16_t color) {
  auto line=[&](int a,int b,int c,int d) {
    dst.drawLine(x+a,y+b,x+c,y+d,color);
    // Two-pixel strokes stay inside a 24 x 24 cell.
    dst.drawLine(x+a+1,y+b,x+c+1,y+d,color);
  };
  auto box=[&](int a,int b,int c,int d) {
    line(a,b,c,b);line(c,b,c,d);line(c,d,a,d);line(a,d,a,b);
  };
  auto ring=[&](int a,int b,int r) {
    line(a-r,b-2,a-2,b-r);line(a-2,b-r,a+2,b-r);
    line(a+2,b-r,a+r,b-2);line(a+r,b-2,a+r,b+2);
    line(a+r,b+2,a+2,b+r);line(a+2,b+r,a-2,b+r);
    line(a-2,b+r,a-r,b+2);line(a-r,b+2,a-r,b-2);
  };
  switch(icon) {
    case Icon::Back:line(14,5,7,12);line(7,12,14,19);break;
    case Icon::Next:line(8,5,15,12);line(15,12,8,19);break;
    case Icon::Play:line(7,4,18,12);line(18,12,7,20);line(7,20,7,4);break;
    case Icon::Pause:box(6,4,8,20);box(15,4,17,20);break;
    case Icon::Finish:line(5,3,5,21);line(5,4,19,4);line(19,4,15,9);line(15,9,19,14);line(19,14,5,14);break;
    case Icon::Save:box(3,3,20,21);box(7,3,16,9);box(7,14,16,21);break;
    case Icon::Check:line(4,12,9,17);line(9,17,19,6);break;
    case Icon::Retry:line(4,10,7,4);line(7,4,16,4);line(16,4,20,10);line(20,10,19,17);line(19,17,13,21);line(13,21,6,18);line(4,4,4,10);line(4,10,10,10);break;
    case Icon::Minus:line(5,12,18,12);break;
    case Icon::Plus:line(5,12,18,12);line(12,5,12,19);break;
    case Icon::Center:ring(11,12,6);line(11,2,11,6);line(11,18,11,22);line(1,12,5,12);line(17,12,21,12);break;
    case Icon::List:for(int i=0;i<3;i++){box(2,4+i*7,4,6+i*7);line(8,5+i*7,20,5+i*7);}break;
    case Icon::Map:line(2,5,8,2);line(8,2,15,5);line(15,5,20,2);line(20,2,20,19);line(20,19,15,22);line(15,22,8,19);line(8,19,2,22);line(2,22,2,5);line(8,2,8,19);line(15,5,15,22);break;
    case Icon::Ride:ring(5,17,4);ring(17,17,4);line(5,17,10,7);line(10,7,17,17);line(5,17,17,17);line(10,7,15,7);line(10,7,8,3);line(8,3,5,3);break;
    case Icon::Camera:box(2,7,21,21);line(6,7,9,3);line(9,3,15,3);line(15,3,18,7);ring(11,14,4);break;
    case Icon::Mode:box(2,5,14,19);line(14,10,21,6);line(21,6,21,18);line(21,18,14,14);break;
    case Icon::Screen:box(2,3,21,17);line(12,17,12,21);line(7,21,17,21);break;
    case Icon::Wake:line(13,2,4,14);line(4,14,11,14);line(11,14,9,22);line(9,22,20,9);line(20,9,12,9);line(12,9,13,2);break;
    case Icon::Power:line(11,2,11,12);line(5,5,2,10);line(2,10,3,17);line(3,17,8,21);line(8,21,15,21);line(15,21,20,17);line(20,17,21,10);line(21,10,17,5);break;
    case Icon::Search:ring(9,9,6);line(14,14,21,21);break;
  }
}
}
