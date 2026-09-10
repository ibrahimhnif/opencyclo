#include "battery.h"
#include "config/pins.h"

void initBatteryADC() {
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_BATTERY_ADC, ADC_11db);
  pinMode(PIN_BATTERY_ADC, INPUT);
}

float readBatteryVoltage() {
  // Read calibrated millivolts on GPIO 9 (PIN_BATTERY_ADC)
  uint32_t rawMv = 0;
  for (int i = 0; i < 16; ++i) rawMv += analogReadMilliVolts(PIN_BATTERY_ADC);
  rawMv /= 16;
  // Built-in equal-resistor voltage divider (200k/200k in the board schematic).
  float vBat = (rawMv * 2.0f) / 1000.0f;
  return vBat;
}

uint8_t readBatteryPercentage() {
  float v = readBatteryVoltage();
  if (v >= 4.15f) return 100;
  if (v <= 3.30f) return 0;
  
  // Piecewise Li-Po discharge curve
  if (v >= 4.00f) return 85 + (uint8_t)((v - 4.00f) / (4.15f - 4.00f) * 15.0f);
  if (v >= 3.85f) return 60 + (uint8_t)((v - 3.85f) / (4.00f - 3.85f) * 25.0f);
  if (v >= 3.70f) return 35 + (uint8_t)((v - 3.70f) / (3.85f - 3.70f) * 25.0f);
  if (v >= 3.50f) return 10 + (uint8_t)((v - 3.50f) / (3.70f - 3.50f) * 25.0f);
  return (uint8_t)((v - 3.30f) / (3.50f - 3.30f) * 10.0f);
}
