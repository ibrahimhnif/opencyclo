#pragma once

// Shared exclusion between firmware updates and shutdown/restart.
bool beginFirmwareUpdate();
void endFirmwareUpdate();
bool beginRouteSync();
void endRouteSync();
bool beginPowerOff();
void cancelPowerOff();
bool isPowerOffRequested();

void initPower();
bool hasUsbPowerSense();
bool isUsbPowerConnected();
bool shouldStartChargingMode();
void markPowerOn();
// Charging UI only: no background tasks have been started yet.
bool sleepFromChargingMode();
// Called by the UI task, after the user selects shutdown/restart.
// Returns an error message on failure; successful shutdown does not return.
const char* powerOff(bool restart);
