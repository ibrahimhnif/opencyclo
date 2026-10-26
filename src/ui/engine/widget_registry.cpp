#include "hardware/sensor_catalog.h"
#include "hardware/baro_calibration.h"
#include "ui/scroll_gesture.h"
#include "widget_registry.h"
#include "widget_catalog.h"
#include "theme.h"
#include "resolved_theme.h"
#include "ui/components/hero_value.h"
#include "ui/components/value_tile.h"
#include "widget_model.h"
#include "ui/power_menu.h"
#include "ui/control_layout.h"
#include "storage/settings.h"
#include "storage/gps_cache.h"
#include "hardware/battery.h"
#include "hardware/ble_task.h"
#include "hardware/ble_layout_sync.h"
#include "hardware/ble_camera_remote.h"
#include "hardware/camera_control.h"
#include "hardware/camera_protocol.h"
#include <stdio.h>

// Every color comes from the active resolved theme, so a browser-pushed theme
// recolors the panel live. Defaults equal the legacy hardcoded palette.
static uint16_t tok(uitheme::ColorToken token) { return ui::activeTheme().colors[token]; }
static ui::HitRect hitRect(const Rect& b) { return {b.x, b.y, b.w, b.h}; }
static bool imperial() { return g_settings.units == 1; }

static void drawTile(const Rect& b, const wmodel::TileModel& m, bool force) {
  ui::drawValueTile(canvas, hitRect(b), m, force, ui::activeTheme());
}

// 1. SPEED (hero) — the one widget with its own layout, everything else is a "tile."
static void renderWidgetSpeed(const Rect& b, const TelemetryState& state, bool force) {
  wmodel::HeroModel m;
  wmodel::modelSpeed(state.display_speed_kmh, imperial(),
                     state.speed_source == SPEED_SOURCE_BLE_CSC ? wmodel::SPEED_HINT_BLE : wmodel::SPEED_HINT_GPS, m);
  ui::drawHeroValue(canvas, hitRect(b), m, force, ui::activeTheme());
}

// Tile text, units and tones live in widget_model (shared with the browser
// preview); these functions only pick the model and draw it.
static void renderWidgetDistance(const Rect& b, const TelemetryState& state, bool force) {
  wmodel::TileModel m;
  wmodel::modelDistance(state.trip_distance_km, imperial(), m);
  drawTile(b, m, force);
}

static void renderWidgetRideTime(const Rect& b, const TelemetryState& state, bool force) {
  wmodel::TileModel m;
  wmodel::modelRideTime(state.ride_time_s, m);
  drawTile(b, m, force);
}

static void renderWidgetCadence(const Rect& b, const TelemetryState& state, bool force) {
  wmodel::TileModel m;
  wmodel::modelCadence(state.cadence_rpm, m);
  drawTile(b, m, force);
}

static void renderWidgetHeartRate(const Rect& b, const TelemetryState& state, bool force) {
  wmodel::TileModel m;
  wmodel::modelHeartRate(state.heart_rate_bpm, m);
  drawTile(b, m, force);
}

static void renderWidgetPower(const Rect& b, const TelemetryState& state, bool force) {
  wmodel::TileModel m;
  wmodel::modelPower(state.power_watts, m);
  drawTile(b, m, force);
}

static void renderWidgetAltitude(const Rect& b, const TelemetryState& state, bool force) {
  wmodel::TileModel m;
  wmodel::modelAltitude(state.altitude_m, state.altitude_valid, imperial(), m);
  drawTile(b, m, force);
}

// Known limitation: a grade at or past +/-10.0% ("+15.0%" = 81px bold)
// exceeds a 74px SMALL tile's 66px value budget. No default page puts Grade
// in a SMALL slot; left as a documented edge case.
static void renderWidgetGrade(const Rect& b, const TelemetryState& state, bool force) {
  wmodel::TileModel m;
  wmodel::modelGrade(state.grade_pct, m);
  drawTile(b, m, force);
}

