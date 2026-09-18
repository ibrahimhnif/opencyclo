#include "baro_task.h"
#include "core/telemetry_state.h"
#include "display.h"
#include "bosch/bme280.h"
#include <math.h>
#include <string.h>
#include "baro_calibration.h"
#include <Preferences.h>
#include <atomic>

static std::atomic<float> baroReference{1013.25f};
static std::atomic<int> requestedElevation{10000};
static std::atomic<bool> autoEnabled{false},autoDone{false};
static std::atomic<int> requestedAuto{-1};
bool getBaroAutoEnabled(){return autoEnabled.load();}
bool baroAutoDone(){return autoDone.load();}
void setBaroAutoEnabled(bool enabled){requestedAuto.store(enabled?1:0);}
static std::atomic<BaroCalibrationStatus> calibrationStatus{BaroCalibrationStatus::Ready};
float getBaroReference() { return baroReference.load(); }
BaroCalibrationStatus getBaroCalibrationStatus() { return calibrationStatus.load(); }
bool requestBaroCalibration(int elevation) {
  if (elevation < -500 || elevation > 9000) return false;
  int empty=10000;
  if(!requestedElevation.compare_exchange_strong(empty,elevation))return false;
  autoDone.store(true);
  // Manual elevation takes precedence until the user explicitly enables GPS.
  setBaroAutoEnabled(false);
  calibrationStatus.store(BaroCalibrationStatus::Saving);
  return true;
}
bool requestBaroAutoCalibration(int elevation){
  if(!getBaroAutoEnabled() || baroAutoDone() || elevation < -500 || elevation > 9000)return false;
  int empty=10000;
  // Encoded request keeps the source and value atomic.
  return requestedElevation.compare_exchange_strong(empty,elevation+20000);
}

// NVS writes stay on the worker, outside the touch/I2C lock.
static void applyCalibration(const BaroSample& sample) {
  int option=requestedAuto.exchange(-1);
  if(option!=-1){
    Preferences prefs;
    bool saved=false;
    if(prefs.begin("baro",false)){
      saved=prefs.putBool("autoGps",option==1)==sizeof(bool);prefs.end();
    }
    if(saved){autoEnabled.store(option==1);autoDone.store(false);}
    else calibrationStatus.store(BaroCalibrationStatus::Failed);
  }
  int elevation = requestedElevation.exchange(10000);
  if (elevation == 10000) return;
  const bool automatic=elevation>=19500;
  if(automatic){
    elevation-=20000;
    if(!getBaroAutoEnabled() || baroAutoDone())return;
  }
  if (getTelemetrySnapshot().ride_state != RIDE_STATE_IDLE) {
    calibrationStatus.store(BaroCalibrationStatus::RideActive); return;
  }
  if (!sample.isValid) {
    calibrationStatus.store(BaroCalibrationStatus::NoData); return;
  }
  const float reference = baroReferenceForElevation(sample.pressureHpa, elevation);
  Preferences prefs;
  bool saved = false;
  if (baroReferenceValid(reference) && prefs.begin("baro", false)) {
    saved = prefs.putFloat("reference", reference) == sizeof(float);
    prefs.end();
  }
  if (saved) {
    baroReference.store(reference);
    if(automatic)autoDone.store(true);
    Serial.printf("[BARO CAL] source=%s elevation=%d reference=%.2f\n",automatic?"GPS":"manual",elevation,reference);
  }
  calibrationStatus.store(saved ? BaroCalibrationStatus::Saved : BaroCalibrationStatus::Failed);
}

QueueHandle_t g_baro_queue = nullptr;
static bme280_dev sensor{};
static uint8_t sensorAddress = 0x76;

// Bosch callbacks run only while BaroTask owns the shared touch/I2C mutex.
static int8_t readRegisters(uint8_t reg, uint8_t* data, uint32_t size, void* context) {
  return lgfx::i2c::transactionWriteRead(0, *static_cast<uint8_t*>(context),
      &reg, 1, data, size, 400000).has_value() ? 0 : -1;
}
static int8_t writeRegisters(uint8_t reg, const uint8_t* data, uint32_t size, void* context) {
  uint8_t buffer[32];
  if (size > sizeof(buffer) - 1) return -1;
  buffer[0] = reg;
  memcpy(buffer + 1, data, size);
  return lgfx::i2c::transactionWrite(0, *static_cast<uint8_t*>(context),
      buffer, size + 1, 400000).has_value() ? 0 : -1;
}
static void sensorDelay(uint32_t us, void*) {
  if (us >= 1000) vTaskDelay(pdMS_TO_TICKS((us + 999) / 1000) + 1);
  else delayMicroseconds(us);
}

