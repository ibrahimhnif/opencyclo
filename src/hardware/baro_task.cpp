#include "baro_task.h"
#include "config/pins.h"
#include "core/telemetry_state.h"
#include <Wire.h>
#include <Adafruit_BMP280.h>

QueueHandle_t g_baro_queue = NULL;

static Adafruit_BMP280 bmp;

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
  bool bmpInitialized = false;
  uint8_t baroAddress = 0x76;

  // Try initializing BMP280 with I2C bus mutex protection
  for (int attempt = 0; attempt < 5; attempt++) {
    if (g_i2c_mutex != NULL && xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
      if (bmp.begin(0x76)) {
        bmpInitialized = true;
        baroAddress = 0x76;
        Serial.println("[BARO] BMP280 initialized at 0x76");
      } else if (bmp.begin(0x77)) {
        bmpInitialized = true;
        baroAddress = 0x77;
        Serial.println("[BARO] BMP280 initialized at 0x77");
      }
      xSemaphoreGive(g_i2c_mutex);
    }
    if (bmpInitialized) break;
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  if (bmpInitialized) {
    if (g_i2c_mutex != NULL && xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                      Adafruit_BMP280::SAMPLING_X2,     // temp sampling
                      Adafruit_BMP280::SAMPLING_X16,    // pressure sampling
                      Adafruit_BMP280::FILTER_X16,      // IIR filter
                      Adafruit_BMP280::STANDBY_MS_63);  // standby time
      xSemaphoreGive(g_i2c_mutex);
    }
  } else {
    Serial.println("[BARO WARNING] BMP280 sensor not detected on I2C bus (0x76 / 0x77)");
  }

  for (;;) {
    BaroSample sample;
    sample.isValid = false;
    sample.pressureHpa = 0.0f;
    sample.altitudeM = 0.0f;
    sample.temperatureC = 0.0f;

    if (bmpInitialized) {
      if (g_i2c_mutex != NULL && xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        float press = bmp.readPressure() / 100.0f; // Pa to hPa
        float temp = bmp.readTemperature();
        float alt = bmp.readAltitude(1013.25f);    // Standard sea level pressure

        xSemaphoreGive(g_i2c_mutex);

        if (press > 300.0f && press < 1200.0f) {
          sample.isValid = true;
          sample.pressureHpa = press;
          sample.temperatureC = temp;
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
