#include "ui/camera_widget.h"
#include "ui/camera_view.h"
#include "ui/components/button.h"
#include "ui/gfx.h"
#include "ui/engine/resolved_theme.h"
#include "ui/engine/widget_model.h"
#include "hardware/camera_control.h"
#include "hardware/display.h"
#include <stdio.h>
#include <string.h>

// See camera_view.h for the page flow. Render and touch share the geometry
// there and the scan rows cached here, so a tap always lands on the row that
// was drawn.

namespace {

camui::Page s_page = camui::PAGE_SCAN;
bool s_wasConnected = false;
int s_modePage = 0;
int s_selectedMode = 0;  // CAMERA_MODES index driving the capture page
camera_proto::CameraCandidate s_rows[camera_proto::CAMERA_MAX_CANDIDATES];
size_t s_rowCount = 0;

uint16_t tok(uitheme::ColorToken token) { return ui::activeTheme().colors[token]; }
uint16_t tone(wmodel::Tone t) { return ui::activeTheme().toneColor(t, uitheme::STYLE_TEXT_MUTED); }

ui::HitRect at(const Rect& b, ui::HitRect r) {
  r.x += b.x;
  r.y += b.y;
  return r;
}

void button(const Rect& b, const ui::HitRect& r, const char* text, ui::Icon icon, uitheme::StyleId style) {
  ui::drawButton(canvas, at(b, r), ui::ButtonProps{text, icon, true, false}, style, ui::activeTheme());
}

void text(const char* s, int x, int y, uint16_t color, GfxFont font = &fonts::FreeSansBold9pt7b) {
  canvas.setFont(font);
  canvas.setTextColor(color, tok(uitheme::TOK_BG));
  canvas.drawString(s, x, y);
}

void drawBattery(const Rect& b, const wmodel::CameraIndicatorModel& m) {
  const int x = b.x + b.w - 84, y = b.y + 10;
  const uint16_t level = m.charging ? tok(uitheme::TOK_ACCENT) : tone(m.battery_tone);
  canvas.fillRoundRect(x, y, 24, 12, 2, tok(uitheme::TOK_LABEL));
  canvas.fillRect(x + 2, y + 2, 20, 8, tok(uitheme::TOK_BG));
  canvas.fillRect(x + 24, y + 3, 3, 6, tok(uitheme::TOK_LABEL));
  const int fill = (20 * m.battery_fill + 50) / 100;
  if (fill > 0) canvas.fillRect(x + 2, y + 2, fill, 8, level);
  text(m.battery, x + 32, b.y + 8, level);
}

void drawHeader(const Rect& b, bool connected, const wmodel::CameraIndicatorModel& m) {
  if (connected) {
    text("Connected", b.x + 8, b.y + 8, tok(uitheme::TOK_SUCCESS));
    if (m.has_battery) drawBattery(b, m);
  } else if (cameraControlIsConnecting()) {
    text("Connecting...", b.x + 8, b.y + 8, tok(uitheme::TOK_WARNING));
  } else if (cameraControlIsScanning()) {
    text("Scanning...", b.x + 8, b.y + 8, tok(uitheme::TOK_WARNING));
  } else {
    text("Not connected", b.x + 8, b.y + 8, tok(uitheme::TOK_LABEL));
  }
}

void footer(const Rect& b, const char* left, uint16_t leftColor, const char* right) {
  if (left && left[0]) text(left, b.x + 8, b.y + 226, leftColor);
  if (right && right[0]) text(right, b.x + b.w - 8 - int(strlen(right)) * 10, b.y + 226, tok(uitheme::TOK_LABEL));
}

void renderScan(const Rect& b) {
  const bool saved = cameraControlHasSaved();
  const bool scanning = cameraControlIsScanning();
  button(b, camui::topButton(0, saved ? 2 : 1, b.w), scanning ? "Scanning" : "Scan", ui::Icon::Search,
         uitheme::STYLE_BUTTON_COMPACT_PRIMARY);
  if (saved) button(b, camui::topButton(1, 2, b.w), "Forget", ui::Icon::Minus, uitheme::STYLE_BUTTON_COMPACT_SECONDARY);

  camera_proto::CameraCandidate found[camera_proto::CAMERA_MAX_CANDIDATES];
  const size_t n = cameraControlCandidates(found, camera_proto::CAMERA_MAX_CANDIDATES);
  s_rowCount = camui::sortCandidates(found, n);
  memcpy(s_rows, found, sizeof(s_rows[0]) * s_rowCount);
  for (size_t i = 0; i < s_rowCount; ++i) {
    const ui::HitRect r = at(b, camui::scanRow(int(i), b.w));
    canvas.fillRoundRect(r.x, r.y, r.w, r.h, 8, tok(uitheme::TOK_PANEL));
    ui::drawIcon(canvas, ui::Icon::Camera, r.x + 8, r.y + 8, tok(uitheme::TOK_ACCENT));
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    canvas.setTextColor(tok(uitheme::TOK_TEXT), tok(uitheme::TOK_PANEL));
    canvas.drawString(s_rows[i].name, r.x + 40, r.y + 6);
    char rssi[12];
    camui::formatRssi(s_rows[i].rssi, rssi, sizeof(rssi));
    canvas.setFont(&fonts::Font0);
    canvas.setTextColor(tok(uitheme::TOK_LABEL), tok(uitheme::TOK_PANEL));
    canvas.drawString(rssi, r.x + 40, r.y + 26);
  }
  if (s_rowCount > 0) {
    footer(b, "Tap a camera to connect", tok(uitheme::TOK_LABEL), "");
  } else {
    // Two short lines: 9pt bold fits ~19 characters across the widget.
    const char* line1 = cameraControlIsConnecting() ? "Connecting to the"
                        : scanning                  ? "Looking for"
                        : saved                     ? "Reconnecting to the"
                                                    : "Turn the camera on,";
    const char* line2 = cameraControlIsConnecting() ? "camera..."
                        : scanning                  ? "cameras..."
                        : saved                     ? "saved camera..."
                                                    : "then tap Scan";
    text(line1, b.x + 16, b.y + 112, tok(uitheme::TOK_LABEL));
    text(line2, b.x + 16, b.y + 136, tok(uitheme::TOK_LABEL));
  }
}

void renderModes(const Rect& b, const camera_proto::CameraStatus& cam, const wmodel::CameraIndicatorModel& m) {
  button(b, camui::topButton(0, 2, b.w), "More", ui::Icon::List, uitheme::STYLE_BUTTON_COMPACT_SECONDARY);
  button(b, camui::topButton(1, 2, b.w), "Next", ui::Icon::Next, uitheme::STYLE_BUTTON_COMPACT_SECONDARY);
  const char* active = camera_proto::cameraModeName(cam);
  for (int i = 0; i < camui::kModeCells; ++i) {
    const int index = s_modePage * camui::kModeCells + i;
    if (index >= int(camera_proto::CAMERA_MODE_COUNT)) break;
    const camera_proto::CameraModeEntry& mode = camera_proto::CAMERA_MODES[index];
    const bool on = active && !strcmp(active, mode.label);
    ui::drawButton(canvas, at(b, camui::modeCell(i, b.w)), ui::ButtonProps{mode.short_label, camui::modeIcon(mode), true, false},
                   on ? uitheme::STYLE_BUTTON_COMPACT_PRIMARY : uitheme::STYLE_BUTTON_COMPACT_SECONDARY, ui::activeTheme());
  }
  char page[8];
  snprintf(page, sizeof(page), "%d/%d", s_modePage + 1, camui::modePages());
  footer(b, m.storage, tone(m.storage_tone), page);
}

void renderCapture(const Rect& b, const camera_proto::CameraStatus& cam, const wmodel::CameraIndicatorModel& m) {
  const camera_proto::CameraModeEntry& mode = camera_proto::CAMERA_MODES[s_selectedMode];
  const bool photo = mode.scope == camera_proto::SCOPE_PHOTO;
  button(b, camui::topButton(0, 2, b.w), "Modes", ui::Icon::Back, uitheme::STYLE_BUTTON_COMPACT_SECONDARY);
  const ui::HitRect name = at(b, camui::topButton(1, 2, b.w));
  text(photo ? "PHOTO" : "VIDEO", name.x + 4, name.y + 4, tok(uitheme::TOK_LABEL), &fonts::Font0);
  text(mode.label, name.x + 4, name.y + 20, tok(uitheme::TOK_ACCENT));

  const camui::CaptureView v = camui::captureView(cam, photo);
  const ui::HitRect p = at(b, camui::indicatorRect(b.w));
  canvas.fillRoundRect(p.x, p.y, p.w, p.h, 8, tok(uitheme::TOK_PANEL));
  int statusX = p.x + 12;
  if (v.rec_dot) {
    canvas.fillRoundRect(p.x + 12, p.y + 16, 12, 12, 6, tok(uitheme::TOK_DANGER));
    statusX += 22;
  }
  canvas.setFont(&fonts::FreeSansBold12pt7b);
  canvas.setTextColor(tone(v.status_tone), tok(uitheme::TOK_PANEL));
  canvas.drawString(v.status, statusX, p.y + 10);
  canvas.setFont(&fonts::Font0);
  canvas.setTextColor(tone(v.detail_tone), tok(uitheme::TOK_PANEL));
  canvas.drawString(v.detail, p.x + 12, p.y + 46);

  for (int i = 0; i < v.action_count; ++i)
    button(b, camui::actionRect(i, v.action_count, b.w), v.actions[i].label, v.actions[i].icon, v.actions[i].style);
  footer(b, m.storage[0] ? m.storage : "", tone(m.storage_tone), "");
}

void renderMore(const Rect& b, const wmodel::CameraIndicatorModel& m) {
  button(b, camui::topButton(0, 2, b.w), "Back", ui::Icon::Back, uitheme::STYLE_BUTTON_COMPACT_SECONDARY);
  button(b, camui::modeCell(0, b.w), "Off", ui::Icon::Power, uitheme::STYLE_BUTTON_COMPACT_SECONDARY);
  button(b, camui::modeCell(1, b.w), "Forget", ui::Icon::Minus, uitheme::STYLE_BUTTON_COMPACT_SECONDARY);
  text("Off is experimental", b.x + 16, b.y + 168, tok(uitheme::TOK_LABEL));
  text("on the Ace Pro 2", b.x + 16, b.y + 190, tok(uitheme::TOK_LABEL));
  footer(b, m.storage, tone(m.storage_tone), "");
}

bool hit(const ui::HitRect& r, int16_t x, int16_t y) { return r.contains(x, y); }

}  // namespace

