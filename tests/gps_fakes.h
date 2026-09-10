#pragma once
#include <stdint.h>
#include <cmath>
#include <cstring>
using byte=uint8_t;
using SemaphoreHandle_t=void*;
extern unsigned long fakeTime;
inline unsigned long millis(){return fakeTime;}
inline double radians(double v){return v*3.141592653589793/180;}
inline double degrees(double v){return v*180/3.141592653589793;}
inline double sq(double v){return v*v;}
#define TWO_PI 6.283185307179586
