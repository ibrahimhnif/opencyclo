#include "gps_task.h"
#include "config/pins.h"
#include <TinyGPS++.h>

QueueHandle_t g_gps_queue = NULL;

static TinyGPSPlus gps;
static HardwareSerial gpsSerial(1);

void startGpsTask() {
  if (g_gps_queue == NULL) {
    g_gps_queue = xQueueCreate(10, sizeof(GpsFix));
  }

  xTaskCreatePinnedToCore(
    gpsTaskLoop,
    "GpsTask",
    4096,
    NULL,
    2, // Priority 2
    NULL,
    0  // Core 0
  );
}

void gpsTaskLoop(void* pvParameters) {
  gpsSerial.begin(GPS_BAUD_RATE, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  Serial.printf("[GPS TASK] Started on RX=%d, TX=%d @ %u baud\n", PIN_GPS_RX, PIN_GPS_TX, GPS_BAUD_RATE);

  uint32_t lastPushMs = 0;
  uint32_t lastDebugLogMs = 0;
  uint32_t totalChars = 0;

  for (;;) {
    while (gpsSerial.available() > 0) {
      char c = (char)gpsSerial.read();
      totalChars++;
      gps.encode(c);
    }

    uint32_t now = millis();

    if (now - lastDebugLogMs >= 3000) {
      lastDebugLogMs = now;
      Serial.printf("[GPS STATUS] RX Pin:%d | Total Chars:%u | Sentences Passed:%u | Fix:%d | Sats:%u | HDOP:%.2f\n",
                    PIN_GPS_RX, totalChars,
                    (uint32_t)gps.passedChecksum(),
                    gps.location.isValid(),
                    gps.satellites.isValid() ? gps.satellites.value() : 0,
                    gps.hdop.isValid() ? gps.hdop.hdop() : 99.99);
    }

    // Send GPS fix status to queue
    if (now - lastPushMs >= 500 || gps.location.isUpdated()) {
      lastPushMs = now;

      GpsFix fix;
      fix.isValid = gps.location.isValid() && (gps.location.age() < 5000);
      fix.latitude = gps.location.isValid() ? gps.location.lat() : 0.0;
      fix.longitude = gps.location.isValid() ? gps.location.lng() : 0.0;
      fix.speedKmh = gps.speed.isValid() ? (float)gps.speed.kmph() : 0.0f;
      fix.altitudeM = gps.altitude.isValid() ? (float)gps.altitude.meters() : 0.0f;
      fix.hdop = gps.hdop.isValid() ? (float)gps.hdop.hdop() : 99.99f;
      fix.satellites = gps.satellites.isValid() ? gps.satellites.value() : 0;
      
      if (gps.date.isValid() && gps.time.isValid()) {
        fix.year = gps.date.year();
        fix.month = gps.date.month();
        fix.day = gps.date.day();
        fix.hour = gps.time.hour();
        fix.minute = gps.time.minute();
        fix.second = gps.time.second();
      } else {
        fix.year = 0;
        fix.month = 0;
        fix.day = 0;
        fix.hour = 0;
        fix.minute = 0;
        fix.second = 0;
      }
      fix.ageMs = gps.location.age();

      if (g_gps_queue != NULL) {
        xQueueSend(g_gps_queue, &fix, 0);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
