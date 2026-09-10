#include "ble_ota_handler.h"
#include "hardware/power.h"
#include <Update.h>
#include <esp_ota_ops.h>

static NimBLECharacteristic* pOtaControlChar = nullptr;
static NimBLECharacteristic* pOtaDataChar = nullptr;

static size_t otaTotalBytes = 0;
static size_t otaWrittenBytes = 0;
static bool otaInProgress = false;

void abortBleOtaOnDisconnect() {
  // Invoked on the same NimBLE host task as the OTA write callbacks.
  if (!otaInProgress) return;
  Update.abort();
  otaInProgress = false;
  endFirmwareUpdate();
  Serial.println("[BLE OTA] Disconnected; aborted incomplete update.");
}

class OtaControlCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar) override {
    std::string val = pChar->getValue();
    if (val.empty()) return;

    uint8_t cmd = (uint8_t)val[0];

    // CMD 0x01: BEGIN OTA (Payload: 1 byte cmd + 4 bytes totalSize)
    if (cmd == 0x01 && val.length() >= 5) {
      if (!beginFirmwareUpdate()) {
        uint8_t resp[2] = {0xFF, 0xFE}; // busy: OTA or shutdown already owns power
        pChar->setValue(resp, 2);
        pChar->notify();
        return;
      }
      uint32_t size = 0;
      memcpy(&size, val.data() + 1, 4);
      otaTotalBytes = size;
      otaWrittenBytes = 0;

      Serial.printf("[BLE OTA] Starting OTA update, size: %u bytes...\n", (unsigned int)size);
      if (!Update.begin(otaTotalBytes, U_FLASH)) {
        Serial.printf("[BLE OTA] Update.begin failed: %s\n", Update.errorString());
        uint8_t resp[2] = {0xFF, (uint8_t)Update.getError()};
        pChar->setValue(resp, 2);
        pChar->notify();
        otaInProgress = false;
        endFirmwareUpdate();
        return;
      }

      otaInProgress = true;
      uint8_t resp[2] = {0x01, 0x00}; // ACK Ready for data
      pChar->setValue(resp, 2);
      pChar->notify();
    }
    // CMD 0x02: END OTA & REBOOT
    else if (cmd == 0x02 && otaInProgress) {
      Serial.printf("[BLE OTA] Finalizing OTA, written %u/%u bytes...\n",
                    (unsigned int)otaWrittenBytes, (unsigned int)otaTotalBytes);

      if (Update.end(true)) {
        Serial.println("[BLE OTA] OTA Update successful! Rebooting in 1 second...");
        uint8_t resp[2] = {0x02, 0x00}; // Flash success
        pChar->setValue(resp, 2);
        pChar->notify();
        delay(1000);
        ESP.restart();
      } else {
        Serial.printf("[BLE OTA] Update.end failed: %s\n", Update.errorString());
        uint8_t resp[2] = {0xFF, (uint8_t)Update.getError()};
        pChar->setValue(resp, 2);
        pChar->notify();
      }
      otaInProgress = false;
      endFirmwareUpdate();
    }
    // CMD 0x03: ABORT OTA
    else if (cmd == 0x03) {
      Serial.println("[BLE OTA] OTA Aborted by client.");
      Update.abort();
      otaInProgress = false;
      endFirmwareUpdate();
      uint8_t resp[2] = {0x00, 0x00};
      pChar->setValue(resp, 2);
      pChar->notify();
    }
  }
};

class OtaDataCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar) override {
    if (!otaInProgress) return;

    std::string val = pChar->getValue();
    size_t len = val.length();
    if (len == 0) return;

    size_t written = Update.write((uint8_t*)val.data(), len);
    otaWrittenBytes += written;

    if (written != len) {
      Serial.printf("[BLE OTA] Write error: expected %u, wrote %u\n", (unsigned int)len, (unsigned int)written);
    }
  }
};

void initBleOtaService(NimBLEServer* pServer) {
  NimBLEService* pOtaService = pServer->createService(BLE_OTA_SERVICE_UUID);

  pOtaControlChar = pOtaService->createCharacteristic(
    BLE_OTA_CONTROL_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY
  );
  pOtaControlChar->setCallbacks(new OtaControlCallbacks());

  pOtaDataChar = pOtaService->createCharacteristic(
    BLE_OTA_DATA_UUID,
    NIMBLE_PROPERTY::WRITE_NR
  );
  pOtaDataChar->setCallbacks(new OtaDataCallbacks());

  pOtaService->start();
  Serial.println("[BLE OTA] OTA GATT Service registered.");
}
