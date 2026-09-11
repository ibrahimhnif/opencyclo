#include "widget_registry.h"
#include "widget_catalog.h"
#include "ui/power_menu.h"
#include "ui/control_layout.h"
#include "storage/settings.h"
#include "hardware/battery.h"
#include "hardware/ble_task.h"
#include "hardware/ble_camera_remote.h"
#include <stdio.h>

static uint16_t COLOR_BG = TFT_BLACK;

static uint16_t COLOR_HAIRLINE = tft.color565(28, 28, 28);
static uint16_t COLOR_TEXT     = TFT_WHITE;
static uint16_t COLOR_LABEL    = ui::muted;
static uint16_t COLOR_GREEN    = ui::success;
static uint16_t COLOR_AMBER    = ui::warning;
static uint16_t COLOR_RED      = ui::danger;
static uint16_t COLOR_CYAN     = ui::accent;

// 1. SPEED (hero) — the one widget with its own layout, everything else is a "tile."
static void renderWidgetSpeed(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    canvas.fillRect(b.x, b.y, b.w, b.h, COLOR_BG);
  }
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(COLOR_LABEL, COLOR_BG);
  canvas.setTextPadding(b.w - 8);
  char srcBuf[16];
  snprintf(srcBuf, sizeof(srcBuf), "speed . %s", state.speed_source == SPEED_SOURCE_BLE_CSC ? "ble" : "gps");
  canvas.drawString(srcBuf, b.x + 4, b.y + 4);
  canvas.setTextPadding(0);

  canvas.setFont(&fonts::FreeSansBold24pt7b);
  canvas.setTextColor(COLOR_TEXT, COLOR_BG);
  char buf[12];
  float speed = (g_settings.units == 1) ? (state.speed_kmh * 0.621371f) : state.speed_kmh;
  snprintf(buf, sizeof(buf), "%.1f", speed);
  canvas.setTextPadding(b.w - 70);
  canvas.drawString(buf, b.x + 4, b.y + 24);
  canvas.setTextPadding(0);

  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(COLOR_LABEL, COLOR_BG);
  canvas.setCursor(b.x + b.w - 44, b.y + b.h - 18);
  canvas.print((g_settings.units == 1) ? "mph" : "km/h");
}

// Shared layout for every SMALL/MEDIUM "label above, value below, hairline
// above the tile" widget — same visual pattern, different label/value/color.
//
// Vertical budget inside the shortest (SMALL, 54px) tile: the label is
// FreeSans9pt7b (18px tall) at +4, so it owns rows 4..21; the value is
// FreeSans12pt7b (23px tall) at +24, owning rows 24..46. The 2px gap keeps the
// value's background fill off the label's descenders ("avg spd", "power",
// "grade"), which the previous +6 / +22 pair clipped.
static void renderTile(const Rect& b, const char* label, const char* valueStr, uint16_t valueColor, bool force) {
  if (force) {
    canvas.fillRect(b.x, b.y, b.w, b.h, COLOR_BG);
    canvas.drawFastHLine(b.x, b.y, b.w, COLOR_HAIRLINE);
  }
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(COLOR_LABEL, COLOR_BG);
  canvas.setTextPadding(b.w - 8);
  canvas.drawString(label, b.x + 4, b.y + 4);
  canvas.setTextPadding(0);

  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextColor(valueColor, COLOR_BG);
  canvas.setTextPadding(b.w - 8);
  canvas.drawString(valueStr, b.x + 4, b.y + 24);
  canvas.setTextPadding(0);
}

// 2. DISTANCE
// The unit rides in the 9pt label, not the 12pt value: "125.75 km" measures
// 108px in FreeSans12pt7b, which is wider than a MEDIUM tile's 106px erase
// padding and reaches to within 2px of the tile edge. The bare number is 71px.
static void renderWidgetDistance(const Rect& b, const TelemetryState& state, bool force) {
  char buf[16];
  float d = (g_settings.units == 1) ? (state.trip_distance_km * 0.621371f) : state.trip_distance_km;
  snprintf(buf, sizeof(buf), "%.2f", d);
  renderTile(b, (g_settings.units == 1) ? "dist (mi)" : "dist (km)", buf, COLOR_TEXT, force);
}

