#include "baro_task.h"
#include "config/pins.h"
#include "core/telemetry_state.h"
#include <Wire.h>
#include <Adafruit_BME280.h>

QueueHandle_t g_baro_queue = NULL;

static Adafruit_BME280 bme;

void startBaroTask() {
  if (g_baro_queue == NULL) {
    g_baro_queue = xQueueCreate(10, sizeof(BaroSample));
  }

  xTaskCreatePinnedToCore(
    baroTaskLoop,
    "BaroTask",
    4096,
    NULL,
    2, // Priority 2
    NULL,
    0  // Core 0
  );
}

void baroTaskLoop(void* pvParameters) {
  bool bmeInitialized = false;

  // Try initializing BME280 with I2C bus mutex protection (probe 0x76 and 0x77)
  for (int attempt = 0; attempt < 5; attempt++) {
    if (g_i2c_mutex != NULL && xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
      if (bme.begin(0x76, &Wire)) {
        bmeInitialized = true;
        Serial.println("[BARO] BME280 sensor initialized at I2C address 0x76");
      } else if (bme.begin(0x77, &Wire)) {
        bmeInitialized = true;
        Serial.println("[BARO] BME280 sensor initialized at I2C address 0x77");
      }
      xSemaphoreGive(g_i2c_mutex);
    }
    if (bmeInitialized) break;
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  if (bmeInitialized) {
    if (g_i2c_mutex != NULL && xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      bme.setSampling(Adafruit_BME280::MODE_NORMAL,
                      Adafruit_BME280::SAMPLING_X2,     // temp
                      Adafruit_BME280::SAMPLING_X16,    // pressure
                      Adafruit_BME280::SAMPLING_X1,     // humidity
                      Adafruit_BME280::FILTER_X16);
      xSemaphoreGive(g_i2c_mutex);
    }
  } else {
    Serial.println("[BARO WARNING] BME280 sensor not detected on I2C bus (0x76 / 0x77)");
  }

  for (;;) {
    BaroSample sample;
    sample.isValid = false;
    sample.pressureHpa = 0.0f;
    sample.altitudeM = 0.0f;
    sample.temperatureC = 0.0f;
    sample.humidityPct = 0.0f;

    if (bmeInitialized) {
      if (g_i2c_mutex != NULL && xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        float press = bme.readPressure() / 100.0f; // Pa to hPa
        float temp = bme.readTemperature();
        float hum = bme.readHumidity();
        float alt = bme.readAltitude(1013.25f);    // Standard sea level pressure

        xSemaphoreGive(g_i2c_mutex);

        if (press > 300.0f && press < 1200.0f) {
          sample.isValid = true;
          sample.pressureHpa = press;
          sample.temperatureC = temp;
          sample.humidityPct = hum;
          sample.altitudeM = alt;
        }
      }
    }

    if (g_baro_queue != NULL) {
      xQueueSend(g_baro_queue, &sample, 0);
    }

    vTaskDelay(pdMS_TO_TICKS(250)); // ~4Hz sampling rate
  }
}
