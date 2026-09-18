#pragma once
#include <stdint.h>
#include <cstring>
#include <cstdio>

enum class SensorKind : uint8_t { Speed, Cadence, Csc, Heart, Power };
inline const char* sensorKindName(SensorKind kind) {
  switch(kind){case SensorKind::Speed:return "Speed";case SensorKind::Cadence:return "Cadence";
    case SensorKind::Heart:return "Heart rate";case SensorKind::Power:return "Power";default:return "Speed/Cadence";}
}
struct SensorRow {
  char name[32]{},mac[18]{};
  SensorKind kind=SensorKind::Csc;
  uint8_t addressType=0,profile=0;
  int rssi=0;
  bool connected=false;
};
struct SensorSnapshot {
  SensorRow found[24]{},paired[4]{};
  unsigned foundCount=0,pairedCount=0;
  bool scanning=false,busy=false,full=false;
  char status[64]{};
};
class SensorCatalog {
public:
  SensorSnapshot snapshot;
  void clearFound(){snapshot.foundCount=0;snapshot.full=false;}
  void discover(const SensorRow& row) {
    for(unsigned i=0;i<snapshot.foundCount;i++) {
      auto& old=snapshot.found[i];
      if(old.addressType==row.addressType && !strcmp(old.mac,row.mac) && old.kind==row.kind){old=row;return;}
    }
    if(snapshot.foundCount==24){snapshot.full=true;return;}
    snapshot.found[snapshot.foundCount++]=row;
  }
};
SensorSnapshot getSensorSnapshot();
bool requestSensorConnect(const SensorRow& row);