// 3. RIDE TIME
static void renderWidgetRideTime(const Rect& b, const TelemetryState& state, bool force) {
  char buf[16];
  uint32_t hrs = state.ride_time_s / 3600;
  uint32_t mins = (state.ride_time_s % 3600) / 60;
  uint32_t secs = state.ride_time_s % 60;
  if (hrs > 0) {
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u", hrs, mins, secs);
  } else {
    snprintf(buf, sizeof(buf), "%02u:%02u", mins, secs);
  }
  renderTile(b, "ride time", buf, COLOR_TEXT, force);
}

// 4. CADENCE
// "cad" not "cadence": Cadence is SIZE_SMALL-only (74px tiles), and
// FreeSansBold9pt7b measures "cadence" at 72px against the tile's 66px erase
// budget (b.w-8) — it was already tight at regular weight (68px) and bold
// pushed it over. There's no headroom to widen the budget without also
// shrinking the tile's side margins.
static void renderWidgetCadence(const Rect& b, const TelemetryState& state, bool force) {
  char buf[8];
  if (state.cadence_rpm >= 0) snprintf(buf, sizeof(buf), "%d", state.cadence_rpm);
  else snprintf(buf, sizeof(buf), "0");
  renderTile(b, "cad", buf, COLOR_CYAN, force);
}

// 5. HEART RATE
static void renderWidgetHeartRate(const Rect& b, const TelemetryState& state, bool force) {
  char buf[8];
  if (state.heart_rate_bpm >= 0) snprintf(buf, sizeof(buf), "%d", state.heart_rate_bpm);
  else snprintf(buf, sizeof(buf), "0");
  renderTile(b, "heart", buf, COLOR_RED, force);
}

// 6. POWER
static void renderWidgetPower(const Rect& b, const TelemetryState& state, bool force) {
  char buf[8];
  if (state.power_watts >= 0) snprintf(buf, sizeof(buf), "%d", state.power_watts);
  else snprintf(buf, sizeof(buf), "0");
  renderTile(b, "power", buf, COLOR_GREEN, force);
}

// 7. ALTITUDE
// Same unit-in-the-label treatment as Distance, and more urgently so: altitude
// declares SIZE_SMALL support, and a SMALL tile is only 74px wide. "12345 ft"
// measures 85px in FreeSans12pt7b, i.e. it would spill 15px past the tile edge
// into its neighbour. The bare number is 65px, inside the 66px erase padding.
static void renderWidgetAltitude(const Rect& b, const TelemetryState& state, bool force) {
  char buf[16];
  float alt = (g_settings.units == 1) ? (state.altitude_m * 3.28084f) : state.altitude_m;
  snprintf(buf, sizeof(buf), "%.0f", alt);
  renderTile(b, (g_settings.units == 1) ? "alt (ft)" : "alt (m)", buf, COLOR_TEXT, force);
}

// 8. GRADE
// Known limitation: Grade is SIZE_SMALL-capable, and a value at or past
// +/-10.0% ("+15.0%" = 81px bold) exceeds a canonical 74px SMALL tile's 66px
// value budget. Not reachable on any default page (Climb places Grade in a
// 114px-wide slot, well within budget), only if a future phone-app layout
// puts it in a Hero-6-Grid bottom-row slot AND the grade happens to hit double
// digits at that exact moment. Left as a documented edge case rather than
// truncating displayed precision to force a fit everywhere.
static void renderWidgetGrade(const Rect& b, const TelemetryState& state, bool force) {
  char buf[12];
  snprintf(buf, sizeof(buf), "%+.1f%%", state.grade_pct);
  uint16_t gradeColor = (state.grade_pct > 3.0f) ? COLOR_AMBER : ((state.grade_pct < -2.0f) ? COLOR_CYAN : COLOR_GREEN);
  renderTile(b, "grade", buf, gradeColor, force);
}