static bool initializeSensor() {
  for (uint8_t address : {uint8_t(0x76), uint8_t(0x77)}) {
    sensorAddress = address;
    sensor = {};
    sensor.intf = BME280_I2C_INTF;
    sensor.intf_ptr = &sensorAddress;
    sensor.read = readRegisters;
    sensor.write = writeRegisters;
    sensor.delay_us = sensorDelay;
    int8_t result = bme280_init(&sensor);
    if (result == BME280_OK) {
      bme280_settings settings{};
      settings.osr_t = BME280_OVERSAMPLING_2X;
      settings.osr_p = BME280_OVERSAMPLING_16X;
      settings.osr_h = BME280_OVERSAMPLING_1X;
      settings.filter = BME280_FILTER_COEFF_16;
      settings.standby_time = BME280_STANDBY_TIME_0_5_MS;
      result = bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &settings, &sensor);
      if (result == BME280_OK) result = bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, &sensor);
    }
    Serial.printf("[BARO] LGFX addr=0x%02x chip=0x%02x init=%d\n", address, sensor.chip_id, result);
    if (result == BME280_OK && sensor.calib_data.dig_p1 != 0) return true;
  }
  return false;
}

void startBaroTask() {
  if (!g_baro_queue) g_baro_queue = xQueueCreate(1, sizeof(BaroSample));
  xTaskCreatePinnedToCore(baroTaskLoop, "BaroTask", 4096, nullptr, 2, nullptr, 0);
}

void baroTaskLoop(void*) {
  Preferences prefs;
  if (prefs.begin("baro", true)) {
    const float reference = prefs.getFloat("reference", 1013.25f);
    if (baroReferenceValid(reference)) baroReference.store(reference);
    autoEnabled.store(prefs.getBool("autoGps", false));
    prefs.end();
  }
  // Touch initializes this hardware bus. Never race that initialization.
  while (!isDisplayReady()) vTaskDelay(pdMS_TO_TICKS(50));
  bool initialized = false;
  uint32_t lastAttempt = millis() - 10000, lastLog = 0;
  unsigned failures = 0, validSamples = 0, readErrors = 0, lockMisses = 0;
  for (;;) {
    BaroSample sample{};
    int result = BME280_E_COMM_FAIL;
    if (g_i2c_mutex && xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      if (!initialized && millis() - lastAttempt >= 10000) {
        lastAttempt = millis();
        initialized = initializeSensor();
        failures = 0;
      } else if (initialized) {
        bme280_data data{};
        result = bme280_get_sensor_data(BME280_ALL, &data, &sensor);
        if (result == BME280_OK && isfinite(data.pressure) && isfinite(data.temperature) &&
            data.pressure > 30000 && data.pressure < 110000 &&
            data.temperature >= -40 && data.temperature <= 85) {
          sample.pressureHpa = data.pressure / 100.0;
          sample.temperatureC = data.temperature;
          sample.humidityPct = data.humidity;
          sample.altitudeM = 44330.0f * (1.0f - powf(sample.pressureHpa / getBaroReference(), 0.1903f));
          sample.isValid = isfinite(sample.altitudeM);
        }
        if (sample.isValid) { failures = 0; ++validSamples; }
        else {
          ++readErrors;
          if (++failures >= 8) { initialized = false; lastAttempt = millis(); }
        }
      }
      xSemaphoreGive(g_i2c_mutex);
    } else ++lockMisses;
    applyCalibration(sample);
    if (sample.isValid) {
      sample.referenceHpa=getBaroReference();
      sample.altitudeM = 44330.0f * (1.0f - powf(sample.pressureHpa / getBaroReference(), 0.1903f));
    }
    // Latest sample only: queued old values must never look fresh.
    if (g_baro_queue) xQueueOverwrite(g_baro_queue, &sample);
    if (millis() - lastLog >= 10000) {
      lastLog = millis();
      Serial.printf("[BARO] ready=%d valid=%d pressure=%.2f temp=%.2f humidity=%.2f good=%u errors=%u lockMiss=%u result=%d\n",
        initialized, sample.isValid, sample.pressureHpa, sample.temperatureC,
        sample.humidityPct, validSamples, readErrors, lockMisses, result);
    }
    vTaskDelay(pdMS_TO_TICKS(initialized ? 250 : 1000));
  }
}