static void renderWidgetTotalAscent(const Rect& b, const TelemetryState& state, bool force) {
  wmodel::TileModel m;
  wmodel::modelTotalAscent(state.total_ascent_m, imperial(), m);
  drawTile(b, m, force);
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
    canvas.fillRect(b.x, b.y, b.w, b.h, tok(uitheme::TOK_BG));
    canvas.drawFastHLine(b.x, b.y, b.w, tok(uitheme::TOK_HAIRLINE));
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextColor(tok(uitheme::TOK_LABEL), tok(uitheme::TOK_BG));
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
  canvas.fillRect(innerX, innerY, innerW, innerH, tok(uitheme::TOK_BG));

  int prevPx = -1, prevPy = -1;
  for (uint8_t i = 0; i < count; i++) {
    uint8_t idx = s_elevFilled ? ((s_elevHead + i) % ELEV_SAMPLES) : i;
    float val = s_elevHistory[idx];
    int px = innerX + (i * innerW) / (ELEV_SAMPLES - 1);
    int py = innerY + innerH - (int)(((val - minAlt) / (maxAlt - minAlt)) * (innerH - 4));
    if (prevPx != -1) {
      canvas.drawLine(prevPx, prevPy, px, py, tok(uitheme::TOK_SUCCESS));
    }
    prevPx = px;
    prevPy = py;
  }
}

static void renderWidgetAvgSpeed(const Rect& b, const TelemetryState& state, bool force) {
  wmodel::TileModel m;
  wmodel::modelAvgSpeed(state.avg_speed_kmh, imperial(), m);
  drawTile(b, m, force);
}

static void renderWidgetMaxSpeed(const Rect& b, const TelemetryState& state, bool force) {
  wmodel::TileModel m;
  wmodel::modelMaxSpeed(state.max_speed_kmh, imperial(), m);
  drawTile(b, m, force);
}

static void renderWidgetBattery(const Rect& b, const TelemetryState& state, bool force) {
  wmodel::TileModel m;
  wmodel::modelBattery(state.battery_pct, m);
  drawTile(b, m, force);
}

