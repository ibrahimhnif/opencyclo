#include "hardware/sensor_catalog.h"
#include "ui/scroll_gesture.h"
#include <cassert>
#include <cstdio>
int main(){
  SensorCatalog catalog;SensorRow row;snprintf(row.mac,18,"00:11:22:33:44:55");
  catalog.discover(row);row.rssi=-40;catalog.discover(row);
  assert(catalog.snapshot.foundCount==1 && catalog.snapshot.pairedCount==0);
  assert(catalog.snapshot.found[0].rssi==-40);
  for(int i=0;i<40;i++){snprintf(row.mac,18,"00:11:22:33:55:%02d",i);catalog.discover(row);}
  assert(catalog.snapshot.foundCount==24 && catalog.snapshot.full);
  catalog.clearFound();assert(catalog.snapshot.foundCount==0 && !catalog.snapshot.full);
  ui::ScrollGesture g;
  g.update(true,20,150,300);g.update(true,20,20,300);assert(g.offset==130);
  assert(g.update(false,20,20,300)==ui::ScrollGesture::None);
  g.update(true,20,100,300);g.update(true,20,1000,300);assert(g.offset==0);
  g.update(false,20,1000,300);
  g.update(true,20,100,300);g.update(true,20,-1000,300);assert(g.offset==300);
  g.cancel();assert(!g.active());g.clamp(0);assert(g.offset==0);
  puts("Sensor discovery dedup, bound, no implicit pairing and scroll limits passed");
}