// 9. TOTAL ASCENT
// SIZE_SMALL-capable like Altitude, so the unit moves into the label for the
// same reason ("12345 ft" = 85px vs a 74px tile).
static void renderWidgetTotalAscent(const Rect& b, const TelemetryState& state, bool force) {
  char buf[16];
  float asc = (g_settings.units == 1) ? (state.total_ascent_m * 3.28084f) : state.total_ascent_m;
  snprintf(buf, sizeof(buf), "%.0f", asc);
  renderTile(b, (g_settings.units == 1) ? "asc (ft)" : "asc (m)", buf, COLOR_GREEN, force);
}

// 10. ELEVATION CHART
#define ELEV_SAMPLES 30
static float s_elevHistory[ELEV_SAMPLES];
static uint8_t s_elevHead = 0;
static bool s_elevFilled = false;

static void renderWidgetElevationChart(const Rect& b, const TelemetryState& state, bool force) {
  s_elevHistory[s_elevHead] = state.altitude_m;
  s_elevHead = (s_elevHead + 1) % ELEV_SAMPLES;
  if (s_elevHead == 0) s_elevFilled = true;

  if (force) {
    canvas.fillRect(b.x, b.y, b.w, b.h, COLOR_BG);
    canvas.drawFastHLine(b.x, b.y, b.w, COLOR_HAIRLINE);
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextColor(COLOR_LABEL, COLOR_BG);
    canvas.setCursor(b.x + 4, b.y + 6);
    canvas.print("elevation profile");
  }

  float minAlt = 99999.0f, maxAlt = -99999.0f;
  uint8_t count = s_elevFilled ? ELEV_SAMPLES : s_elevHead;
  if (count < 2) count = 2;
  for (uint8_t i = 0; i < count; i++) {
    float val = s_elevHistory[i];
    if (val < minAlt) minAlt = val;
    if (val > maxAlt) maxAlt = val;
  }
  if (maxAlt - minAlt < 5.0f) maxAlt = minAlt + 5.0f;

  int innerX = b.x + 4;
  int innerY = b.y + 26;
  int innerW = b.w - 8;
  int innerH = b.h - 32;
  canvas.fillRect(innerX, innerY, innerW, innerH, COLOR_BG);

  int prevPx = -1, prevPy = -1;
  for (uint8_t i = 0; i < count; i++) {
    uint8_t idx = s_elevFilled ? ((s_elevHead + i) % ELEV_SAMPLES) : i;
    float val = s_elevHistory[idx];
    int px = innerX + (i * innerW) / (ELEV_SAMPLES - 1);
    int py = innerY + innerH - (int)(((val - minAlt) / (maxAlt - minAlt)) * (innerH - 4));
    if (prevPx != -1) {
      canvas.drawLine(prevPx, prevPy, px, py, COLOR_GREEN);
    }
    prevPx = px;
    prevPy = py;
  }
}

// 11. AVG SPEED
// "avg" not "avg spd": same SIZE_SMALL budget problem as Cadence — bold
// "avg spd" measures 68px against a 66px SMALL-tile erase budget.
static void renderWidgetAvgSpeed(const Rect& b, const TelemetryState& state, bool force) {
  char buf[12];
  float avg = (g_settings.units == 1) ? (state.avg_speed_kmh * 0.621371f) : state.avg_speed_kmh;
  snprintf(buf, sizeof(buf), "%.1f", avg);
  renderTile(b, "avg", buf, COLOR_TEXT, force);
}

// 12. MAX SPEED
// "max" not "max spd": bold "max spd" measures 73px against the same 66px
// SMALL-tile budget.
static void renderWidgetMaxSpeed(const Rect& b, const TelemetryState& state, bool force) {
  char buf[12];
  float maxS = (g_settings.units == 1) ? (state.max_speed_kmh * 0.621371f) : state.max_speed_kmh;
  snprintf(buf, sizeof(buf), "%.1f", maxS);
  renderTile(b, "max", buf, COLOR_TEXT, force);
}

// 13. BATTERY
static void renderWidgetBattery(const Rect& b, const TelemetryState& state, bool force) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%u%%", state.battery_pct);
  uint16_t color = (state.battery_pct < 20) ? COLOR_AMBER : COLOR_GREEN;
  renderTile(b, "battery", buf, color, force);
}

