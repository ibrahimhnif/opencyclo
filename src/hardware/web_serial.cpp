#include "web_serial.h"

#include <Arduino.h>
#include <string.h>

#include "storage/layout_config.h"
#include "storage/theme_config.h"
#include "ui/engine/ui_config.h"
#include "ui/engine/theme.h"

#define WEB_SERIAL_PROTO_VERSION 1u
#define WEB_SERIAL_LINE_MAX 4096
#define WEB_SERIAL_DOC_MAX 3072

namespace {

void replyOkExtra(const char* extraKey, const char* extraJson) {
  if (extraKey != nullptr) {
    Serial.printf("OC={\"ok\":true,\"%s\":%s}\n", extraKey, extraJson);
  } else {
    Serial.println("OC={\"ok\":true}");
  }
}

void replyErr(const char* err) {
  Serial.printf("OC={\"ok\":false,\"err\":\"%s\"}\n", err);
}

void handleLine(const char* line) {
  if (strncmp(line, "OC+", 3) != 0) return;  // log-monitor traffic, not ours
  const char* cmd = line + 3;
  const char* colon = strchr(cmd, ':');
  size_t cmdLen = (colon != nullptr) ? (size_t)(colon - cmd) : strlen(cmd);
  const char* payload = (colon != nullptr) ? colon + 1 : "";

  char name[16];
  if (cmdLen >= sizeof(name)) {
    replyErr("unknown command");
    return;
  }
  strncpy(name, cmd, cmdLen);
  name[cmdLen] = '\0';

  if (!strcmp(name, "HELLO")) {
    Serial.printf(
        "OC={\"ok\":true,\"proto\":%u,\"fw\":\"0.1.0\",\"layout_schema\":%u,\"theme_schema\":%u}\n",
        (unsigned)WEB_SERIAL_PROTO_VERSION, (unsigned)UI_CONFIG_SCHEMA_VERSION,
        (unsigned)UI_THEME_SCHEMA_VERSION);
  } else if (!strcmp(name, "GET_LAYOUT")) {
    static char doc[WEB_SERIAL_DOC_MAX];
    if (exportLayoutToString(doc, sizeof(doc)) == 0) {
      replyErr("layout too large");
    } else {
      replyOkExtra("layout", doc);
    }
  } else if (!strcmp(name, "SET_LAYOUT")) {
    if (importLayoutFromString(payload)) {
      replyOkExtra(nullptr, nullptr);
    } else {
      replyErr("invalid layout");
    }
  } else if (!strcmp(name, "GET_THEME")) {
    static char doc[WEB_SERIAL_DOC_MAX];
    if (exportThemeToString(doc, sizeof(doc)) == 0) {
      replyErr("theme too large");
    } else {
      replyOkExtra("theme", doc);
    }
  } else if (!strcmp(name, "SET_THEME")) {
    if (importThemeFromString(payload)) {
      replyOkExtra(nullptr, nullptr);
    } else {
      replyErr("invalid theme");
    }
  } else if (!strcmp(name, "RESET_ALL")) {
    resetLayoutToDefaults();
    saveLayoutConfig();
    resetThemeToDefault();
    saveThemeConfig();
    replyOkExtra(nullptr, nullptr);
  } else {
    replyErr("unknown command");
  }
}

void webSerialTaskLoop(void* pvParameters) {
  (void)pvParameters;
  static char line[WEB_SERIAL_LINE_MAX];
  size_t len = 0;
  for (;;) {
    while (Serial.available() > 0) {
      char c = (char)Serial.read();
      if (c == '\r') continue;
      if (c == '\n') {
        line[len] = '\0';
        if (len > 0) handleLine(line);
        len = 0;
        continue;
      }
      // Overlong lines can never be valid commands; drop the whole line
      // rather than dispatching a truncated payload as if it were complete.
      if (len + 1 < sizeof(line)) {
        line[len++] = c;
      } else {
        len = 0;
        // Drain to end of line so the next dispatch starts clean.
        while (Serial.available() > 0 && Serial.read() != '\n') {
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

}  // namespace

void startWebSerialTask() {
  xTaskCreatePinnedToCore(webSerialTaskLoop, "WebSerial", 8192, NULL, 1, NULL, 1);
}
