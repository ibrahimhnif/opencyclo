#include "gps_task.h"
#include "config/pins.h"
#include "gps_decoder.h"
#include "gps_filter.h"
#include "storage/gps_diagnostics.h"
#include "gnss_power.h"
#include "gps_assistance.h"
#include "gps_identity.h"
#include "storage/gps_cache.h"
#include "gps_source_arbiter.h"
#include "storage/settings.h"
#include "core/utc_time.h"
#include <mutex>
#include <driver/gpio.h>
#include <driver/uart.h>

QueueHandle_t g_gps_queue = NULL;
QueueHandle_t g_phone_gps_queue = NULL;

struct PhoneGpsSample {
  double latitude;
  double longitude;
  float accuracyM;
  uint32_t utcEpochS;
  uint32_t receivedAtMs;
};

void setPhoneGpsSample(double lat, double lon, float accuracyM, uint32_t utcEpochS) {
  if (g_phone_gps_queue == NULL) return;
  PhoneGpsSample sample{lat, lon, accuracyM, utcEpochS, millis()};
  xQueueOverwrite(g_phone_gps_queue, &sample);
}

// Tracks the previous phone sample actually used to derive a phone-sourced
// GpsFix, so computePhoneSpeedKmh() has a delta to work with across loop
// iterations. Lives here (not in the pure arbiter) because it's stateful.
static bool havePreviousPhoneSample = false;
static double previousPhoneLat = 0, previousPhoneLon = 0;
static uint32_t previousPhoneAtMs = 0;
// The phone queue is peeked (non-destructive) and pushes run far faster than
// the ~1 Hz phone sample rate, so the SAME sample is re-read many times.
// Recomputing the speed on a re-read yields speedValid=false (zero delta),
// which makes FusionTask zero the speed and reset its prevLat/prevLon, so trip
// distance never accumulates. Cache the last real result and reuse it instead.
static bool havePhoneSpeed = false;
static float cachedPhoneSpeedKmh = 0.0f;

// Last UTC the M10 reported this boot. The 11-byte phone payload carries no
// timestamp, and GpxWriter drops fixes whose date is zero, so phone-sourced
// fixes inherit this. Not real-time-accurate across a long phone-only session,
// but strictly better than a timeless ride; stays zero if the M10 never had a
// time this boot.
static bool haveHardwareUtc = false;
static uint16_t lastHardwareYear = 0;
static uint8_t lastHardwareMonth = 0, lastHardwareDay = 0;
static uint8_t lastHardwareHour = 0, lastHardwareMinute = 0, lastHardwareSecond = 0;