// 14. BLE MANAGER
static void renderWidgetBleManager(const Rect& b,const TelemetryState& state,bool force) {
  (void)force;
  canvas.fillRect(b.x,b.y,b.w,b.h,COLOR_BG);canvas.setTextPadding(0);
  const uint16_t scanColor=g_ble_scanning?COLOR_AMBER:COLOR_CYAN;
  canvas.fillRoundRect(b.x+6,b.y+6,b.w-12,44,8,scanColor);
  ui::drawIcon(canvas,ui::Icon::Search,b.x+14,b.y+16,TFT_BLACK);
  canvas.setFont(&fonts::FreeSansBold9pt7b);canvas.setTextColor(TFT_BLACK,scanColor);
  canvas.drawString(g_ble_scanning?"Scanning...":"Scan",b.x+48,b.y+19);
  const char* labels[]={"Speed","Heart","Power"};
  const char* macs[]={g_settings.paired_csc_mac,g_settings.paired_hr_mac,g_settings.paired_power_mac};
  for(int i=0;i<3;i++){
    const int y=b.y+64+i*52;
    canvas.setFont(&fonts::FreeSansBold9pt7b);canvas.setTextColor(TFT_WHITE,COLOR_BG);
    canvas.drawString(labels[i],b.x+10,y);
    if(macs[i][0]){
      canvas.setFont(&fonts::Font0);canvas.setTextColor(COLOR_LABEL,COLOR_BG);
      canvas.drawString(macs[i],b.x+10,y+27);
      canvas.fillRoundRect(b.x+b.w-94,y,88,44,8,ui::panel);
      canvas.setFont(&fonts::FreeSansBold9pt7b);canvas.setTextColor(COLOR_RED,ui::panel);
      canvas.drawString("Forget",b.x+b.w-80,y+13);
    }else{
      canvas.setTextColor(COLOR_LABEL,COLOR_BG);canvas.drawString("Not paired",b.x+10,y+23);
    }
    canvas.drawFastHLine(b.x+6,y+47,b.w-12,COLOR_HAIRLINE);
  }
  canvas.setFont(&fonts::FreeSansBold9pt7b);canvas.setTextColor(COLOR_LABEL,COLOR_BG);
  canvas.drawString(state.gps_has_fix?"GPS ready":state.gps_fix_quality==1?"GPS weak":"GPS waiting",b.x+10,b.y+230);
}
static bool touchWidgetBleManager(const Rect& b,int16_t x,int16_t y) {
  if(x>=b.x+6&&x<b.x+b.w-6&&y>=b.y+6&&y<b.y+50){triggerBleScan();return true;}
  static const BleProfileType profiles[]={BLE_PROFILE_CSC,BLE_PROFILE_HR,BLE_PROFILE_POWER};
  for(int i=0;i<3;i++){
    const int row=b.y+64+i*52;
    if(x>=b.x+b.w-94&&x<b.x+b.w-6&&y>=row&&y<row+44){forgetSensorProfile(profiles[i]);return true;}
  }
  return false;
}

// 15. SETTINGS LIST
//
// Row geometry is shared between the renderer and the touch handler below so
// the drawn rows and the tappable bands cannot drift apart: row `i`'s text cell
// starts at b.y + SETTINGS_ROW_Y0 + i * SETTINGS_ROW_STRIDE and is 18px tall
// (FreeSans9pt7b), with a divider hairline SETTINGS_ROW_DIVIDER px below that
// top edge. Rows in order: 0 units, 1 brightness, 2 wheel size, 3 sd logging,
// 4 battery, 5 firmware.
static const int SETTINGS_ROW_Y0      = 8;
static const int SETTINGS_ROW_STRIDE  = 32;
static const int SETTINGS_ROW_DIVIDER = 20;

