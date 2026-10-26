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

  puts("Insta360 control protocol: frames, SetOptions and mode table byte-exact");
  return 0;
}
