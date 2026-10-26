#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include "control_layout.h"
#include "icons.h"
#include "engine/theme.h"
#include "engine/widget_model.h"
#include "hardware/camera_protocol.h"

// What the camera widget shows, with no drawing and no hardware access.
// Host-tested (tests/test_camera_view.cpp); camera_widget.cpp draws it.
//
// Flow: Scan (find + pick a camera) -> Modes (pick a capture mode) ->
// Capture (record/stop, or take a photo, with a live indicator).
// More (Off / Forget) hangs off Modes. Losing the link always returns to Scan.
namespace camui {

enum Page : uint8_t { PAGE_SCAN = 0, PAGE_MODES, PAGE_CAPTURE, PAGE_MORE };

// Applies the connection rules to the page the user is on. `wasConnected`
// carries the link state between frames.
inline Page syncPage(Page page, bool connected, bool& wasConnected) {
  const bool fresh = connected && !wasConnected;
  wasConnected = connected;
  if (!connected) return PAGE_SCAN;
  if (fresh || page == PAGE_SCAN) return PAGE_MODES;
  return page;
}

// ---- Geometry (relative to the widget; shared by render and touch) --------
// Header text at y=8, footer text at y=232; everything tappable sits between.
constexpr int kScanRows = 3;
constexpr int kModeCells = 4;
inline ui::HitRect topButton(int index, int count, int width) {
  if (count <= 1) return {8, 40, width - 16, 48};
  const int half = (width - 24) / 2;
  return {8 + index * (half + 8), 40, half, 48};
}
inline ui::HitRect scanRow(int index, int width) { return {8, 96 + index * 44, width - 16, 40}; }
inline ui::HitRect modeCell(int index, int width) {
  const int half = (width - 24) / 2;
  return {8 + (index % 2) * (half + 8), 96 + (index / 2) * 56, half, 48};
}
inline ui::HitRect indicatorRect(int width) { return {8, 96, width - 16, 66}; }
inline ui::HitRect actionRect(int index, int count, int width) {
  if (count <= 1) return {8, 170, width - 16, 48};
  const int half = (width - 24) / 2;
  return {8 + index * (half + 8), 170, half, 48};
}
inline int modePages() { return (int(camera_proto::CAMERA_MODE_COUNT) + kModeCells - 1) / kModeCells; }

// ---- Scan list -------------------------------------------------------------
// Strongest signal first; returns how many rows are shown.
inline size_t sortCandidates(camera_proto::CameraCandidate* c, size_t n) {
  for (size_t i = 1; i < n; ++i)
    for (size_t j = i; j > 0 && c[j].rssi > c[j - 1].rssi; --j) {
      const camera_proto::CameraCandidate t = c[j];
      c[j] = c[j - 1];
      c[j - 1] = t;
    }
  return n < size_t(kScanRows) ? n : size_t(kScanRows);
}
inline void formatRssi(int8_t rssi, char* out, size_t n) { snprintf(out, n, "%d dBm", int(rssi)); }

// ---- Capture page ------------------------------------------------------------
enum Op : uint8_t { OP_RECORD = 0, OP_STOP, OP_PHOTO };
struct Action {
  const char* label;
  ui::Icon icon;
  uitheme::StyleId style;
  Op op;
};
struct CaptureView {
  char status[24];  // big line in the indicator panel
  wmodel::Tone status_tone;
  bool rec_dot;     // red recording dot before the status
  char detail[48];  // small line under it
  wmodel::Tone detail_tone;
  int action_count;
  Action actions[2];
};

inline CaptureView captureView(const camera_proto::CameraStatus& s, bool photo) {
  CaptureView v{};
  v.detail_tone = wmodel::TONE_MUTED;
  if (photo) {
    v.action_count = 1;
    v.actions[0] = {"Take photo", ui::Icon::Camera, uitheme::STYLE_BUTTON_COMPACT_PRIMARY, OP_PHOTO};
    switch (s.photo_state) {
      case camera_proto::PHOTO_PENDING:
        snprintf(v.status, sizeof(v.status), "Saving...");
        v.status_tone = wmodel::TONE_WARNING;
        snprintf(v.detail, sizeof(v.detail), "Camera is writing the photo");
        break;
      case camera_proto::PHOTO_SAVED:
        snprintf(v.status, sizeof(v.status), "Saved");
        v.status_tone = wmodel::TONE_SUCCESS;
        snprintf(v.detail, sizeof(v.detail), "%s", s.last_file[0] ? s.last_file : "Photo on the SD card");
        v.detail_tone = wmodel::TONE_SUCCESS;
        break;
      case camera_proto::PHOTO_FAILED:
        snprintf(v.status, sizeof(v.status), "Photo failed");
        v.status_tone = wmodel::TONE_DANGER;
        snprintf(v.detail, sizeof(v.detail), "Check the camera, then try again");
        break;
      default:
        snprintf(v.status, sizeof(v.status), "Ready");
        v.status_tone = wmodel::TONE_MUTED;
        snprintf(v.detail, sizeof(v.detail), "Tap Take photo");
        break;
    }
    return v;
  }
  v.action_count = 1;
  const Action stop{"Stop", ui::Icon::Stop, uitheme::STYLE_BUTTON_DANGER, OP_STOP};
  if (s.capture_request == camera_proto::CAPTURE_STOPPING) {
    snprintf(v.status, sizeof(v.status), "Saving...");
    v.status_tone = wmodel::TONE_WARNING;
    snprintf(v.detail, sizeof(v.detail), "Camera is writing the video");
    v.action_count = 0;
    return v;
  }
  if (s.capture_request == camera_proto::CAPTURE_STARTING) {
    snprintf(v.status, sizeof(v.status), "Starting...");
    v.status_tone = wmodel::TONE_WARNING;
    snprintf(v.detail, sizeof(v.detail), "Waiting for the camera");
    v.actions[0] = stop;
    return v;
  }
  if (s.capture_state > 0) {
    char t[12];
    wmodel::formatDuration(s.capture_secs, t, sizeof(t));
    snprintf(v.status, sizeof(v.status), "REC %s", t);
    v.status_tone = wmodel::TONE_DANGER;
    v.rec_dot = true;
    snprintf(v.detail, sizeof(v.detail), "Recording");
    v.actions[0] = stop;
    return v;
  }
  snprintf(v.status, sizeof(v.status), "Ready");
  v.status_tone = wmodel::TONE_MUTED;
  if (s.last_file[0]) {
    snprintf(v.detail, sizeof(v.detail), "Saved %s", s.last_file);
    v.detail_tone = wmodel::TONE_SUCCESS;
  } else {
    snprintf(v.detail, sizeof(v.detail), "Tap Record to start");
  }
  v.actions[0] = {"Record", ui::Icon::Record, uitheme::STYLE_BUTTON_DANGER, OP_RECORD};
  return v;
}

// Each capture mode's own icon (photo modes fall back to the camera).
inline ui::Icon modeIcon(const camera_proto::CameraModeEntry& m) {
  if (m.scope == camera_proto::SCOPE_PHOTO) {
    switch (m.subMode) {
      case 1: return ui::Icon::HdrPhoto;
      case 2: return ui::Icon::Interval;
      case 3: return ui::Icon::Burst;
      case 7: return ui::Icon::Starlapse;
      default: return ui::Icon::Camera;
    }
  }
  switch (m.subMode) {
    case camera_proto::VIDEO_SUB_FREEFRAME: return ui::Icon::Freeframe;
    case 2: return ui::Icon::Timelapse;
    case 3: return ui::Icon::Hdr;
    case 4: return ui::Icon::Timeshift;
    case 6: return ui::Icon::Loop;
    case 7: return ui::Icon::Fpv;
    case 8: return ui::Icon::Movie;
    case 9: return ui::Icon::Slowmo;
    case 11: return ui::Icon::Pure;
    case 14: return ui::Icon::Dashcam;
    default: return ui::Icon::Mode;
  }
}

}  // namespace camui
