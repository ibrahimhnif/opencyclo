#ifndef OPENCYCLO_HARDWARE_BLE_OTA_HANDLER_H
#define OPENCYCLO_HARDWARE_BLE_OTA_HANDLER_H

#include <NimBLEDevice.h>

#define BLE_OTA_SERVICE_UUID "00001910-0000-1000-8000-00805F9B34FB"
#define BLE_OTA_CONTROL_UUID "00001911-0000-1000-8000-00805F9B34FB"
#define BLE_OTA_DATA_UUID    "00001912-0000-1000-8000-00805F9B34FB"

// Every OTA control write is answered with {status, code} on 0x1911 notify:
//   0x01 0x00  ready for data (BEGIN accepted)
//   0x02 0x00  image committed, device is about to reboot
//   0x00 0x00  client abort acknowledged
//   0xFF code  failure -- `code` is either Update.getError() (small values) or
//              one of the OTA_ERR_* reasons below.
#define OTA_ERR_BUSY        0xFE // shutdown or another update already owns power
#define OTA_ERR_NO_UPDATE   0xFB // END with no update in progress (aborted earlier)
#define OTA_ERR_OVERRUN     0xFA // more data than the BEGIN size declared
#define OTA_ERR_SHORT_WRITE 0xF9 // Update.write() stored fewer bytes than sent

void initBleOtaService(NimBLEServer* pServer);
void abortBleOtaOnDisconnect();

#endif // OPENCYCLO_HARDWARE_BLE_OTA_HANDLER_H
