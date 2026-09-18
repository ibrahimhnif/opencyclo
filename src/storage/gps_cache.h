#pragma once
#include <stddef.h>
#include <stdint.h>
#include "hardware/gnss_power.h"
// Logger owns filesystem work; GPS task owns receiver work.
void gpsCacheStorageTick(bool mounted);
void gpsCacheCommand(const uint8_t* p,size_t n);
void gpsCacheStatus(uint8_t out[20]);
void gpsCacheGpsTick(gnss::Port& port,bool supported,bool occupied);
void gpsCacheReceive(const gnss::Frame& frame);
bool gpsCacheInjecting();
void gpsCacheCancel();
const char* gpsCacheLabel();
