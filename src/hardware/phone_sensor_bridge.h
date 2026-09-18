#ifndef OPENCYCLO_HARDWARE_PHONE_SENSOR_BRIDGE_H
#define OPENCYCLO_HARDWARE_PHONE_SENSOR_BRIDGE_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Phone-sourced altitude/heading samples, mirroring the phone GPS sample
// pattern in gps_task.h/.cpp (setPhoneGpsSample / g_phone_gps_queue). Kept
// in their own file since neither sensor domain (barometer, compass) owns
// the phone as one of its existing sources.

struct PhoneAltitudeSample {
  float altitudeM;
  uint32_t receivedAtMs;
};

struct PhoneHeadingSample {
  float headingDeg;
  uint8_t accuracy; // 0=low, 1=medium, 2=high; informational only.
  uint32_t receivedAtMs;
};

extern QueueHandle_t g_phone_baro_queue;
extern QueueHandle_t g_phone_heading_queue;

// Creates the queues. Call once during setup(), before BLE starts accepting
// writes on 0x190C/0x190D.
void startPhoneSensorBridge();

// Called from the 0x190C BLE write handler (ble_layout_sync.cpp).
void setPhoneAltitudeSample(float altitudeM);

// Called from the 0x190D BLE write handler (ble_layout_sync.cpp).
void setPhoneHeadingSample(float headingDeg, uint8_t accuracy);

#endif // OPENCYCLO_HARDWARE_PHONE_SENSOR_BRIDGE_H
