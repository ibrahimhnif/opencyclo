#ifndef OPENCYCLO_HARDWARE_BLE_LAYOUT_SYNC_H
#define OPENCYCLO_HARDWARE_BLE_LAYOUT_SYNC_H

#include <NimBLEDevice.h>
#include "core/telemetry_state.h"

#define BLE_OPENCYCLO_SERVICE_UUID     "00001900-0000-1000-8000-00805F9B34FB"
#define BLE_LAYOUT_CONFIG_CHAR_UUID    "00001901-0000-1000-8000-00805F9B34FB"
#define BLE_TELEMETRY_STREAM_CHAR_UUID "00001902-0000-1000-8000-00805F9B34FB"
#define BLE_DEVICE_COMMAND_CHAR_UUID   "00001903-0000-1000-8000-00805F9B34FB"
#define BLE_PHONE_GPS_CHAR_UUID        "0000190A-0000-1000-8000-00805F9B34FB"
#define BLE_GPS_SOURCE_MODE_CHAR_UUID  "0000190B-0000-1000-8000-00805F9B34FB"

void initBleLayoutSyncService(NimBLEServer* pServer);
void notifyBleTelemetry(const TelemetryState& state);
// Persists mode, notifies subscribed clients on 0x190B. Called from the
// 0x190B write handler and from the device's own settings menu.
void setGpsSourceMode(uint8_t mode);

#endif // OPENCYCLO_HARDWARE_BLE_LAYOUT_SYNC_H