static void renderWidgetSettingsList(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    canvas.fillRect(b.x, b.y, b.w, b.h, COLOR_BG);
  }
  int y = b.y + SETTINGS_ROW_Y0;
  canvas.setFont(&fonts::FreeSansBold9pt7b);

  // Value column budget widened from b.w-126 (106px) to b.w-120 (112px): bold
  // "100% (4.12V)" measures 109px — it was already over the old budget at
  // regular weight (110px), bold just made it worse. The firmware string is
  // shortened below rather than the column widened further, since "opencyclo
  // v0.2.0" at 142px bold would run 26px past the tile's right edge even in
  // a 232px FULL-size widget — that's an actual off-tile overflow, not just a
  // ghosting-erase shortfall.
  auto row = [&](const char* label, const char* value, uint16_t valueColor) {
    canvas.setTextColor(COLOR_LABEL, COLOR_BG);
    canvas.setTextPadding(110);
    canvas.drawString(label, b.x + 10, y);
    canvas.setTextPadding(0);
    canvas.setTextColor(valueColor, COLOR_BG);
    canvas.setTextPadding(b.w - 120);
    canvas.drawString(value, b.x + 116, y);
    canvas.setTextPadding(0);
    canvas.drawFastHLine(b.x + 6, y + SETTINGS_ROW_DIVIDER, b.w - 12, COLOR_HAIRLINE);
    y += SETTINGS_ROW_STRIDE;
  };

  row("units", g_settings.units == 0 ? "metric" : "imperial", COLOR_TEXT);

  char brightStr[8];
  snprintf(brightStr, sizeof(brightStr), "%u%%", (g_settings.brightness * 100) / 255);
  row("brightness", brightStr, COLOR_TEXT);

  char wheelStr[24];
  snprintf(wheelStr, sizeof(wheelStr), "%u mm", g_settings.wheel_circumference_mm);
  row("wheel size", wheelStr, COLOR_TEXT);

  row("sd logging", g_settings.sd_logging_enabled ? "enabled" : "disabled",
      g_settings.sd_logging_enabled ? COLOR_GREEN : COLOR_AMBER);

  char batStr[24];
  snprintf(batStr, sizeof(batStr), "%u%% (%.2fV)", state.battery_pct, readBatteryVoltage());
  row("battery", batStr, COLOR_GREEN);

  row("firmware", "v0.2.0", COLOR_CYAN);

  // Filled controls are the only raised surfaces in the Minimal UI. Keep the
  // power entry in the same cyan/black treatment as the other primary action
  // buttons, and render it to the shared sprite like the rest of the page.
  if (b.h >= 252) {
    canvas.fillRoundRect(b.x + 10, b.y + 208, b.w - 20, 44, 8, COLOR_CYAN);
    canvas.setTextColor(TFT_BLACK, COLOR_CYAN);
    ui::drawIcon(canvas,ui::Icon::Power,b.x+20,b.y+218,TFT_BLACK);
    canvas.setTextPadding(b.w - 64);
    canvas.drawString("Power", b.x + 54, b.y + 221);
    canvas.setTextPadding(0);
  }
}

// A row's visible cell is everything between the divider above it and its own
// divider, which is what a user aims at when tapping. Row `index`'s text top is
// at rowY, the divider above sits at rowY - 16 and its own at rowY + 20, so the
// band is [rowY - 14, rowY + 20] — the whole cell bar a 2px sliver at the top
// edge, which keeps neighbouring bands from claiming the same pixel row. The
// previous [rowY - 8, rowY + 12] band left ~16px of each 36px row untappable.
static bool settingsRowHit(const Rect& b, int16_t y, uint8_t index) {
  const int rowY = b.y + SETTINGS_ROW_Y0 + index * SETTINGS_ROW_STRIDE;
  return (y >= rowY - 10) && (y < rowY + SETTINGS_ROW_DIVIDER);
}

static bool touchWidgetSettingsList(const Rect& b, int16_t x, int16_t y) {
  if (b.h >= 252 && x >= b.x + 10 && x < b.x + b.w - 10 &&
      y >= b.y + 208 && y < b.y + 252) {
    openPowerMenu();
    return true;
  }
  if (settingsRowHit(b, y, 0)) { // units
    g_settings.units = (g_settings.units == 0) ? 1 : 0;
    saveSettings();
    return true;
  }
  if (settingsRowHit(b, y, 1)) { // brightness
    g_settings.brightness = (g_settings.brightness >= 250) ? 50 : (g_settings.brightness + 50);
    setDisplayBrightness(g_settings.brightness);
    saveSettings();
    return true;
  }
  // Row 2 (wheel size) is display-only — not touch-editable.
  if (settingsRowHit(b, y, 3)) { // sd logging
    g_settings.sd_logging_enabled = !g_settings.sd_logging_enabled;
    saveSettings();
    return true;
  }
  return false;
}

