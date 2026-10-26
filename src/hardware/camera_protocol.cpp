#include "camera_protocol.h"
#include <string.h>

namespace camera_proto {

const CameraModeEntry CAMERA_MODES[] = {
    {"Video", SCOPE_VIDEO, 0},                                    // VIDEO_NORMAL
    {"Freeframe", SCOPE_VIDEO, VIDEO_SUB_FREEFRAME_PROVISIONAL},
    {"HDR", SCOPE_VIDEO, 3},                                      // VIDEO_HDR
    {"Timeshift", SCOPE_VIDEO, 4},                                // VIDEO_TIMESHIFT
    {"Super", SCOPE_VIDEO, 5},                                    // VIDEO_SUPER
    {"Loop", SCOPE_VIDEO, 6},                                     // VIDEO_LOOP_RECORDING
    {"Fpv", SCOPE_VIDEO, 7},                                      // VIDEO_FPV
    {"Movie", SCOPE_VIDEO, 8},                                    // VIDEO_MOVIE
    {"Slowmo", SCOPE_VIDEO, 9},                                   // VIDEO_SLOW_MOTION
    {"Pure", SCOPE_VIDEO, 11},                                    // VIDEO_PURE
    {"Timelapse", SCOPE_VIDEO, 2},                                // VIDEO_TIMELAPSE
    {"Dashcam", SCOPE_VIDEO, 14},                                 // VIDEO_DASHCAM
    {"Photo", SCOPE_PHOTO, 0},                                    // PHOTO_SINGLE
    {"HDR Photo", SCOPE_PHOTO, 1},                                // PHOTO_HDR
    {"Interval", SCOPE_PHOTO, 2},                                 // PHOTO_INTERVAL
    {"Burst", SCOPE_PHOTO, 3},                                    // PHOTO_BURST
    {"Starlapse", SCOPE_PHOTO, 7},                                // PHOTO_STAR_LAPSE
};
const size_t CAMERA_MODE_COUNT = sizeof(CAMERA_MODES) / sizeof(CAMERA_MODES[0]);

namespace {

// protobuf base-128 varint.
size_t putVarint(uint8_t* out, uint64_t value) {
  size_t n = 0;
  do {
    uint8_t byte = uint8_t(value & 0x7f);
    value >>= 7;
    if (value) byte |= 0x80;
    out[n++] = byte;
  } while (value);
  return n;
}

size_t writeFrame(uint8_t* out, size_t cap, const uint8_t* payload, size_t len) {
  const size_t total = len + 4;
  if (total > cap) return 0;
  out[0] = uint8_t(total);
  out[1] = uint8_t(total >> 8);
  out[2] = uint8_t(total >> 16);
  out[3] = uint8_t(total >> 24);
  if (len) memcpy(out + 4, payload, len);
  return total;
}

size_t varintSize(uint64_t value) {
  size_t n = 1;
  while (value >>= 7) ++n;
  return n;
}

// field 1, repeated OptionType, proto3-packed: 0x0a <len> <varints>.
size_t writePackedOptionTypes(uint8_t* out, size_t cap, const uint16_t* values, size_t count) {
  if (count == 0) return 0;
  size_t m = 0;
  for (size_t i = 0; i < count; ++i) m += varintSize(values[i]);
  if (m > 0x7f || m + 2 > cap) return 0;
  out[0] = 0x0a;
  out[1] = uint8_t(m);
  size_t n = 2;
  for (size_t i = 0; i < count; ++i) n += putVarint(out + n, values[i]);
  return n;
}

// field 2, an Options message holding a single OptionType=value pair:
// 0x12 <len> (<optionType<<3>) <value>.
size_t writeSingleOptionValue(uint8_t* out, size_t cap, uint16_t optionType, int32_t value) {
  const size_t inner = varintSize(uint64_t(optionType) << 3) + varintSize(uint32_t(value));
  if (inner > 0x7f || inner + 2 > cap) return 0;
  out[0] = 0x12;
  out[1] = uint8_t(inner);
  size_t n = 2;
  n += putVarint(out + n, uint64_t(optionType) << 3);  // wire type 0 (varint)
  n += putVarint(out + n, uint32_t(value));
  return n;
}

}  // namespace

size_t buildSync(uint8_t* out, size_t cap) {
  static const uint8_t kPayload[13] = {0x06, 0x00, 0x00, 's', 'y', 'N',
                                       'c',  'e',  'N',  'd', 'i', 'n', 'S'};
  return writeFrame(out, cap, kPayload, sizeof(kPayload));
}

size_t buildKeepalive(uint8_t* out, size_t cap) {
  static const uint8_t kPayload[3] = {0x05, 0x00, 0x00};
  return writeFrame(out, cap, kPayload, sizeof(kPayload));
}

size_t buildCommand(uint8_t* out, size_t cap, uint16_t cmdId, uint32_t seq,
                    const uint8_t* pb, size_t pbLen) {
  const size_t total = 12 + pbLen + 4;
  if (total > cap) return 0;
  out[0] = uint8_t(total);
  out[1] = uint8_t(total >> 8);
  out[2] = uint8_t(total >> 16);
  out[3] = uint8_t(total >> 24);
  uint8_t* h = out + 4;
  h[0] = 0x04;
  h[1] = 0x00;
  h[2] = 0x00;
  h[3] = uint8_t(cmdId & 0xff);
  h[4] = uint8_t(cmdId >> 8);
  h[5] = 0x02;
  h[6] = uint8_t(seq & 0xff);
  h[7] = uint8_t((seq >> 8) & 0xff);
  h[8] = uint8_t((seq >> 16) & 0xff);
  h[9] = 0x80;
  h[10] = 0x00;
  h[11] = 0x00;
  if (pbLen) memcpy(out + 16, pb, pbLen);
  return total;
}

size_t pbSetOption(uint8_t* out, size_t cap, uint16_t optionType, int32_t value) {
  size_t n = writePackedOptionTypes(out, cap, &optionType, 1);
  if (!n) return 0;
  const size_t valueLen = writeSingleOptionValue(out + n, cap - n, optionType, value);
  if (!valueLen) return 0;
  return n + valueLen;
}

size_t pbGetOptions(uint8_t* out, size_t cap, const uint16_t* optionTypes, size_t count) {
  return writePackedOptionTypes(out, cap, optionTypes, count);
}

bool parseFrameLength(const uint8_t* data, size_t len, uint32_t& total) {
  if (len < 4) return false;
  total = uint32_t(data[0]) | (uint32_t(data[1]) << 8) | (uint32_t(data[2]) << 16) |
          (uint32_t(data[3]) << 24);
  return total >= 4;
}

bool parseResponseHeader(const uint8_t* payload, size_t len, ResponseHeader& out) {
  if (len < 12) return false;
  out.type = payload[0];
  out.code = uint16_t(payload[3] | (uint16_t(payload[4]) << 8));
  out.seq = uint32_t(payload[6]) | (uint32_t(payload[7]) << 8) | (uint32_t(payload[8]) << 16);
  return true;
}

}  // namespace camera_proto
