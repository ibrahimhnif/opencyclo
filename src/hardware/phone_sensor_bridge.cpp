#include "phone_sensor_bridge.h"

QueueHandle_t g_phone_baro_queue = NULL;
QueueHandle_t g_phone_heading_queue = NULL;

void startPhoneSensorBridge() {
  if (g_phone_baro_queue == NULL) {
    g_phone_baro_queue = xQueueCreate(1, sizeof(PhoneAltitudeSample));
  }
  if (g_phone_heading_queue == NULL) {
    g_phone_heading_queue = xQueueCreate(1, sizeof(PhoneHeadingSample));
  }
}

void setPhoneAltitudeSample(float altitudeM) {
  if (g_phone_baro_queue == NULL) return;
  PhoneAltitudeSample sample{altitudeM, millis()};
  xQueueOverwrite(g_phone_baro_queue, &sample);
}

void setPhoneHeadingSample(float headingDeg, uint8_t accuracy) {
  if (g_phone_heading_queue == NULL) return;
  PhoneHeadingSample sample{headingDeg, accuracy, millis()};
  xQueueOverwrite(g_phone_heading_queue, &sample);
}
