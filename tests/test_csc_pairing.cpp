#include "csc_pairing_fakes.h"
#include "hardware/csc_sensors.h"
#include <cassert>
Settings g_settings;int saves=0;bool connectOk=false,subscribeOk=false;FakeSerial Serial;
int main(){
  assert(!pairCscSensor("00:00:00:00:00:01",1));assert(saves==0 && !g_settings.paired_csc_mac[0]);
  connectOk=true;assert(!pairCscSensor("00:00:00:00:00:01",1));assert(saves==0);
  subscribeOk=true;assert(pairCscSensor("00:00:00:00:00:01",1));assert(saves==1);
  assert(!strcmp(g_settings.paired_csc_mac,"00:00:00:00:00:01") && g_settings.paired_csc_addr_type==1);
  assert(pairCscSensor("00:00:00:00:00:02",0));assert(saves==2);
  assert(!pairCscSensor("00:00:00:00:00:03",0));assert(saves==2);
  forgetCscSensor(0);tickCscSensors(true);assert(!g_settings.paired_csc_mac[0]);
  assert(!strcmp(g_settings.paired_cadence_mac,"00:00:00:00:00:02"));
  puts("CSC explicit pairing: connect/subscribe failure never saves; two slots and targeted forget passed");
}