void renderCameraWidget(const Rect& b) {
  canvas.fillRect(b.x, b.y, b.w, b.h, tok(uitheme::TOK_BG));
  canvas.setTextPadding(0);
  const bool connected = cameraControlIsConnected();
  s_page = camui::syncPage(s_page, connected, s_wasConnected);
  camera_proto::CameraStatus cam;
  cameraControlStatus(cam);
  wmodel::CameraIndicatorModel m;
  wmodel::modelCameraIndicator(cam.battery_pct, cam.charging, cam.card_state, cam.free_bytes, cam.capture_state,
                               cam.capture_secs, camera_proto::cameraModeName(cam), m);
  drawHeader(b, connected, m);
  switch (s_page) {
    case camui::PAGE_SCAN: renderScan(b); break;
    case camui::PAGE_MODES: renderModes(b, cam, m); break;
    case camui::PAGE_CAPTURE: renderCapture(b, cam, m); break;
    case camui::PAGE_MORE: renderMore(b, m); break;
  }
}

bool touchCameraWidget(const Rect& b, int16_t x, int16_t y) {
  x -= b.x;
  y -= b.y;
  s_page = camui::syncPage(s_page, cameraControlIsConnected(), s_wasConnected);
  switch (s_page) {
    case camui::PAGE_SCAN: {
      const bool saved = cameraControlHasSaved();
      if (hit(camui::topButton(0, saved ? 2 : 1, b.w), x, y)) { cameraControlScan(); return true; }
      if (saved && hit(camui::topButton(1, 2, b.w), x, y)) { cameraControlForget(); return true; }
      for (size_t i = 0; i < s_rowCount; ++i)
        if (hit(camui::scanRow(int(i), b.w), x, y)) { cameraControlConnect(s_rows[i]); return true; }
      return false;
    }
    case camui::PAGE_MODES: {
      if (hit(camui::topButton(0, 2, b.w), x, y)) { s_page = camui::PAGE_MORE; return true; }
      if (hit(camui::topButton(1, 2, b.w), x, y)) { s_modePage = (s_modePage + 1) % camui::modePages(); return true; }
      for (int i = 0; i < camui::kModeCells; ++i) {
        const int index = s_modePage * camui::kModeCells + i;
        if (index >= int(camera_proto::CAMERA_MODE_COUNT) || !hit(camui::modeCell(i, b.w), x, y)) continue;
        s_selectedMode = index;
        setCameraMode(camera_proto::CAMERA_MODES[index].scope, camera_proto::CAMERA_MODES[index].subMode);
        s_page = camui::PAGE_CAPTURE;
        return true;
      }
      return false;
    }
    case camui::PAGE_CAPTURE: {
      if (hit(camui::topButton(0, 2, b.w), x, y)) { s_page = camui::PAGE_MODES; return true; }
      camera_proto::CameraStatus cam;
      cameraControlStatus(cam);
      const bool photo = camera_proto::CAMERA_MODES[s_selectedMode].scope == camera_proto::SCOPE_PHOTO;
      const camui::CaptureView v = camui::captureView(cam, photo);
      for (int i = 0; i < v.action_count; ++i) {
        if (!hit(camui::actionRect(i, v.action_count, b.w), x, y)) continue;
        switch (v.actions[i].op) {
          case camui::OP_RECORD: cameraControlStartRecording(); break;
          case camui::OP_STOP: cameraControlStopRecording(); break;
          case camui::OP_PHOTO: cameraControlTakePhoto(); break;
        }
        return true;
      }
      return false;
    }
    case camui::PAGE_MORE: {
      if (hit(camui::topButton(0, 2, b.w), x, y)) { s_page = camui::PAGE_MODES; return true; }
      if (hit(camui::modeCell(0, b.w), x, y)) { cameraControlPowerOff(); return true; }
      if (hit(camui::modeCell(1, b.w), x, y)) { cameraControlForget(); s_page = camui::PAGE_SCAN; return true; }
      return false;
    }
  }
  return false;
}
