// Byte-exact tests for the Insta360 control protocol framers. The expected
// vectors come from the public reverse engineering (hypoxic/insta360-ble-api
// PKT_SYNC/PKT_KEEPALIVE and the packet-anatomy header layout) plus the
// SetOptions frames verified live against an Ace Pro 2, not from our encoder.
#include "hardware/camera_protocol.h"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace camera_proto;

static void expect(const uint8_t* got, size_t gotLen, const uint8_t* want, size_t wantLen) {
  assert(gotLen == wantLen);
  for (size_t i = 0; i < wantLen; ++i) assert(got[i] == want[i]);
  (void)got;(void)want;
}

int main() {
  uint8_t buf[128];

  // SYNC: 4-byte length prefix (0x11 = 17), then 06 00 00 + "syNceNdinS".
  {
    static const uint8_t want[] = {0x11, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 's', 'y',
                                   'N',  'c',  'e',  'N',  'd',  'i',  'n', 'S'};
    size_t n = buildSync(buf, sizeof(buf));
    expect(buf, n, want, sizeof(want));
  }

  // KEEPALIVE: length 0x07, payload 05 00 00.
  {
    static const uint8_t want[] = {0x07, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00};
    size_t n = buildKeepalive(buf, sizeof(buf));
    expect(buf, n, want, sizeof(want));
  }

  // Command with no protobuf body: length 16, 04 00 00, cmd:u16 LE, 02,
  // seq:u24 LE, 80 00 00. SetOptions = 7.
  {
    static const uint8_t want[] = {0x10, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x07,
                                   0x00, 0x02, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00};
    size_t n = buildCommand(buf, sizeof(buf), CMD_SET_OPTIONS, 0, nullptr, 0);
    expect(buf, n, want, sizeof(want));
  }

  // Sequence number is 24-bit little-endian at payload[6..8] (payload starts
  // after the 4-byte length prefix, so that is buf[10..12]).
  {
    size_t n = buildCommand(buf, sizeof(buf), CMD_START_CAPTURE, 0x123456, nullptr, 0);
    assert(n == 16);
    assert(buf[4 + 6] == 0x56 && buf[4 + 7] == 0x34 && buf[4 + 8] == 0x12);
    assert(buf[4 + 3] == 4 && buf[4 + 4] == 0x00);  // cmd 4
  }

  // Response header: 04 00 00 | c8 00 (200) | 02 | 1d 00 00 (seq 29) | ...
  {
    static const uint8_t payload[] = {0x04, 0x00, 0x00, 0xc8, 0x00, 0x02, 0x1d, 0x00,
                                      0x00, 0x80, 0x00, 0x00};
    ResponseHeader h{};
    assert(parseResponseHeader(payload, sizeof(payload), h));
    assert(h.type == 0x04 && h.code == RESP_OK && h.seq == 0x1d);
    uint8_t err[12];
    memcpy(err, payload, sizeof(payload));
    err[3] = 0xf4; err[4] = 0x01;
    assert(parseResponseHeader(err, sizeof(err), h) && h.code == RESP_ERROR);
    err[3] = 0x90; err[4] = 0x01;
    assert(parseResponseHeader(err, sizeof(err), h) && h.code == RESP_BAD_REQUEST);
  }

  // Notification code 8208 = 0x2010 arrives where a response code would.
  {
    static const uint8_t payload[] = {0x04, 0x00, 0x00, 0x10, 0x20, 0x02, 0xff,
                                      0x8a, 0x43, 0xf4, 0x00, 0x00};
    ResponseHeader h{};
    assert(parseResponseHeader(payload, sizeof(payload), h) && h.code == 8208);
  }

  // SetOptions video_sub_mode=0 (VIDEO_NORMAL) -- the frame verified live.
  // option_types=[41] packed, value{41:0} -> 0a 01 29 12 03 c8 02 00
  {
    static const uint8_t want[] = {0x0a, 0x01, 0x29, 0x12, 0x03, 0xc8, 0x02, 0x00};
    size_t n = pbSetOption(buf, sizeof(buf), OPT_VIDEO_SUB_MODE, 0);
    expect(buf, n, want, sizeof(want));
  }

  // SetOptions main_mode=1 (PHOTO): 107<<3 = 856 -> varint d8 06.
  {
    static const uint8_t want[] = {0x0a, 0x01, 0x6b, 0x12, 0x03, 0xd8, 0x06, 0x01};
    size_t n = pbSetOption(buf, sizeof(buf), OPT_MAIN_MODE, MAIN_MODE_PHOTO);
    expect(buf, n, want, sizeof(want));
  }

  // GetOptions with option_types [40,41]: packed 0a 02 28 29.
  {
    const uint16_t types[] = {OPT_PHOTO_SUB_MODE, OPT_VIDEO_SUB_MODE};
    static const uint8_t want[] = {0x0a, 0x02, 0x28, 0x29};
    size_t n = pbGetOptions(buf, sizeof(buf), types, 2);
    expect(buf, n, want, sizeof(want));
  }

  // Frame length round-trip.
  {
    size_t pbLen = pbSetOption(buf, sizeof(buf), OPT_VIDEO_SUB_MODE, 5);
    uint8_t frame[128];
    size_t n = buildCommand(frame, sizeof(frame), CMD_SET_OPTIONS, 5, buf, pbLen);
    uint32_t total = 0;
    assert(parseFrameLength(frame, n, total) && total == n);
    assert(frame[4] == 0x04 && frame[4 + 9] == 0x80);
  }

  // Length prefix rejected below 4 bytes; overflow returns 0.
  {
    uint32_t total = 0;
    assert(!parseFrameLength(buf, 3, total));
    uint8_t tiny[8];
    assert(buildCommand(tiny, sizeof(tiny), CMD_START_CAPTURE, 0, nullptr, 0) == 0);
    assert(buildSync(tiny, 4) == 0);
  }

  // The picker exposes Freeframe, spans video and photo scopes, and every entry
  // carries a sub-mode value.
  {
    assert(CAMERA_MODE_COUNT > 0);
    bool sawFreeframe = false, sawPhoto = false, sawVideo = false;
    for (size_t i = 0; i < CAMERA_MODE_COUNT; ++i) {
      assert(CAMERA_MODES[i].label && CAMERA_MODES[i].label[0]);
      if (!strcmp(CAMERA_MODES[i].label, "Freeframe")) sawFreeframe = true;
      if (CAMERA_MODES[i].scope == SCOPE_PHOTO) sawPhoto = true;
      if (CAMERA_MODES[i].scope == SCOPE_VIDEO) sawVideo = true;
    }
    assert(sawFreeframe && sawPhoto && sawVideo);
    assert(CAMERA_MODES[0].scope == SCOPE_VIDEO && CAMERA_MODES[0].subMode == 0);
  }

  // FreeFrame is video_sub_mode 5 on the Ace Pro 2 (verified by
  // insta360-ace-control); 5 is not also offered as "Super".
  {
    int freeframes = 0;
    for (size_t i = 0; i < CAMERA_MODE_COUNT; ++i) {
      if (CAMERA_MODES[i].scope == SCOPE_VIDEO && CAMERA_MODES[i].subMode == VIDEO_SUB_FREEFRAME) {
        assert(!strcmp(CAMERA_MODES[i].label, "Freeframe"));
        ++freeframes;
      }
    }
    assert(VIDEO_SUB_FREEFRAME == 5 && freeframes == 1);
  }

  // GetOptions for the status indicator: packed [11,20,40,41] -> 0a 04 0b 14 28 29.
  {
    static const uint8_t want[] = {0x0a, 0x04, 0x0b, 0x14, 0x28, 0x29};
    size_t n = pbGetOptions(buf, sizeof(buf), STATUS_OPTIONS, STATUS_OPTION_COUNT);
    expect(buf, n, want, sizeof(want));
  }

  // A fresh status knows nothing.
  {
    CameraStatus s;
    assert(s.battery_pct == -1 && s.card_state == -1 && s.capture_state == -1);
    assert(s.photo_sub == -1 && s.video_sub == -1 && !s.charging);
  }

  // Real Ace Pro 2 GetOptions response body (docs/ble-protocol.md 6.1 in
  // insta360-ace-control): charging at 54%, card normal, ~12.4 GiB free of
  // ~119 GiB, video normal.
  {
    static const uint8_t body[] = {
        0x08, 0x0b, 0x08, 0x14, 0x08, 0x0f, 0x08, 0x1e, 0x08, 0x28, 0x08, 0x29, 0x08, 0x30,
        0x12, 0x51,
        0x5a, 0x06, 0x08, 0x01, 0x10, 0x36, 0x20, 0x00,
        0x7a, 0x0e, 'I', 'B', 'G', 'L', 'A', '2', '5', '0', '1', 'Y', 'E', 'Y', 'D', '9',
        0xa2, 0x01, 0x11, 0x08, 0x00, 0x10, 0x80, 0x80, 0xd0, 0xd6, 0x31,
        0x18, 0x80, 0x80, 0x80, 0x9b, 0xdc, 0x03, 0x20, 0x00,
        0xf2, 0x01, 0x07, 'v', '2', '.', '0', '.', '5', '1',
        0xc0, 0x02, 0x64,
        0xc8, 0x02, 0x00,
        0x82, 0x03, 0x12, 'I', 'n', 's', 't', 'a', '3', '6', '0', ' ', 'A', 'c', 'e', ' ',
        'P', 'r', 'o', ' ', '2'};
    CameraStatus s;
    assert(parseOptionsResponse(body, sizeof(body), s));
    assert(s.battery_pct == 54 && s.charging);
    assert(s.card_state == 0 && s.free_bytes == 13335003136ULL && s.total_bytes == 127831900160ULL);
    assert(s.photo_sub == 100 && s.video_sub == 0);
    assert(s.capture_state == -1);  // not part of GetOptions
  }

  // A response without the Options field, or a truncated one, changes nothing.
  {
    CameraStatus s;
    static const uint8_t noOptions[] = {0x08, 0x0b};
    assert(!parseOptionsResponse(noOptions, sizeof(noOptions), s));
    static const uint8_t truncated[] = {0x12, 0x10, 0x5a, 0x06, 0x08};
    assert(!parseOptionsResponse(truncated, sizeof(truncated), s));
    assert(s.battery_pct == -1 && s.card_state == -1);
  }

  // Battery on battery power: proto3 omits power_type=0.
  {
    static const uint8_t body[] = {0x12, 0x04, 0x5a, 0x02, 0x10, 0x07};
    CameraStatus s;
    s.charging = true;
    assert(parseOptionsResponse(body, sizeof(body), s));
    assert(s.battery_pct == 7 && !s.charging && s.card_state == -1);
  }

  // 0x2010 CURRENT_CAPTURE_STATUS: recording, 15 s.
  {
    static const uint8_t body[] = {0x08, 0x01, 0x10, 0x0f, 0x38, 0x02};
    CameraStatus s;
    assert(applyNotification(NOTIFY_CAPTURE_STATUS, body, sizeof(body), s) == NOTIFY_UPDATED);
    assert(s.capture_state == 1 && s.capture_secs == 15);
  }

  // GetCurrentCaptureStatus (cmd 0x0F) response wraps the status in field 1.
  {
    static const uint8_t body[] = {0x0a, 0x06, 0x08, 0x00, 0x10, 0x0f, 0x50, 0x00};
    CameraStatus s;
    s.capture_state = 1;
    assert(parseCaptureStatusResponse(body, sizeof(body), s));
    assert(s.capture_state == 0 && s.capture_secs == 15);
  }

  // 0x2009 CAPTURE_STOPPED clears recording.
  {
    CameraStatus s;
    s.capture_state = 1;
    s.capture_secs = 40;
    assert(applyNotification(NOTIFY_CAPTURE_STOPPED, nullptr, 0, s) == NOTIFY_UPDATED);
    assert(s.capture_state == 0 && s.capture_secs == 0);
  }

  // 0x2053 CAM_SUBMODE_CHANGE: field 1 photo, field 2 video; absent = 0.
  {
    CameraStatus s;
    static const uint8_t freeframe[] = {0x08, 0x64, 0x10, 0x05};
    assert(applyNotification(NOTIFY_SUBMODE_CHANGE, freeframe, sizeof(freeframe), s) == NOTIFY_UPDATED);
    assert(s.photo_sub == 100 && s.video_sub == 5);
    static const uint8_t photo[] = {0x08, 0x00};
    assert(applyNotification(NOTIFY_SUBMODE_CHANGE, photo, sizeof(photo), s) == NOTIFY_UPDATED);
    assert(s.photo_sub == 0 && s.video_sub == 0);
  }

  // Storage and battery updates ask for a fresh GetOptions; noise is ignored.
  {
    CameraStatus s;
    static const uint8_t storage[] = {0x08, 0x00, 0x10, 0x00};
    assert(applyNotification(NOTIFY_STORAGE_UPDATE, storage, sizeof(storage), s) == NOTIFY_REFRESH_OPTIONS);
    assert(applyNotification(NOTIFY_BATTERY_UPDATE, nullptr, 0, s) == NOTIFY_REFRESH_OPTIONS);
    static const uint8_t face[] = {0x08, 0x00};
    assert(applyNotification(0x203a, face, sizeof(face), s) == NOTIFY_IGNORED);
    assert(s.battery_pct == -1 && s.capture_state == -1);
  }

  // Mode names follow the active sub-mode (photo wins unless it is NONE=100).
  {
    CameraStatus s;
    assert(cameraModeName(s) == nullptr);
    s.photo_sub = 100;
    s.video_sub = 5;
    assert(!strcmp(cameraModeName(s), "Freeframe"));
    s.photo_sub = 0;
    assert(!strcmp(cameraModeName(s), "Photo"));
    s.photo_sub = 100;
    s.video_sub = 42;
    assert(cameraModeName(s) == nullptr);
  }

  puts("Insta360 control protocol: frames, SetOptions, status parsers and notifications byte-exact");
  return 0;
}
