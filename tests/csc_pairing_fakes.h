#pragma once
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <string>
using String=std::string;
inline uint32_t millis(){return 20000;}
struct Settings {
  char paired_csc_mac[18]{},paired_cadence_mac[18]{};
  uint8_t paired_csc_addr_type=0,paired_cadence_addr_type=0;
  uint16_t wheel_circumference_mm=2096;
};
extern Settings g_settings;
extern int saves;
inline void saveSettings(){++saves;}
inline void setCscTelemetry(float,int16_t,uint8_t){}
struct FakeSerial{template<class... T>void printf(const char*,T...) {}};
extern FakeSerial Serial;
struct NimBLEUUID{explicit NimBLEUUID(uint16_t){}};
struct NimBLEAddress{NimBLEAddress(std::string,uint8_t){}};
extern bool connectOk,subscribeOk;
class NimBLEClient;
class NimBLERemoteCharacteristic;
class NimBLERemoteService {
public:
  NimBLEClient* owner=nullptr;
  NimBLEClient* getClient(){return owner;}
  NimBLERemoteCharacteristic* getCharacteristic(NimBLEUUID);
};
class NimBLERemoteCharacteristic {
public:
  NimBLERemoteService* owner=nullptr;
  NimBLERemoteService* getRemoteService(){return owner;}
  bool canNotify(){return true;}
  bool subscribe(bool,void(*)(NimBLERemoteCharacteristic*,uint8_t*,size_t,bool)){return subscribeOk;}
};
class NimBLEClient {
  bool connected=false;
  NimBLERemoteService service;
public:
  NimBLEClient(){service.owner=this;}
  void setConnectTimeout(int){}
  bool connect(NimBLEAddress){return connected=connectOk;}
  bool isConnected(){return connected;}
  void disconnect(){connected=false;}
  NimBLERemoteService* getService(NimBLEUUID){return &service;}
};
inline NimBLERemoteCharacteristic* NimBLERemoteService::getCharacteristic(NimBLEUUID){static NimBLERemoteCharacteristic c;c.owner=this;return &c;}
struct NimBLEDevice{static NimBLEClient* createClient(){return new NimBLEClient();}};
