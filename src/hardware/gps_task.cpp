#include "gps_task.h"
#include "config/pins.h"
#include "gps_decoder.h"
#include "gps_filter.h"
#include "gnss_power.h"
#include <mutex>
#include <driver/gpio.h>
#include <driver/uart.h>

QueueHandle_t g_gps_queue = NULL;

static GpsDecoder gps;
static GpsFilter gpsFilter;
static HardwareSerial gpsSerial(1);
static std::mutex gpsMutex;
static bool uartReady=false,quiesced=false;
struct GpsPort : gnss::Port {
  uint32_t now() override {return millis();}
  void wait(uint32_t ms) override {delay(ms);}
  int read() override {return gpsSerial.read();}
  bool write(const uint8_t* p,size_t n) override {
    const uint32_t start=millis();
    while(gpsSerial.availableForWrite()<int(n)) {
      if(millis()-start>=200)return false;
      delay(1);
    }
    return gpsSerial.write(p,n)==n && uart_wait_tx_done(UART_NUM_1,pdMS_TO_TICKS(200))==ESP_OK;
  }
};
static GpsPort gpsPort;
static gnss::Power gpsPower(gpsPort);

static char nmeaLineBuf[80];
static uint8_t nmeaLineIdx = 0;

static void wakeGps() {
  gpio_hold_dis(static_cast<gpio_num_t>(PIN_GPS_TX));
  const bool responded=gpsPower.wake();
  Serial.printf("[GPS POWER] UART wake: %s; protocol 34.10: %s\n",
    responded?"receiver replied":"no version reply",gpsPower.supported()?"recognized":"not confirmed");
  if(responded)Serial.printf("[GPS POWER] SW=%s HW=%s %s\n",gpsPower.software,gpsPower.hardware,gpsPower.protocol);
  bool configured=false;
  if(gpsPower.supported()) {
    configured=gpsPower.configure();
    Serial.printf("[GPS POWER] RAM 5 Hz/portable + NAV-PVT: %s\n",configured?"ACK":"not acknowledged; NMEA quality fallback");
  }
  // Never publish pre-standby fixes as fresh positions after recovery.
  gps.reset(configured);gpsFilter.reset();nmeaLineIdx=0;
  if(g_gps_queue) {GpsFix invalid{};invalid.receivedAtMs=millis();xQueueOverwrite(g_gps_queue,&invalid);}
}

bool prepareGpsForPowerOff() {
  std::unique_lock<std::mutex> lock(gpsMutex,std::defer_lock);
  const uint32_t start=millis();
  while(!lock.try_lock()) {if(millis()-start>=500)return false;delay(5);}
  if(!uartReady)return false;
  if(quiesced)return true;
  const auto result=gpsPower.standby();
  if(result!=gnss::Standby::Quiet) {
    Serial.printf("[GPS POWER] Standby not verified (reason=%u); canceling shutdown\n",unsigned(result));
    wakeGps();return false;
  }
  // UART TX is idle HIGH after bounded TX completion. Keep it there across
  // ESP32 sleep: a floating/transitioning GPS RX would wake the GNSS again.
  if(gpio_hold_en(static_cast<gpio_num_t>(PIN_GPS_TX))!=ESP_OK) {wakeGps();return false;}
  quiesced=true;
  Serial.println("[GPS POWER] PMREQ sent; UART quiet; TX held. Measure current to verify standby.");
  return true;
}

void startGpsTask() {
  if (g_gps_queue == NULL) {
    g_gps_queue = xQueueCreate(1, sizeof(GpsFix));
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
  {
  std::lock_guard<std::mutex> lock(gpsMutex);
  gpio_hold_dis(static_cast<gpio_num_t>(PIN_GPS_TX));
  gpsSerial.setRxBufferSize(2048);
  gpsSerial.begin(GPS_BAUD_RATE, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  Serial.printf("[GPS TASK] Started on RX=%d, TX=%d @ %u baud\n", PIN_GPS_RX, PIN_GPS_TX, GPS_BAUD_RATE);

  uartReady=true;
  wakeGps();
  }

  uint32_t lastPushMs = 0;
  uint32_t lastDebugLogMs = 0;
  uint32_t totalChars = 0;

  for (;;) {
    {
    std::unique_lock<std::mutex> lock(gpsMutex,std::try_to_lock);
    if(!lock.owns_lock() || quiesced) {lock=std::unique_lock<std::mutex>();vTaskDelay(pdMS_TO_TICKS(50));continue;}
    bool changed=false;
    while (gpsSerial.available() > 0) {
      char c = (char)gpsSerial.read();
      totalChars++;
      changed=gps.feed(c,millis()) || changed;

      // Accumulate NMEA line into buffer for Live UI Debug Console
      if (c == '\n' || c == '\r') {
        if (nmeaLineIdx > 0) {
          nmeaLineBuf[nmeaLineIdx] = '\0';
          addNmeaDebugLine(nmeaLineBuf);
          nmeaLineIdx = 0;
        }
      } else if (c=='$') {
        nmeaLineIdx=0;nmeaLineBuf[nmeaLineIdx++]=c;
      } else if (nmeaLineIdx && c>=32 && c<=126 && nmeaLineIdx < sizeof(nmeaLineBuf) - 1) {
        nmeaLineBuf[nmeaLineIdx++] = c;
      }
    }

    uint32_t now = millis();

    if (now - lastDebugLogMs >= 5000) {
      lastDebugLogMs = now;
      g_gps_debug.total_chars = totalChars;
      g_gps_debug.sentences_passed = gps.accepted;
      g_gps_debug.active_rx_pin = PIN_GPS_RX;
      const GpsFix raw=gps.snapshot(now),status=gpsFilter.apply(raw,now);
      Serial.printf("[GPS] raw_fix=%d quality=%u raw_speed=%.1f speed=%.1f sats=%u hdop=%.2f accuracy=%d hAcc=%.1fm sAcc=%.2fm/s age=%u chars=%u\n",
        raw.isValid,unsigned(status.quality),raw.speedKmh,status.speedKmh,unsigned(raw.satellites),
        raw.hdop,raw.accuracyValid,raw.horizontalAccuracyM,raw.speedAccuracyMps,unsigned(raw.ageMs),unsigned(totalChars));
    }

    // Send GPS fix status to queue
    if (now - lastPushMs >= 500 || changed) {
      lastPushMs = now;
      GpsFix fix=gpsFilter.apply(gps.snapshot(now),now);

      if (g_gps_queue != NULL) {
        xQueueOverwrite(g_gps_queue, &fix);
      }
    }

    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