static bool sensorDebug=false,sensorResults=false;
static bool baroCalibrationOpen=false,baroCanSave=false;
static int baroElevation=0;
static bool baroSubmitted=false;
static const char* calibrationMessage() {
  if (!baroSubmitted) return "Enter known elevation (m)";
  switch(getBaroCalibrationStatus()) {
    case BaroCalibrationStatus::Saving:return "Saving...";
    case BaroCalibrationStatus::Saved:return "Saved";
    case BaroCalibrationStatus::NoData:return "No fresh barometer data";
    case BaroCalibrationStatus::RideActive:return "Finish ride before calibrating";
    case BaroCalibrationStatus::Failed:return "Save failed / invalid reference";
    default:return "Enter known elevation (m)";
  }
}
static ui::ScrollGesture sensorScroll;
struct SensorListItem{SensorRow row;bool group=false;int top=0;};
static SensorListItem sensorItems[48];
static unsigned sensorItemCount=0;
static int sensorContentHeight=0;
static SensorSnapshot sensorFrame;
static bool sensorBodyDrag=false;
static bool alreadyPaired(const SensorRow& row){for(unsigned i=0;i<sensorFrame.pairedCount;i++)if(!strcmp(row.mac,sensorFrame.paired[i].mac))return true;return false;}
static void buildSensorItems(){
  if(sensorScroll.active())return; // Freeze identity/layout for the whole gesture.
  sensorFrame=getSensorSnapshot();sensorItemCount=0;sensorContentHeight=0;
  const auto* rows=sensorResults?sensorFrame.found:sensorFrame.paired;
  unsigned count=sensorResults?sensorFrame.foundCount:sensorFrame.pairedCount;
  for(int kind=0;kind<5;kind++){
    bool group=false;
    for(unsigned i=0;i<count;i++){
      if(int(rows[i].kind)!=kind || (sensorResults && alreadyPaired(rows[i])))continue;
      if(!group){auto& item=sensorItems[sensorItemCount++];item.row=rows[i];item.group=true;item.top=sensorContentHeight;sensorContentHeight+=22;group=true;}
      auto& item=sensorItems[sensorItemCount++];item.row=rows[i];item.group=false;item.top=sensorContentHeight;sensorContentHeight+=56;
    }
  }
}
static bool touchWidgetBleManager(const Rect& b,int16_t x,int16_t y);
static void renderWidgetBleManager(const Rect& b,const TelemetryState& state,bool force) {
  (void)force;
  canvas.fillRect(b.x,b.y,b.w,b.h,tok(uitheme::TOK_BG));canvas.setTextPadding(0);
  if(baroCalibrationOpen) {
    canvas.setFont(&fonts::FreeSansBold9pt7b);canvas.setTextColor(tok(uitheme::TOK_ACCENT),tok(uitheme::TOK_BG));
    canvas.drawString("Back",b.x+8,b.y+12);
    canvas.drawString("Calibrate",b.x+112,b.y+12);
    char line[48];snprintf(line,sizeof(line),"%d m",baroElevation);
    canvas.setTextColor(tok(uitheme::TOK_TEXT),tok(uitheme::TOK_BG));canvas.drawString(line,b.x+86,b.y+58);
    const char* labels[]={"-10","-1","+1","+10"};
    for(int i=0;i<4;i++){
      canvas.fillRoundRect(b.x+8+i*55,b.y+90,50,42,6,tok(uitheme::TOK_PANEL));
      canvas.setTextColor(tok(uitheme::TOK_ACCENT),tok(uitheme::TOK_PANEL));canvas.drawString(labels[i],b.x+14+i*55,b.y+102);
    }
    canvas.setFont(&fonts::Font0);canvas.setTextColor(tok(uitheme::TOK_LABEL),tok(uitheme::TOK_BG));
    snprintf(line,sizeof(line),"Reference %.2f hPa",getBaroReference());canvas.drawString(line,b.x+8,b.y+148);
    baroCanSave=state.baro_valid && state.baro_age_ms<2000 && state.ride_state==RIDE_STATE_IDLE &&
      getBaroCalibrationStatus()!=BaroCalibrationStatus::Saving;
    canvas.drawString(state.ride_state!=RIDE_STATE_IDLE?"Finish ride to calibrate":
      !state.baro_valid?"Waiting for barometer":calibrationMessage(),b.x+8,b.y+169);
    canvas.fillRoundRect(b.x+8,b.y+184,b.w-16,40,8,tok(uitheme::TOK_PANEL));
    canvas.setFont(&fonts::FreeSansBold9pt7b);canvas.setTextColor(baroCanSave?tok(uitheme::TOK_ACCENT):tok(uitheme::TOK_LABEL),tok(uitheme::TOK_PANEL));
    canvas.drawString("Save",b.x+90,b.y+198);
    canvas.setFont(&fonts::Font0);canvas.setTextColor(tok(uitheme::TOK_ACCENT),tok(uitheme::TOK_BG));
    canvas.drawString(getBaroAutoEnabled()?(baroAutoDone()?"GPS auto: ON - calibrated":"GPS auto: ON - waiting stable fix"):
      "GPS auto: OFF - tap to enable",b.x+8,b.y+240);return;
  }
  if(sensorDebug) {
    canvas.setFont(&fonts::Font0);canvas.setTextColor(tok(uitheme::TOK_ACCENT),tok(uitheme::TOK_BG));
    canvas.drawString("Sensor debug - tap to close",b.x+8,b.y+10);
    char line[80];const int x=b.x+8;
    snprintf(line,sizeof(line),"Speed raw %.1f / shown %.1f",state.speed_kmh,state.display_speed_kmh);canvas.drawString(line,x,b.y+42);
    snprintf(line,sizeof(line),"Source %u | Cadence %d",state.speed_source,state.cadence_rpm);canvas.drawString(line,x,b.y+68);
    snprintf(line,sizeof(line),"GPS fix %u sats %u HDOP %.1f",state.gps_fix_quality,state.satellites,state.hdop);canvas.drawString(line,x,b.y+94);
    snprintf(line,sizeof(line),"Baro %s age %lu ms",state.baro_valid?"OK":"missing/stale",(unsigned long)state.baro_age_ms);canvas.drawString(line,x,b.y+120);
    snprintf(line,sizeof(line),"Pressure %.2f hPa / %.1f C",state.baro_pressure_hpa,state.baro_temperature_c);canvas.drawString(line,x,b.y+146);
    snprintf(line,sizeof(line),"Altitude %.1f source %u",state.altitude_m,state.altitude_source);canvas.drawString(line,x,b.y+172);
    canvas.drawString("Alt source: 0 none 1 baro 2 GPS",x,b.y+198);
    snprintf(line,sizeof(line),"Baro reference: %.2f hPa",getBaroReference());canvas.drawString(line,x,b.y+222);
    snprintf(line,sizeof(line),"GPS aid: %s",gpsCacheLabel());canvas.drawString(line,x,b.y+244);return;
  }
  buildSensorItems();
  const int listTop=54,listBottom=b.h-42,listHeight=listBottom-listTop;
  sensorScroll.clamp(std::max(0,sensorContentHeight-listHeight));
  canvas.setFont(&fonts::FreeSansBold9pt7b);canvas.setTextColor(tok(uitheme::TOK_ACCENT),tok(uitheme::TOK_BG));
  canvas.drawString(sensorResults?"Back":"Paired",b.x+8,b.y+18);
  canvas.fillRoundRect(b.x+b.w-88,b.y+6,80,40,8,tok(uitheme::TOK_PANEL));
  canvas.setTextColor(sensorFrame.scanning?tok(uitheme::TOK_WARNING):tok(uitheme::TOK_ACCENT),tok(uitheme::TOK_PANEL));
  canvas.drawString(sensorFrame.scanning?"Scan...":"Scan",b.x+b.w-76,b.y+18);
  canvas.setClipRect(b.x,b.y+listTop,b.w,listHeight);
  for(unsigned i=0;i<sensorItemCount;i++){
    const auto& item=sensorItems[i];int y=b.y+listTop+item.top-sensorScroll.offset;
    if(y+56<b.y+listTop || y>=b.y+listBottom)continue;
    if(item.group){canvas.setFont(&fonts::Font0);canvas.setTextColor(tok(uitheme::TOK_ACCENT),tok(uitheme::TOK_BG));canvas.drawString(sensorKindName(item.row.kind),b.x+8,y+6);continue;}
    canvas.setFont(&fonts::Font0);canvas.setTextColor(TFT_WHITE,tok(uitheme::TOK_BG));
    const char* name=item.row.name[0]?item.row.name:sensorKindName(item.row.kind);
    char shortName[22];snprintf(shortName,sizeof(shortName),"%.20s",name);
    canvas.drawString(shortName,b.x+8,y+3);
    canvas.setTextColor(tok(uitheme::TOK_LABEL),tok(uitheme::TOK_BG));canvas.drawString(item.row.mac,b.x+8,y+18);
    char status[32];if(sensorResults)snprintf(status,sizeof(status),"%d dBm",item.row.rssi);
    else snprintf(status,sizeof(status),"%s",item.row.connected?"Connected":"Offline");
    canvas.drawString(status,b.x+8,y+33);
    canvas.fillRoundRect(b.x+b.w-76,y+4,68,36,6,tok(uitheme::TOK_PANEL));
    canvas.setTextColor(sensorFrame.busy?tok(uitheme::TOK_LABEL):sensorResults?tok(uitheme::TOK_ACCENT):tok(uitheme::TOK_DANGER),tok(uitheme::TOK_PANEL));
    canvas.drawString(sensorResults?"Connect":"Forget",b.x+b.w-65,y+17);
    canvas.drawFastHLine(b.x+8,y+53,b.w-16,tok(uitheme::TOK_HAIRLINE));
  }
  if(!sensorItemCount){canvas.setFont(&fonts::Font0);canvas.setTextColor(tok(uitheme::TOK_LABEL),tok(uitheme::TOK_BG));canvas.drawString(sensorResults?(sensorFrame.scanning?"Searching...":"No new sensors. Tap Scan."):"No paired sensors. Tap Scan.",b.x+8,b.y+90);}
  canvas.clearClipRect();
  if(sensorContentHeight>listHeight){int height=std::max(10,listHeight*listHeight/sensorContentHeight);int y=b.y+listTop+(listHeight-height)*sensorScroll.offset/(sensorContentHeight-listHeight);canvas.fillRect(b.x+b.w-3,y,2,height,tok(uitheme::TOK_ACCENT));}
  canvas.setFont(&fonts::Font0);canvas.setTextColor(tok(uitheme::TOK_LABEL),tok(uitheme::TOK_BG));
  char briefStatus[36];snprintf(briefStatus,sizeof(briefStatus),"%.35s",sensorFrame.full?"Scan full (24). Rescan closer.":sensorFrame.status);
  canvas.drawString(briefStatus,b.x+8,b.y+b.h-36);
  canvas.setTextColor(tok(uitheme::TOK_ACCENT),tok(uitheme::TOK_BG));canvas.drawString("Debug sensors",b.x+8,b.y+b.h-16);
  canvas.drawString("Calibrate",b.x+144,b.y+b.h-16);
}
static bool touchWidgetBleManager(const Rect& b,int16_t x,int16_t y) {
  if(baroCalibrationOpen){
    if(y>=b.y+228 && y<b.y+258){setBaroAutoEnabled(!getBaroAutoEnabled());return true;}
    if(y<b.y+42 && x<b.x+95){baroCalibrationOpen=false;return true;}
    if(getBaroCalibrationStatus()==BaroCalibrationStatus::Saving)return true;
    if(y>=b.y+90 && y<b.y+132 && x>=b.x+8 && x<b.x+228){
      int index=(x-b.x-8)/55;const int steps[]={-10,-1,1,10};
      baroElevation=std::max(-500,std::min(9000,baroElevation+steps[index]));baroSubmitted=false;
    }
    if(y>=b.y+184 && y<b.y+224 && x>=b.x+8 && x<b.x+b.w-8 && baroCanSave)
      baroSubmitted=requestBaroCalibration(baroElevation);
    return true;
  }
  if(sensorDebug){sensorDebug=false;return true;}
  if(y>=b.y+b.h-24 && x>=b.x+130){baroCalibrationOpen=true;baroSubmitted=false;return true;}
  if(y>=b.y+b.h-24){sensorDebug=true;return true;}
  if(y>=b.y+6 && y<b.y+46){
    if(sensorResults && x<b.x+90){sensorResults=false;sensorScroll.offset=0;return true;}
    if(x>=b.x+b.w-88){sensorResults=true;sensorScroll.offset=0;triggerBleScan();return true;}
  }
  if(sensorFrame.busy || y<b.y+54 || y>=b.y+b.h-42 || x<b.x+b.w-76)return false;
  int contentY=y-b.y-54+sensorScroll.offset;
  for(unsigned i=0;i<sensorItemCount;i++){
    const auto& item=sensorItems[i];if(item.group || contentY<item.top+4 || contentY>=item.top+40)continue;
    if(sensorResults)requestSensorConnect(item.row);else forgetSensorProfile(BleProfileType(item.row.profile));return true;
  }
  return false;
}
void cancelSensorWidgetTouch(){sensorScroll.cancel();}
bool handleSensorWidgetStream(const Rect& b,bool touched,int16_t x,int16_t y,int& pageSwipe) {
  if(!sensorScroll.active()){
    if(!touched || x<b.x || x>=b.x+b.w || y<b.y || y>=b.y+b.h)return false;
    buildSensorItems();sensorBodyDrag=y>=b.y+54 && y<b.y+b.h-42;
  }
  auto result=sensorScroll.update(touched,x,y,std::max(0,sensorContentHeight-(b.h-96)),sensorBodyDrag&&!sensorDebug&&!baroCalibrationOpen);
  if(result==ui::ScrollGesture::Tap)touchWidgetBleManager(b,sensorScroll.tapX(),sensorScroll.tapY());
  else if(result==ui::ScrollGesture::PreviousPage)pageSwipe=-1;
  else if(result==ui::ScrollGesture::NextPage)pageSwipe=1;
  return true;
}

