#include <Arduino.h>
#include "config/pins.h"
#include "core/telemetry_state.h"
#include "hardware/gps_task.h"
#include "hardware/baro_task.h"
#include "core/fusion_task.h"
#include "ui/ui_task.h"

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("==========================================");
  Serial.println("         OPENCYCLO FIRMWARE v0.1.0        ");
  Serial.println("       ESP32-S3 DIY GPS Cycling Computer  ");
  Serial.println("==========================================");

  // Initialize shared telemetry state & mutexes
  initTelemetryState();

  // Start FreeRTOS Subsystem Tasks
  Serial.println("[SYSTEM] Starting GPS Task on Core 0...");
  startGpsTask();

  Serial.println("[SYSTEM] Starting Barometer BMP280 Task on Core 0...");
  startBaroTask();

  Serial.println("[SYSTEM] Starting Telemetry Fusion Task on Core 0...");
  startFusionTask();

  Serial.println("[SYSTEM] Starting UI & Touch Task on Core 1...");
  startUiTask();

  Serial.println("[SYSTEM] Setup complete. Tasks running.");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