// 16. CAMERA REMOTE
//
// Impersonates the Insta360 "GPS Remote" accessory over BLE to trigger an
// Insta360 Ace Pro 2 -- confirmed working on real hardware (see
// ble_camera_remote.cpp for the protocol source). Five verified commands:
// Shutter (photo in photo mode, record start/stop toggle in video mode),
// Mode (cycles Photo/Video/Time Shift -- same as the physical remote, does
// NOT reach every camera mode; e.g. FreeFrame is touchscreen-only even on
// Insta360's own remote), Screen Toggle (short press), Power Off (3s hold),
// and Wake Camera (beacons the camera-specific bytes set via a dedicated
// BLE characteristic -- see ble_camera_remote.h -- so a sleeping/off camera
// notices and reconnects on its own; the button is greyed distinctly if no
// wake bytes have been set yet).
//
// Row geometry is shared between render and touch, same discipline as the
// BLE Manager / Settings List widgets above, so drawn buttons and tappable
// bands can't drift apart.
// Pair, shutter and a 2 x 2 options grid fit the existing FULL widget.
// Visual rectangles are also the hit-test source; all six dispatches below
// still call the original camera functions with no protocol/state changes.
static bool cameraOptions=false;
static void cameraButton(const Rect& b,ui::HitRect r,ui::Icon icon,const char* text,uint16_t bg,uint16_t fg=TFT_WHITE) {
  r.x+=b.x;r.y+=b.y;
  canvas.fillRoundRect(r.x,r.y,r.w,r.h,8,bg);
  ui::drawIcon(canvas,icon,r.x+8,r.y+12,fg);
  canvas.setFont(&fonts::FreeSansBold9pt7b);canvas.setTextColor(fg,bg);
  canvas.drawString(text,r.x+38,r.y+15);
}
static void renderWidgetCameraRemote(const Rect& b,const TelemetryState& state,bool force) {
  (void)state;(void)force;
  // Two small presentation states, no new camera state/acknowledgement.
  canvas.fillRect(b.x,b.y,b.w,b.h,COLOR_BG);canvas.setTextPadding(0);
  const bool pairing=isCameraPairing(),waking=isCameraWaking();
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(pairing||waking?COLOR_AMBER:isCameraSubscribed()?COLOR_GREEN:COLOR_LABEL,COLOR_BG);
  canvas.drawString(pairing?"Pairing (30s)":waking?"Waking (10s)":
    isCameraSubscribed()?"Connected":"Not connected",b.x+8,b.y+8);
  if(!cameraOptions) {
    cameraButton(b,ui::captureRect(0,b.w),ui::Icon::Search,pairing?"Pairing...":"Pair",ui::panel);
    cameraButton(b,ui::captureRect(1,b.w),ui::Icon::Camera,"Shutter",COLOR_CYAN,TFT_BLACK);
    cameraButton(b,ui::captureRect(2,b.w),ui::Icon::List,"Options",ui::panel);
  } else {
    cameraButton(b,ui::captureRect(0,b.w),ui::Icon::Back,"Back",ui::panel);
    const char* labels[]={"Mode","Screen","Wake","Off"};
    const ui::Icon icons[]={ui::Icon::Mode,ui::Icon::Screen,ui::Icon::Wake,ui::Icon::Power};
    for(int i=0;i<4;i++){
      auto r=ui::optionRect(i,b.w);
      const uint16_t fg=i==3?COLOR_AMBER:i==2&&!hasCameraWakeBytes()?COLOR_LABEL:TFT_WHITE;
      cameraButton(b,r,icons[i],labels[i],ui::panel,fg);
    }
  }
  canvas.setFont(&fonts::FreeSansBold9pt7b);canvas.setTextColor(COLOR_LABEL,COLOR_BG);
  canvas.drawString(cameraOptions?(hasCameraWakeBytes()?"Camera controls":"Wake needs setup"):"Shutter follows mode",b.x+8,b.y+226);
}
static bool touchWidgetCameraRemote(const Rect& b,int16_t x,int16_t y) {
  x-=b.x;y-=b.y;
  if(!cameraOptions){
    if(ui::captureRect(0,b.w).contains(x,y)){startCameraPairing();return true;}
    if(ui::captureRect(1,b.w).contains(x,y)){triggerCameraShutter();return true;}
    if(ui::captureRect(2,b.w).contains(x,y)){cameraOptions=true;return true;}
  } else {
    if(ui::captureRect(0,b.w).contains(x,y)){cameraOptions=false;return true;}
    for(int i=0;i<4;i++)if(ui::optionRect(i,b.w).contains(x,y)){
      switch(i){
        case 0:triggerCameraMode();break;
        case 1:triggerCameraScreenToggle();break;
        case 2:wakeSleepingCamera();break;
        case 3:triggerCameraPowerOff();break;
      }
      return true;
    }
  }
  return false;
}