static GpsDecoder gps;
static GpsFilter gpsFilter;
static HardwareSerial gpsSerial(1);
static std::mutex gpsMutex;
static std::mutex assistanceMutex;
static gnss::Assistance assistance;
static gnss::Identity identity;
static gnss::Parser assistanceParser;
void gpsAssistanceCommand(const uint8_t* bytes,size_t length) {
  std::lock_guard<std::mutex> lock(assistanceMutex);
  if(identity.busy() || gpsCacheInjecting())return;
  assistance.command(bytes,length,millis());
}
void gpsIdentityCommand(const uint8_t* bytes,size_t length) {
  std::lock_guard<std::mutex> lock(assistanceMutex);
  if(assistance.busy() || gpsCacheInjecting())return;
  identity.command(bytes,length);
}
size_t gpsIdentityRead(uint8_t out[20]) {
  std::lock_guard<std::mutex> lock(assistanceMutex);
  return identity.read(out);
}
void gpsAssistanceStatus(uint8_t out[12]) {
  std::lock_guard<std::mutex> lock(assistanceMutex);
  assistance.status(out);
}
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
  {
    std::lock_guard<std::mutex> aidLock(assistanceMutex);
    if(assistance.busy())assistance.fail(gnss::Assistance::Cancelled);
    if(identity.busy())identity.cancel();
    gpsCacheCancel();
  }
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
  if (g_phone_gps_queue == NULL) {
    g_phone_gps_queue = xQueueCreate(1, sizeof(PhoneGpsSample));
  }

  xTaskCreatePinnedToCore(
    gpsTaskLoop,
    "GpsTask",
    GPS_DIAGNOSTICS_ENABLED ? 8192 : 4096,
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
      if(assistanceParser.feed(uint8_t(c))) {
        std::lock_guard<std::mutex> aidLock(assistanceMutex);
        assistance.receive(assistanceParser.frame);
        identity.receive(assistanceParser.frame);
        gpsCacheReceive(assistanceParser.frame);
      }
      const uint32_t sequence=gps.diagnosticSequence;
      changed=gps.feed(c,millis()) || changed;
      if(sequence!=gps.diagnosticSequence) {
        const uint32_t at=millis();const GpsFix raw=gps.snapshot(at);
        captureGpsDiagnostic(raw,raw,"nmea-observation",at,gps.diagnosticLine);
      }

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
    {
      std::lock_guard<std::mutex> aidLock(assistanceMutex);
      assistance.tick(gpsPort,gpsPower.supported());
      identity.tick(gpsPort);
      gpsCacheGpsTick(gpsPort,gpsPower.supported(),assistance.busy() || identity.busy());
      static uint8_t previousState=gnss::Assistance::Idle;
      if(previousState!=assistance.state) {
        previousState=assistance.state;
        if(assistance.state==gnss::Assistance::ConfigAck ||
           assistance.state==gnss::Assistance::Done || assistance.state==gnss::Assistance::Error)
          Serial.printf("[GPS AID] ms=%u state=%u acknowledged=%u error=%u receiver_code=%u\n",
            unsigned(now),unsigned(assistance.state),unsigned(assistance.index),
            unsigned(assistance.error),unsigned(assistance.receiverInfo));
      }
    }

    if (now - lastDebugLogMs >= 5000) {
      lastDebugLogMs = now;
      g_gps_debug.total_chars = totalChars;
      g_gps_debug.sentences_passed = gps.accepted;
      g_gps_debug.active_rx_pin = PIN_GPS_RX;
      const GpsFix raw=gps.snapshot(now),status=gpsFilter.apply(raw,now);
      Serial.printf("[GPS] raw_fix=%d quality=%u raw_speed=%.1f speed=%.1f sats=%u hdop=%.2f accuracy=%d hAcc=%.1fm sAcc=%.2fm/s age=%u chars=%u reason=%s\n",
        raw.isValid,unsigned(status.quality),raw.speedKmh,status.speedKmh,unsigned(raw.satellites),
        raw.hdop,raw.accuracyValid,raw.horizontalAccuracyM,raw.speedAccuracyMps,unsigned(raw.ageMs),unsigned(totalChars),gpsFilter.rejectionReason());
    }

    // Send GPS fix status to queue
    if (now - lastPushMs >= 500 || changed) {
      lastPushMs = now;
      const GpsFix raw=gps.snapshot(now);
      GpsFix fix=gpsFilter.apply(raw,now);
      captureGpsDiagnostic(raw,fix,gpsFilter.rejectionReason(),now);

      PhoneGpsSample phone{};
      const bool havePhone = g_phone_gps_queue != NULL &&
        xQueuePeek(g_phone_gps_queue, &phone, 0) == pdTRUE;
      const uint32_t phoneAgeMs = havePhone ? now - phone.receivedAtMs : UINT32_MAX;
      const GpsFixSource source = selectGpsSource(
        (GpsSourceMode)g_settings.gps_source_mode, fix.isValid, havePhone, phoneAgeMs);

      GpsFix outFix{};
      if (source == GPS_FIX_SOURCE_HARDWARE) {
        outFix = fix;
        outFix.source = GPS_FIX_SOURCE_HARDWARE;
        // GpsDecoder only populates the date/time fields when the receiver
        // reported a valid UTC, so a non-zero year is the freshness marker.
        if (outFix.year != 0) {
          lastHardwareYear = outFix.year;
          lastHardwareMonth = outFix.month;
          lastHardwareDay = outFix.day;
          lastHardwareHour = outFix.hour;
          lastHardwareMinute = outFix.minute;
          lastHardwareSecond = outFix.second;
          haveHardwareUtc = true;
        }
      } else if (source == GPS_FIX_SOURCE_PHONE || source == GPS_FIX_SOURCE_PHONE_FALLBACK) {
        outFix.isValid = true;
        outFix.accuracyValid = true;
        outFix.latitude = phone.latitude;
        outFix.longitude = phone.longitude;
        outFix.horizontalAccuracyM = phone.accuracyM;
        outFix.quality = 2;
        outFix.receivedAtMs = now;
        outFix.source = (uint8_t)source;
        if (!havePreviousPhoneSample || phone.receivedAtMs != previousPhoneAtMs) {
          if (havePreviousPhoneSample) {
            const PhoneSpeedResult speed = computePhoneSpeedKmh(
              previousPhoneLat, previousPhoneLon, previousPhoneAtMs,
              phone.latitude, phone.longitude, phone.receivedAtMs);
            havePhoneSpeed = speed.speedValid;
            cachedPhoneSpeedKmh = speed.speedKmh;
          }
          previousPhoneLat = phone.latitude;
          previousPhoneLon = phone.longitude;
          previousPhoneAtMs = phone.receivedAtMs;
          havePreviousPhoneSample = true;
        }
        // Either freshly computed above, or carried over from the last new
        // sample when this push is a re-read of the same queue entry.
        outFix.speedValid = havePhoneSpeed;
        outFix.speedKmh = cachedPhoneSpeedKmh;
        if (phone.utcEpochS != 0) {
          const utc::Calendar cal = utc::civilFromEpoch(phone.utcEpochS);
          outFix.year = cal.year;
          outFix.month = cal.month;
          outFix.day = cal.day;
          outFix.hour = cal.hour;
          outFix.minute = cal.minute;
          outFix.second = cal.second;
        } else if (haveHardwareUtc) {
          outFix.year = lastHardwareYear;
          outFix.month = lastHardwareMonth;
          outFix.day = lastHardwareDay;
          outFix.hour = lastHardwareHour;
          outFix.minute = lastHardwareMinute;
          outFix.second = lastHardwareSecond;
        }
      } else {
        // Reuse the hardware fix GpsFilter already computed so satellites,
        // HDOP and UTC survive as diagnostics instead of being blanked. The
        // hardware fix can still be *valid* here (phone-forced mode with a
        // stale phone sample), so re-assert that nothing usable is published:
        // FusionTask keys off isValid, not source.
        outFix = fix;
        outFix.isValid = false;
        outFix.speedValid = false;
        outFix.speedKmh = 0;
        if (outFix.quality > 1) outFix.quality = 1;
        outFix.receivedAtMs = now;
        outFix.source = GPS_FIX_SOURCE_NONE;
      }

      if (g_gps_queue != NULL) {
        xQueueOverwrite(g_gps_queue, &outFix);
      }
    }

    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
