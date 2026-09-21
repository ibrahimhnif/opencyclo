#include "ble_ota_handler.h"
#include "hardware/power.h"
#include <Update.h>
#include <esp_ota_ops.h>

static NimBLECharacteristic* pOtaControlChar = nullptr;
static NimBLECharacteristic* pOtaDataChar = nullptr;

static size_t otaTotalBytes = 0;
static size_t otaWrittenBytes = 0;
static bool otaInProgress = false;

// Every control write must produce exactly one reply, otherwise the app (which
// waits for it) cannot distinguish a rejected update from a successful one.
static void notifyOtaResult(uint8_t status, uint8_t code) {
  if (pOtaControlChar == nullptr) return;
  uint8_t resp[2] = {status, code};
  pOtaControlChar->setValue(resp, 2);
  pOtaControlChar->notify();
}

// Fatal mid-stream failure: release the update slot and tell the client why,
// so its final END cannot be mistaken for a successful flash.
static void abortOtaWithError(uint8_t code) {
  Update.abort();
  otaInProgress = false;
  endFirmwareUpdate();
  notifyOtaResult(0xFF, code);
}

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
        notifyOtaResult(0xFF, OTA_ERR_BUSY); // OTA or shutdown already owns power
        return;
      }
      uint32_t size = 0;
      memcpy(&size, val.data() + 1, 4);
      otaTotalBytes = size;
      otaWrittenBytes = 0;

      Serial.printf("[BLE OTA] Starting OTA update, size: %u bytes...\n", (unsigned int)size);
      if (!Update.begin(otaTotalBytes, U_FLASH)) {
        Serial.printf("[BLE OTA] Update.begin failed: %s\n", Update.errorString());
        notifyOtaResult(0xFF, (uint8_t)Update.getError());
        otaInProgress = false;
        endFirmwareUpdate();
        return;
      }

      otaInProgress = true;
      notifyOtaResult(0x01, 0x00); // ACK Ready for data
    }
    // CMD 0x02: END OTA & REBOOT
    else if (cmd == 0x02) {
      // Without this reply an END after a mid-stream abort produced no answer
      // at all, and the client sat on its timeout.
      if (!otaInProgress) {
        Serial.println("[BLE OTA] END with no update in progress.");
        notifyOtaResult(0xFF, OTA_ERR_NO_UPDATE);
        return;
      }
      Serial.printf("[BLE OTA] Finalizing OTA, written %u/%u bytes...\n",
                    (unsigned int)otaWrittenBytes, (unsigned int)otaTotalBytes);

      // A stream that lost bytes must not be committed as if it were complete.
      if (otaWrittenBytes != otaTotalBytes) {
        Serial.println("[BLE OTA] Byte count mismatch; refusing to commit.");
        abortOtaWithError(OTA_ERR_OVERRUN);
        return;
      }

      if (Update.end(true)) {
        Serial.println("[BLE OTA] OTA Update successful! Rebooting in 1 second...");
        notifyOtaResult(0x02, 0x00); // Flash success
        delay(1000);
        ESP.restart();
      } else {
        Serial.printf("[BLE OTA] Update.end failed: %s\n", Update.errorString());
        notifyOtaResult(0xFF, (uint8_t)Update.getError());
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
      notifyOtaResult(0x00, 0x00);
    }
  }
};

class OtaDataCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar) override {
    if (!otaInProgress) return;

    std::string val = pChar->getValue();
    size_t len = val.length();
    if (len == 0) return;

    // The BEGIN command declared otaTotalBytes. Anything past that is a client
    // bug (wrong chunk size, restarted stream) and must abort rather than
    // silently writing a corrupt image, since the client cannot see the
    // short-write counter otherwise.
    if (otaWrittenBytes + len > otaTotalBytes) {
      Serial.printf("[BLE OTA] Overrun: %u + %u > %u, aborting.\n",
                    (unsigned int)otaWrittenBytes, (unsigned int)len, (unsigned int)otaTotalBytes);
      abortOtaWithError(OTA_ERR_OVERRUN);
      return;
    }

    size_t written = Update.write((uint8_t*)val.data(), len);
    otaWrittenBytes += written;

    if (written != len) {
      Serial.printf("[BLE OTA] Write error: expected %u, wrote %u\n", (unsigned int)len, (unsigned int)written);
      abortOtaWithError(OTA_ERR_SHORT_WRITE);
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
