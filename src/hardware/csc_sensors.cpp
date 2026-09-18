#include "csc_sensors.h"
#include "csc_decoder.h"
#include "storage/settings.h"
#include "core/telemetry_state.h"
#include <mutex>
namespace {
struct Sensor {
  char mac[18]{};
  uint8_t type=0;
  NimBLEClient* client=nullptr;
  bool subscribed=false;
  CscDecoder decoder;
  uint32_t attempted=0,packets=0,errors=0;
};
Sensor sensors[2];
std::mutex mutex;
bool initialized=false,dirty=false;
unsigned forgetPending=0;
void initialize() {
  if(initialized)return;
  snprintf(sensors[0].mac,18,"%s",g_settings.paired_csc_mac);
  snprintf(sensors[1].mac,18,"%s",g_settings.paired_cadence_mac);
  sensors[0].type=g_settings.paired_csc_addr_type;
  sensors[1].type=g_settings.paired_cadence_addr_type;
  initialized=true;
}
void notification(NimBLERemoteCharacteristic* chr,uint8_t* data,size_t size,bool) {
  {
  std::lock_guard<std::mutex> lock(mutex);
  for(auto& s:sensors)if(s.client==chr->getRemoteService()->getClient()) {
    if(s.decoder.update(data,size,millis(),g_settings.wheel_circumference_mm))++s.packets;
    else ++s.errors;
  }
  }
  // Publish even while the BLE worker is blocked reconnecting another sensor.
  float speed;int16_t cadence;uint8_t connected;
  cscValues(speed,cadence,connected);setCscTelemetry(speed,cadence,connected);
}
}
namespace {
bool connectSlot(unsigned index) {
  auto& s=sensors[index];std::unique_lock<std::mutex> lock(mutex);
    s.attempted=millis();s.decoder=CscDecoder();
    if(!s.client){s.client=NimBLEDevice::createClient();if(!s.client){++s.errors;return false;}s.client->setConnectTimeout(3);}
    auto client=s.client;NimBLEAddress address(std::string(s.mac),s.type);
    lock.unlock();
    bool ok=client->isConnected() || client->connect(address);
    auto service=ok?client->getService(NimBLEUUID(uint16_t(0x1816))):nullptr;
    auto measurement=service?service->getCharacteristic(NimBLEUUID(uint16_t(0x2a5b))):nullptr;
    ok=measurement && measurement->canNotify() && measurement->subscribe(true,notification);
    if(!ok && client->isConnected())client->disconnect();
    lock.lock();s.subscribed=ok;if(!ok)++s.errors;
    Serial.printf("[CSC] %s subscribed=%d errors=%lu\n",s.mac,ok,(unsigned long)s.errors);

  return ok;
}
}
bool pairCscSensor(const char* mac,uint8_t type) {
  unsigned slot=2;
  {std::lock_guard<std::mutex> lock(mutex);initialize();
   for(unsigned i=0;i<2;i++)if(!strcmp(sensors[i].mac,mac))return true;
   for(unsigned i=0;i<2;i++)if(!sensors[i].mac[0]){slot=i;break;}
   if(slot==2)return false;
   snprintf(sensors[slot].mac,18,"%s",mac);sensors[slot].type=type;
  }
  bool ok=connectSlot(slot);
  {std::lock_guard<std::mutex> lock(mutex);
   if(ok){
     char* saved=slot?g_settings.paired_cadence_mac:g_settings.paired_csc_mac;
     snprintf(saved,18,"%s",mac);
     if(slot)g_settings.paired_cadence_addr_type=type;else g_settings.paired_csc_addr_type=type;
     saveSettings();
   } else {sensors[slot].mac[0]=0;sensors[slot].decoder=CscDecoder();}
  }
  return ok;
}
void cscSensorInfo(unsigned slot,bool& connected,uint8_t& capabilities) {
  std::lock_guard<std::mutex> lock(mutex);initialize();connected=false;capabilities=0;
  if(slot>=2)return;auto& s=sensors[slot];
  connected=s.subscribed && s.client && s.client->isConnected();
  capabilities=(s.decoder.wheelSeen?1:0)|(s.decoder.crankSeen?2:0);
}
void forgetCscSensor(unsigned slot){std::lock_guard<std::mutex> lock(mutex);if(slot<2)forgetPending|=1u<<slot;}
void tickCscSensors(bool scanning) {
  // Connect/discover outside locks: notification callbacks use the same mutex.
  // Called only from the sensor task; never inside a NimBLE callback.
  {
    std::unique_lock<std::mutex> lock(mutex);initialize();
    if(forgetPending) {
      unsigned slots=forgetPending;forgetPending=0;
      for(unsigned i=0;i<2;i++) {
        if(!(slots&(1u<<i)))continue;
        auto& s=sensors[i];
        auto client=s.client;s.mac[0]=0;s.subscribed=false;s.decoder=CscDecoder();
        lock.unlock();if(client && client->isConnected())client->disconnect();lock.lock();
      }
      dirty=true;
    }
    if(dirty) {
      snprintf(g_settings.paired_csc_mac,18,"%s",sensors[0].mac);
      snprintf(g_settings.paired_cadence_mac,18,"%s",sensors[1].mac);
      g_settings.paired_csc_addr_type=sensors[0].type;
      g_settings.paired_cadence_addr_type=sensors[1].type;
      dirty=false;saveSettings();
    }
  }
  if(scanning)return;
  for(auto& s:sensors) {
    std::unique_lock<std::mutex> lock(mutex);
    if(!s.mac[0])continue;
    if(s.client && s.client->isConnected() && s.subscribed)continue;
    s.subscribed=false;
    if(s.attempted && millis()-s.attempted<12000)continue;
    unsigned index=unsigned(&s-sensors);lock.unlock();connectSlot(index);
    break; // at most one connect attempt per tick
  }
}
void cscValues(float& speed,int16_t& cadence,uint8_t& connected) {
  std::lock_guard<std::mutex> lock(mutex);speed=-1;cadence=-1;connected=0;
  for(auto& s:sensors)if(s.subscribed && s.client && s.client->isConnected()) {
    ++connected;float v=s.decoder.speedKmh(millis());int c=s.decoder.cadenceRpm(millis());
    if(v>=0)speed=v;if(c>=0)cadence=c;
  }
}
String cscDebug() {
  std::lock_guard<std::mutex> lock(mutex);String result;
  for(auto& s:sensors){char b[150];snprintf(b,sizeof(b),"%s sub=%d wheel=%d crank=%d age=%lu n=%lu err=%lu\n",s.mac,s.subscribed&&s.client&&s.client->isConnected(),s.decoder.wheelSeen,s.decoder.crankSeen,(unsigned long)s.decoder.age(millis()),(unsigned long)s.packets,(unsigned long)s.errors);result+=b;}
  return result;
}
