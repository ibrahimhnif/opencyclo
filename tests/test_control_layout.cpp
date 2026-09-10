#include "ui/control_layout.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
struct Surface {
  int lines=0;
  void drawLine(int x,int y,int xx,int yy,uint16_t){
    assert(x>=0&&x<24&&xx>=0&&xx<24&&y>=0&&y<24&&yy>=0&&yy<24);lines++;
  }
};
int main(){
  for(int i=0;i<=int(ui::Icon::Next);i++){
    Surface s;ui::drawIcon(s,ui::Icon(i),0,0,0xffff);assert(s.lines>0);
  }
  for(int w:{232,240}) {
    for(int mode=0;mode<2;mode++)for(int i=0;i<(mode?4:3);i++) {
      const auto r=mode?ui::optionRect(i,w):ui::captureRect(i,w);
      assert(r.w>=44&&r.h>=44&&r.x>=0&&r.y>=0&&r.x+r.w<=w&&r.y+r.h<=258);
      for(int j=0;j<i;j++){
        const auto other=mode?ui::optionRect(j,w):ui::captureRect(j,w);
        assert(r.x>=other.x+other.w||other.x>=r.x+r.w||r.y>=other.y+other.h||other.y>=r.y+r.h);
      }
    }
  }
  for(int i=0;i<4;i++){
    auto r=ui::mapControls[i];assert(r.w>=44&&r.h>=44);
    assert(ui::mapControlHit(r.x+r.w/2,r.y+r.h/2,false)==i);
  }
  for(int x=0;x<240;x++)assert(ui::mapControlHit(x,312,false)==-1);
  for(int i=0;i<3;i++){auto r=ui::powerRect(i);assert(r.h>=44&&r.y+r.h<=280);}
  puts("Icon bounds and shared camera/map/power hit rectangles passed");
}