static const WidgetDescriptor s_descriptors[WIDGET_TYPE_COUNT] = {
  {WIDGET_NONE,            nullptr,              nullptr},
  {WIDGET_SPEED,           renderWidgetSpeed,    nullptr},
  {WIDGET_AVG_SPEED,       renderWidgetAvgSpeed, nullptr},
  {WIDGET_MAX_SPEED,       renderWidgetMaxSpeed, nullptr},
  {WIDGET_DISTANCE,        renderWidgetDistance, nullptr},
  {WIDGET_RIDE_TIME,       renderWidgetRideTime, nullptr},
  {WIDGET_CADENCE,         renderWidgetCadence,  nullptr},
  {WIDGET_HEART_RATE,      renderWidgetHeartRate,nullptr},
  {WIDGET_POWER,           renderWidgetPower,    nullptr},
  {WIDGET_ALTITUDE,        renderWidgetAltitude,       nullptr},
  {WIDGET_GRADE,           renderWidgetGrade,          nullptr},
  {WIDGET_TOTAL_ASCENT,    renderWidgetTotalAscent,    nullptr},
  {WIDGET_ELEVATION_CHART, renderWidgetElevationChart, nullptr},
  {WIDGET_BATTERY,         renderWidgetBattery,  nullptr},
  {WIDGET_BLE_MANAGER,     renderWidgetBleManager, touchWidgetBleManager},
  {WIDGET_SETTINGS_LIST,   renderWidgetSettingsList, touchWidgetSettingsList},
  {WIDGET_CAMERA_REMOTE,   renderWidgetCameraRemote, touchWidgetCameraRemote},
};

void initWidgetRegistry() {
  Serial.println("[WIDGET REGISTRY] Initialized 15 modular widgets (Minimal style).");
}

const WidgetDescriptor* getWidgetDescriptor(WidgetType type) {
  if (type < WIDGET_TYPE_COUNT) return &s_descriptors[type];
  return &s_descriptors[WIDGET_NONE];
}

void renderWidget(WidgetType type, const TemplateSlot& slot, const TelemetryState& state, bool forceFullRedraw) {
  // An out-of-range type (e.g. a widget id from a newer/foreign companion-app
  // build) and an unsupported size class are the same failure: the slot cannot
  // be drawn. Both take the fail-safe blank fill so stale pixels from whatever
  // was previously in this slot can never be left behind.
  if (type >= WIDGET_TYPE_COUNT || !widgetSupportsSize(type, slot.size_class)) {
    if (forceFullRedraw) {
      canvas.fillRect(slot.rect.x, slot.rect.y, slot.rect.w, slot.rect.h, COLOR_BG);
    }
    return;
  }
  const WidgetRenderFn fn = s_descriptors[type].render_fn;
  if (fn != nullptr) {
    fn(slot.rect, state, forceFullRedraw);
  }
}

bool handleWidgetTouch(WidgetType type, const TemplateSlot& slot, int16_t x, int16_t y) {
  // Same gate as renderWidget(): a widget that isn't drawable in this slot must
  // not be tappable in it either.
  if (type >= WIDGET_TYPE_COUNT || !widgetSupportsSize(type, slot.size_class)) {
    return false;
  }
  if (s_descriptors[type].touch_fn != nullptr) {
    return s_descriptors[type].touch_fn(slot.rect, x, y);
  }
  return false;
}