// 15. SETTINGS LIST
//
// Row geometry is shared between the renderer and the touch handler below so
// the drawn rows and the tappable bands cannot drift apart: row `i`'s text cell
// starts at b.y + SETTINGS_ROW_Y0 + i * SETTINGS_ROW_STRIDE and is 18px tall
// (FreeSans9pt7b), with a divider hairline SETTINGS_ROW_DIVIDER px below that
// top edge. Rows in order: 0 units, 1 brightness, 2 wheel size, 3 sd logging,
// 4 gps source, 5 baro source, 6 compass source, 7 battery, 8 firmware.
// The power button below is hardcoded at b.y+208 within this widget's fixed
// 258px height, so STRIDE was reduced from 32 to 22 (adding the two new
// rows) to keep the last divider clear of it.
static const int SETTINGS_ROW_Y0      = 8;
static const int SETTINGS_ROW_STRIDE  = 22;
static const int SETTINGS_ROW_DIVIDER = 20;

static void renderWidgetSettingsList(const Rect& b, const TelemetryState& state, bool force) {
  if (force) {
    canvas.fillRect(b.x, b.y, b.w, b.h, tok(uitheme::TOK_BG));
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
    canvas.setTextColor(tok(uitheme::TOK_LABEL), tok(uitheme::TOK_BG));
    canvas.setTextPadding(110);
    canvas.drawString(label, b.x + 10, y);
    canvas.setTextPadding(0);
    canvas.setTextColor(valueColor, tok(uitheme::TOK_BG));
    canvas.setTextPadding(b.w - 120);
    canvas.drawString(value, b.x + 116, y);
    canvas.setTextPadding(0);
    canvas.drawFastHLine(b.x + 6, y + SETTINGS_ROW_DIVIDER, b.w - 12, tok(uitheme::TOK_HAIRLINE));
    y += SETTINGS_ROW_STRIDE;
  };

  row("units", g_settings.units == 0 ? "metric" : "imperial", tok(uitheme::TOK_TEXT));

  char brightStr[8];
  snprintf(brightStr, sizeof(brightStr), "%u%%", (g_settings.brightness * 100) / 255);
  row("brightness", brightStr, tok(uitheme::TOK_TEXT));

  char wheelStr[24];
  snprintf(wheelStr, sizeof(wheelStr), "%u mm", g_settings.wheel_circumference_mm);
  row("wheel size", wheelStr, tok(uitheme::TOK_TEXT));

  row("sd logging", g_settings.sd_logging_enabled ? "enabled" : "disabled",
      g_settings.sd_logging_enabled ? tok(uitheme::TOK_SUCCESS) : tok(uitheme::TOK_WARNING));

  row("gps source", g_settings.gps_source_mode == 0 ? "hardware" : "phone",
      g_settings.gps_source_mode == 0 ? tok(uitheme::TOK_TEXT) : tok(uitheme::TOK_ACCENT));

  row("baro source", g_settings.baro_source_mode == 0 ? "hardware" : "phone",
      g_settings.baro_source_mode == 0 ? tok(uitheme::TOK_TEXT) : tok(uitheme::TOK_ACCENT));

  row("compass source", g_settings.compass_source_mode == 0 ? "hardware" : "phone",
      g_settings.compass_source_mode == 0 ? tok(uitheme::TOK_TEXT) : tok(uitheme::TOK_ACCENT));

  char batStr[24];
  snprintf(batStr, sizeof(batStr), "%u%% (%.2fV)", state.battery_pct, readBatteryVoltage());
  row("battery", batStr, tok(uitheme::TOK_SUCCESS));

  row("firmware", "v0.2.0", tok(uitheme::TOK_ACCENT));

  // Filled controls are the only raised surfaces in the Minimal UI. Keep the
  // power entry in the same cyan/black treatment as the other primary action
  // buttons, and render it to the shared sprite like the rest of the page.
  if (b.h >= 252) {
    canvas.fillRoundRect(b.x + 10, b.y + 208, b.w - 20, 44, 8, tok(uitheme::TOK_ACCENT));
    canvas.setTextColor(TFT_BLACK, tok(uitheme::TOK_ACCENT));
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
  if (settingsRowHit(b, y, 4)) { // gps source
    setGpsSourceMode(g_settings.gps_source_mode == 0 ? 1 : 0);
    return true;
  }
  if (settingsRowHit(b, y, 5)) { // baro source
    setBaroSourceMode(g_settings.baro_source_mode == 0 ? 1 : 0);
    return true;
  }
  if (settingsRowHit(b, y, 6)) { // compass source
    setCompassSourceMode(g_settings.compass_source_mode == 0 ? 1 : 0);
    return true;
  }
  return false;
}

// 16. CAMERA REMOTE
//
// Two channels to an Insta360 Ace Pro 2:
// - The emulated "GPS Remote" accessory (ble_camera_remote.cpp, confirmed on
//   real hardware): Shutter (photo in photo mode, record start/stop toggle in
//   video mode), Screen Toggle (short press), Power Off (3s hold), and Wake
//   Camera (beacons the camera-specific bytes set via a dedicated BLE
//   characteristic -- see ble_camera_remote.h -- so a sleeping/off camera
//   notices and reconnects; the Wake button is greyed if no wake bytes are
//   set yet).
// - The direct BLE camera-control channel (hardware/camera_control.cpp), which
//   speaks the camera's own "OneDriver" protocol and can select arbitrary
//   capture modes -- including FreeFrame, which the GPS Remote's physical Mode
//   button cannot reach. That is the "Modes" picker. Pair starts that pairing
//   (and deliberately does NOT open the GPS-Remote pairing window, or the
//   camera would re-attach as a remote and stop advertising its control
//   service).
//
// Row geometry is shared between render and touch, same discipline as the
// BLE Manager / Settings List widgets above, so drawn buttons and tappable
// bands can't drift apart.
// Pages: Capture (Pair/Shutter/Options) -> Options (Back + Modes/Screen/Wake/
// Off) -> Modes (Back/Next + a paged 4-up mode list).
static int cameraPage=0;      // 0 = capture, 1 = options, 2 = mode picker
static int cameraModePage=0;
static int cameraSelected=-1;
static void cameraButton(const Rect& b,ui::HitRect r,ui::Icon icon,const char* text,uint16_t bg,uint16_t fg=TFT_WHITE) {
  r.x+=b.x;r.y+=b.y;
  canvas.fillRoundRect(r.x,r.y,r.w,r.h,8,bg);
  ui::drawIcon(canvas,icon,r.x+8,r.y+12,fg);
  canvas.setFont(&fonts::FreeSansBold9pt7b);canvas.setTextColor(fg,bg);
  canvas.drawString(text,r.x+38,r.y+15);
}
// What the camera reported over the direct link: a battery glyph + percent
// (top right) and free storage + recording timer / mode (bottom row, capture
// page). Hidden until the camera has answered, so nothing stale is shown.
static uint16_t cameraTone(wmodel::Tone tone) {
  return ui::activeTheme().toneColor(tone,uitheme::STYLE_TEXT_MUTED);
}
static void drawCameraBattery(const Rect& b,const wmodel::CameraIndicatorModel& m) {
  const int x=b.x+b.w-84,y=b.y+10;
  const uint16_t level=m.charging?tok(uitheme::TOK_ACCENT):cameraTone(m.battery_tone);
  canvas.fillRoundRect(x,y,24,12,2,tok(uitheme::TOK_LABEL));
  canvas.fillRect(x+2,y+2,20,8,tok(uitheme::TOK_BG));
  canvas.fillRect(x+24,y+3,3,6,tok(uitheme::TOK_LABEL));
  const int fill=(20*m.battery_fill+50)/100;
  if(fill>0)canvas.fillRect(x+2,y+2,fill,8,level);
  canvas.setFont(&fonts::FreeSansBold9pt7b);canvas.setTextColor(level,tok(uitheme::TOK_BG));
  canvas.drawString(m.battery,x+32,b.y+8);
}
static bool drawCameraStatusRow(const Rect& b,const wmodel::CameraIndicatorModel& m) {
  if(!m.storage[0]&&!m.capture[0])return false;
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  if(m.storage[0]){
    canvas.setTextColor(cameraTone(m.storage_tone),tok(uitheme::TOK_BG));
    canvas.drawString(m.storage,b.x+8,b.y+226);
  }
  if(m.capture[0]){
    canvas.setTextColor(cameraTone(m.capture_tone),tok(uitheme::TOK_BG));
    canvas.drawString(m.capture,b.x+120,b.y+226);
  }
  return true;
}
static void renderWidgetCameraRemote(const Rect& b,const TelemetryState& state,bool force) {
  (void)state;(void)force;
  canvas.fillRect(b.x,b.y,b.w,b.h,tok(uitheme::TOK_BG));canvas.setTextPadding(0);
  const bool pairing=isCameraPairing()||cameraControlIsPairing(),waking=isCameraWaking();
  const bool connected=isCameraSubscribed()||cameraControlIsConnected();
  camera_proto::CameraStatus cam;
  cameraControlStatus(cam);
  wmodel::CameraIndicatorModel indicator;
  wmodel::modelCameraIndicator(cam.battery_pct,cam.charging,cam.card_state,cam.free_bytes,
                               cam.capture_state,cam.capture_secs,camera_proto::cameraModeName(cam),indicator);
  if(indicator.has_battery)drawCameraBattery(b,indicator);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setTextColor(pairing||waking?tok(uitheme::TOK_WARNING):connected?tok(uitheme::TOK_SUCCESS):tok(uitheme::TOK_LABEL),tok(uitheme::TOK_BG));
  canvas.drawString(pairing?(cameraControlIsPairing()?"Camera pairing":"Pairing (30s)"):waking?"Waking (10s)":
    connected?"Connected":"Not connected",b.x+8,b.y+8);
  if(cameraPage==0) {
    cameraButton(b,ui::captureRect(0,b.w),ui::Icon::Search,pairing?"Pairing...":"Pair",tok(uitheme::TOK_PANEL));
    cameraButton(b,ui::captureRect(1,b.w),ui::Icon::Camera,"Shutter",tok(uitheme::TOK_ACCENT),TFT_BLACK);
    cameraButton(b,ui::captureRect(2,b.w),ui::Icon::List,"Options",tok(uitheme::TOK_PANEL));
  } else if(cameraPage==1) {
    cameraButton(b,ui::captureRect(0,b.w),ui::Icon::Back,"Back",tok(uitheme::TOK_PANEL));
    const char* labels[]={"Modes","Screen","Wake","Off"};
    const ui::Icon icons[]={ui::Icon::Mode,ui::Icon::Screen,ui::Icon::Wake,ui::Icon::Power};
    for(int i=0;i<4;i++){
      auto r=ui::optionRect(i,b.w);
      const uint16_t fg=i==3?tok(uitheme::TOK_WARNING):i==2&&!hasCameraWakeBytes()?tok(uitheme::TOK_LABEL):TFT_WHITE;
      cameraButton(b,r,icons[i],labels[i],tok(uitheme::TOK_PANEL),fg);
    }
  } else {
    cameraButton(b,ui::modeNav(0,b.w),ui::Icon::Back,"Back",tok(uitheme::TOK_PANEL));
    cameraButton(b,ui::modeNav(1,b.w),ui::Icon::List,"Next",tok(uitheme::TOK_PANEL));
    for(int i=0;i<4;i++){
      const int index=cameraModePage*4+i;
      if(index>=(int)camera_proto::CAMERA_MODE_COUNT)break;
      const bool sel=index==cameraSelected;
      cameraButton(b,ui::optionRect(i,b.w),ui::Icon::Mode,camera_proto::CAMERA_MODES[index].label,
                   sel?tok(uitheme::TOK_ACCENT):tok(uitheme::TOK_PANEL),sel?TFT_BLACK:TFT_WHITE);
    }
  }
  char hint[40];
  if(cameraPage==2){
    int last=cameraModePage*4+4;if(last>(int)camera_proto::CAMERA_MODE_COUNT)last=(int)camera_proto::CAMERA_MODE_COUNT;
    snprintf(hint,sizeof(hint),"Modes %d-%d of %d",cameraModePage*4+1,last,(int)camera_proto::CAMERA_MODE_COUNT);
  } else snprintf(hint,sizeof(hint),cameraPage==0?"Shutter follows mode":"Camera controls");
  if(cameraPage==0&&drawCameraStatusRow(b,indicator))return;
  canvas.setFont(&fonts::FreeSansBold9pt7b);canvas.setTextColor(tok(uitheme::TOK_LABEL),tok(uitheme::TOK_BG));
  canvas.drawString(hint,b.x+8,b.y+226);
}
static bool touchWidgetCameraRemote(const Rect& b,int16_t x,int16_t y) {
  x-=b.x;y-=b.y;
  if(cameraPage==0){
    if(ui::captureRect(0,b.w).contains(x,y)){cameraControlPair();return true;}
    if(ui::captureRect(1,b.w).contains(x,y)){triggerCameraShutter();return true;}
    if(ui::captureRect(2,b.w).contains(x,y)){cameraPage=1;return true;}
  } else if(cameraPage==1){
    if(ui::captureRect(0,b.w).contains(x,y)){cameraPage=0;return true;}
    for(int i=0;i<4;i++)if(ui::optionRect(i,b.w).contains(x,y)){
      if(i==0)cameraPage=2;
      else if(i==1)triggerCameraScreenToggle();
      else if(i==2)wakeSleepingCamera();
      else triggerCameraPowerOff();
      return true;
    }
  } else {
    if(ui::modeNav(0,b.w).contains(x,y)){cameraPage=1;return true;}
    if(ui::modeNav(1,b.w).contains(x,y)){
      const int pages=((int)camera_proto::CAMERA_MODE_COUNT+3)/4;
      cameraModePage=(cameraModePage+1)%pages;return true;
    }
    for(int i=0;i<4;i++)if(ui::optionRect(i,b.w).contains(x,y)){
      const int index=cameraModePage*4+i;
      if(index<(int)camera_proto::CAMERA_MODE_COUNT){
        cameraSelected=index;
        setCameraMode(camera_proto::CAMERA_MODES[index].scope,camera_proto::CAMERA_MODES[index].subMode);
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
      canvas.fillRect(slot.rect.x, slot.rect.y, slot.rect.w, slot.rect.h, tok(uitheme::TOK_BG));
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
