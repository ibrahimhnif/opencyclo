// Pure camera-screen logic (src/ui/camera_view.h): which page shows, what the
// capture indicator says, which buttons appear, and the shared geometry.
#include "ui/camera_view.h"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace camui;
using camera_proto::CameraStatus;

static bool overlaps(const ui::HitRect& a, const ui::HitRect& b) {
  return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

int main() {
  // Page flow: no link -> Scan; a new link -> Modes; a dropped link -> Scan.
  {
    bool was = false;
    assert(syncPage(PAGE_MODES, false, was) == PAGE_SCAN && !was);
    assert(syncPage(PAGE_SCAN, true, was) == PAGE_MODES && was);
    assert(syncPage(PAGE_CAPTURE, true, was) == PAGE_CAPTURE);
    assert(syncPage(PAGE_MORE, true, was) == PAGE_MORE);
    assert(syncPage(PAGE_CAPTURE, false, was) == PAGE_SCAN && !was);
  }

  // Video idle: Ready + one red Record button.
  {
    CameraStatus s;
    s.capture_state = 0;
    CaptureView v = captureView(s, false);
    assert(!strcmp(v.status, "Ready") && !v.rec_dot && v.action_count == 1);
    assert(v.actions[0].op == OP_RECORD && !strcmp(v.actions[0].label, "Record"));
    assert(v.actions[0].style == uitheme::STYLE_BUTTON_DANGER);
    assert(!strcmp(v.detail, "Tap Record to start"));
  }

  // Video recording: red REC timer + dot and one Stop button (no pause).
  {
    CameraStatus s;
    s.capture_state = 1;
    s.capture_secs = 65;
    CaptureView v = captureView(s, false);
    assert(!strcmp(v.status, "REC 1:05") && v.rec_dot && v.status_tone == wmodel::TONE_DANGER);
    assert(v.action_count == 1 && v.actions[0].op == OP_STOP && !strcmp(v.actions[0].label, "Stop"));
    assert(v.actions[0].style == uitheme::STYLE_BUTTON_DANGER);
    s.capture_secs = 3725;
    v = captureView(s, false);
    assert(!strcmp(v.status, "REC 1:02:05"));
  }

  // The tap shows at once: Starting... (Stop still offered), then Saving...
  // with no button while the camera writes the file.
  {
    CameraStatus s;
    s.capture_state = 0;
    s.capture_request = camera_proto::CAPTURE_STARTING;
    CaptureView v = captureView(s, false);
    assert(!strcmp(v.status, "Starting...") && v.status_tone == wmodel::TONE_WARNING);
    assert(v.action_count == 1 && v.actions[0].op == OP_STOP);
    s.capture_state = 1;
    s.capture_request = camera_proto::CAPTURE_STOPPING;
    v = captureView(s, false);
    assert(!strcmp(v.status, "Saving...") && v.status_tone == wmodel::TONE_WARNING && v.action_count == 0);
  }

  // Video stopped with a saved file: the file name shows in green.
  {
    CameraStatus s;
    s.capture_state = 0;
    snprintf(s.last_file, sizeof(s.last_file), "VID_20261026_110301_255.mp4");
    CaptureView v = captureView(s, false);
    assert(!strcmp(v.detail, "Saved VID_20261026_110301_255.mp4"));
    assert(v.detail_tone == wmodel::TONE_SUCCESS);
  }

  // Photo: Ready -> Saving... -> Saved + file name, or Photo failed.
  {
    CameraStatus s;
    CaptureView v = captureView(s, true);
    assert(!strcmp(v.status, "Ready") && v.action_count == 1 && v.actions[0].op == OP_PHOTO);
    assert(!strcmp(v.actions[0].label, "Take photo"));
    s.photo_state = camera_proto::PHOTO_PENDING;
    v = captureView(s, true);
    assert(!strcmp(v.status, "Saving...") && v.status_tone == wmodel::TONE_WARNING);
    s.photo_state = camera_proto::PHOTO_SAVED;
    snprintf(s.last_file, sizeof(s.last_file), "IMG_20261026_110241_254.jpg");
    v = captureView(s, true);
    assert(!strcmp(v.status, "Saved") && v.status_tone == wmodel::TONE_SUCCESS);
    assert(!strcmp(v.detail, "IMG_20261026_110241_254.jpg"));
    s.photo_state = camera_proto::PHOTO_FAILED;
    v = captureView(s, true);
    assert(!strcmp(v.status, "Photo failed") && v.status_tone == wmodel::TONE_DANGER);
  }

  // Scan list: strongest signal first; at most kScanRows rows.
  {
    camera_proto::CameraCandidate c[4] = {};
    snprintf(c[0].name, sizeof(c[0].name), "Far");      c[0].rssi = -90;
    snprintf(c[1].name, sizeof(c[1].name), "Near");     c[1].rssi = -40;
    snprintf(c[2].name, sizeof(c[2].name), "Middle");   c[2].rssi = -60;
    snprintf(c[3].name, sizeof(c[3].name), "Farthest"); c[3].rssi = -95;
    const size_t shown = sortCandidates(c, 4);
    assert(shown == size_t(kScanRows));
    assert(!strcmp(c[0].name, "Near") && !strcmp(c[1].name, "Middle") && !strcmp(c[2].name, "Far"));
    char rssi[12];
    formatRssi(c[0].rssi, rssi, sizeof(rssi));
    assert(!strcmp(rssi, "-40 dBm"));
  }

  // Geometry: on every page the tappable rects stay inside the 232x258 widget
  // and never overlap each other or the header (y < 36) / footer (y >= 226).
  {
    const int w = 232, h = 258;
    ui::HitRect r[8];
    int n = 0;
    r[n++] = topButton(0, 2, w); r[n++] = topButton(1, 2, w);
    for (int i = 0; i < kScanRows; ++i) r[n++] = scanRow(i, w);
    for (int i = 0; i < n; ++i) {
      assert(r[i].x >= 0 && r[i].x + r[i].w <= w && r[i].y >= 36 && r[i].y + r[i].h <= 226 && r[i].h <= h);
      for (int j = i + 1; j < n; ++j) assert(!overlaps(r[i], r[j]));
    }
    n = 0;
    r[n++] = topButton(0, 2, w); r[n++] = topButton(1, 2, w);
    for (int i = 0; i < kModeCells; ++i) r[n++] = modeCell(i, w);
    for (int i = 0; i < n; ++i) {
      assert(r[i].y >= 36 && r[i].y + r[i].h <= 226);
      for (int j = i + 1; j < n; ++j) assert(!overlaps(r[i], r[j]));
    }
    n = 0;
    r[n++] = topButton(0, 2, w); r[n++] = actionRect(0, 2, w); r[n++] = actionRect(1, 2, w);
    const ui::HitRect panel = indicatorRect(w);
    for (int i = 0; i < n; ++i) {
      assert(r[i].y >= 36 && r[i].y + r[i].h <= 226 && !overlaps(r[i], panel));
      for (int j = i + 1; j < n; ++j) assert(!overlaps(r[i], r[j]));
    }
    assert(actionRect(0, 1, w).w == w - 16);
    assert(topButton(0, 1, w).w == w - 16);
  }

  // Every mode has its own short label (fits a half-width mode button) and icon.
  {
    for (size_t i = 0; i < camera_proto::CAMERA_MODE_COUNT; ++i) {
      const camera_proto::CameraModeEntry& a = camera_proto::CAMERA_MODES[i];
      assert(a.short_label && a.short_label[0] && strlen(a.short_label) <= 6);
      for (size_t j = i + 1; j < camera_proto::CAMERA_MODE_COUNT; ++j) {
        const camera_proto::CameraModeEntry& b = camera_proto::CAMERA_MODES[j];
        assert(strcmp(a.short_label, b.short_label) != 0);
        assert(modeIcon(a) != modeIcon(b));
      }
    }
  }

  // Mode paging covers every mode exactly once.
  {
    const int pages = modePages();
    assert(pages * kModeCells >= int(camera_proto::CAMERA_MODE_COUNT));
    assert((pages - 1) * kModeCells < int(camera_proto::CAMERA_MODE_COUNT));
  }

  puts("Camera view: page flow, capture indicator/buttons, scan list and geometry passed");
  return 0;
}
