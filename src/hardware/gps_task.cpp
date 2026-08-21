#include "gps_task.h"
#include "config/pins.h"
#include <TinyGPS++.h>

QueueHandle_t g_gps_queue = NULL;

static TinyGPSPlus gps;
static HardwareSerial gpsSerial(1);

struct GpsProbeConfig {
  int rxPin;
  int txPin;
  uint32_t baud;
};

static const GpsProbeConfig PROBE_CONFIGS[] = {
  {44, 43, 115200},
  {43, 44, 115200},
  {44, 43, 9600},
  {43, 44, 9600}
};
static const size_t NUM_PROBES = sizeof(PROBE_CONFIGS) / sizeof(PROBE_CONFIGS[0]);

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
  size_t currentProbeIdx = 0;
  bool isLocked = false;

  gpsSerial.begin(PROBE_CONFIGS[0].baud, SERIAL_8N1, PROBE_CONFIGS[0].rxPin, PROBE_CONFIGS[0].txPin);
  Serial.printf("[GPS] Probing RX=%d, TX=%d @ %u baud...\n",
                PROBE_CONFIGS[0].rxPin, PROBE_CONFIGS[0].txPin, PROBE_CONFIGS[0].baud);

  uint32_t lastPushMs = 0;
  uint32_t lastDebugLogMs = 0;
  uint32_t probeStartMs = millis();
  uint32_t totalChars = 0;

  for (;;) {
    while (gpsSerial.available() > 0) {
      char c = (char)gpsSerial.read();
      totalChars++;
      gps.encode(c);

      if (!isLocked) {
        isLocked = true;
        Serial.printf("\n[GPS NMEA ARRIVED!] RxPin=%d TxPin=%d Baud=%u\n",
                      PROBE_CONFIGS[currentProbeIdx].rxPin,
                      PROBE_CONFIGS[currentProbeIdx].txPin,
                      PROBE_CONFIGS[currentProbeIdx].baud);
      }
    }

    uint32_t now = millis();

    // If 0 characters received after 3 seconds, try next pin/baud combo
    if (!isLocked && (now - probeStartMs >= 3000)) {
      currentProbeIdx = (currentProbeIdx + 1) % NUM_PROBES;
      probeStartMs = now;
      gpsSerial.end();
      gpsSerial.begin(PROBE_CONFIGS[currentProbeIdx].baud, SERIAL_8N1,
                      PROBE_CONFIGS[currentProbeIdx].rxPin, PROBE_CONFIGS[currentProbeIdx].txPin);
      Serial.printf("[GPS PROBE] Trying RX=%d, TX=%d @ %u baud...\n",
                    PROBE_CONFIGS[currentProbeIdx].rxPin,
                    PROBE_CONFIGS[currentProbeIdx].txPin,
                    PROBE_CONFIGS[currentProbeIdx].baud);
    }

    if (now - lastDebugLogMs >= 3000) {
      lastDebugLogMs = now;
      Serial.printf("[GPS STATUS] RX Pin:%d | Total Chars:%u | Sentences Passed:%u | Fix:%d | Sats:%u | HDOP:%.2f\n",
                    PROBE_CONFIGS[currentProbeIdx].rxPin, totalChars,
                    (uint32_t)gps.passedChecksum(),
                    gps.location.isValid(),
                    gps.satellites.isValid() ? gps.satellites.value() : 0,
                    gps.hdop.isValid() ? gps.hdop.hdop() : 99.99);
    }

    // Send GPS fix status to queue every 500ms or on updated location
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
