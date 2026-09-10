#include <Arduino.h>
#include "core/telemetry_state.h"
#include "core/fusion_task.h"
#include "hardware/gps_task.h"
#include "hardware/baro_task.h"
#include "hardware/ble_task.h"
#include "storage/logger_task.h"
#include "storage/settings.h"
#include "storage/layout_config.h"
#include "ui/ui_task.h"
#include "hardware/power.h"
#include "hardware/battery.h"
#include "ui/charging_screen.h"
#include "storage/sd_access.h"

void setup() {
  initPower();
  initBatteryADC();
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=================================");
  Serial.println("  OpenCyclo GPS Computer v0.1.0  ");
  Serial.println("  Modular UI Component Engine    ");
  Serial.println("=================================");

  // 1. Initialize Shared Mutexes and Telemetry State
  initTelemetryState();
  initSdAccess();

  // 2. Initialize NVS Settings & Layout Configuration Tree
  initSettings();
  initLayoutConfig();

  // USB can wake a powered-off device into a charging-only UI. Cycling tasks
  // start only after ON (or a BOOT-button wake), never merely on USB insertion.
  showChargingScreenIfNeeded();
  markPowerOn();

  // 3. Launch FreeRTOS Tasks
  startGpsTask();
  startBaroTask();
  startBleTask();
  startLoggerTask();
  startFusionTask();
  startUiTask();

  Serial.println("[SYSTEM] All tasks launched successfully.");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
