#include "gps_task.h"
#include "config/pins.h"
#include <TinyGPS++.h>

QueueHandle_t g_gps_queue = NULL;

static TinyGPSPlus gps;
static HardwareSerial gpsSerial(1);

static char nmeaLineBuf[80];
static uint8_t nmeaLineIdx = 0;

// u-blox M10 UBX Command: Set Navigation Refresh Rate to 5Hz (200ms)
static const uint8_t UBX_CFG_RATE_5HZ[] = {
  0xB5, 0x62, 0x06, 0x08, 0x06, 0x00, 0xC8, 0x00, 0x01, 0x00, 0x01, 0x00, 0xDE, 0x6A
};

// u-blox UBX Command: Enable NAV-PVT High Accuracy Output
static const uint8_t UBX_CFG_NAV5_PORTABLE[] = {
  0xB5, 0x62, 0x06, 0x24, 0x24, 0x00, 0xFF, 0xFF, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0xDC
};

static void sendUbloxConfig() {
  Serial.println("[GPS] Sending u-blox M10 5Hz & Multi-GNSS optimization commands...");
  gpsSerial.write(UBX_CFG_RATE_5HZ, sizeof(UBX_CFG_RATE_5HZ));
  delay(50);
  gpsSerial.write(UBX_CFG_NAV5_PORTABLE, sizeof(UBX_CFG_NAV5_PORTABLE));
  delay(50);
}

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

  sendUbloxConfig();

  uint32_t lastPushMs = 0;
  uint32_t lastDebugLogMs = 0;
  uint32_t totalChars = 0;

  for (;;) {
    while (gpsSerial.available() > 0) {
      char c = (char)gpsSerial.read();
      totalChars++;
      gps.encode(c);

      // Accumulate NMEA line into buffer for Live UI Debug Console
      if (c == '\n' || c == '\r') {
        if (nmeaLineIdx > 0) {
          nmeaLineBuf[nmeaLineIdx] = '\0';
          addNmeaDebugLine(nmeaLineBuf);
          nmeaLineIdx = 0;
        }
      } else if (nmeaLineIdx < sizeof(nmeaLineBuf) - 1) {
        nmeaLineBuf[nmeaLineIdx++] = c;
      }
    }

    uint32_t now = millis();

    if (now - lastDebugLogMs >= 1000) {
      lastDebugLogMs = now;
      g_gps_debug.total_chars = totalChars;
      g_gps_debug.sentences_passed = (uint32_t)gps.passedChecksum();
      g_gps_debug.active_rx_pin = PIN_GPS_RX;
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
