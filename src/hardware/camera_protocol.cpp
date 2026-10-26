#include "camera_protocol.h"
#include <string.h>

namespace camera_proto {

const CameraModeEntry CAMERA_MODES[] = {
    {"Video", "Video", SCOPE_VIDEO, 0},                         // VIDEO_NORMAL
    {"Freeframe", "Free", SCOPE_VIDEO, VIDEO_SUB_FREEFRAME},
    {"HDR", "HDR", SCOPE_VIDEO, 3},                             // VIDEO_HDR
    {"Timeshift", "TShift", SCOPE_VIDEO, 4},                    // VIDEO_TIMESHIFT
    {"Loop", "Loop", SCOPE_VIDEO, 6},                           // VIDEO_LOOP_RECORDING
    {"Fpv", "FPV", SCOPE_VIDEO, 7},                             // VIDEO_FPV
    {"Movie", "Movie", SCOPE_VIDEO, 8},                         // VIDEO_MOVIE
    {"Slowmo", "Slow", SCOPE_VIDEO, 9},                         // VIDEO_SLOW_MOTION
    {"Pure", "Pure", SCOPE_VIDEO, 11},                          // VIDEO_PURE
    {"Timelapse", "Lapse", SCOPE_VIDEO, 2},                     // VIDEO_TIMELAPSE
    {"Dashcam", "Dash", SCOPE_VIDEO, 14},                       // VIDEO_DASHCAM
    {"Photo", "Photo", SCOPE_PHOTO, 0},                         // PHOTO_SINGLE
    {"HDR Photo", "P-HDR", SCOPE_PHOTO, 1},                     // PHOTO_HDR
    {"Interval", "Intvl", SCOPE_PHOTO, 2},                      // PHOTO_INTERVAL
    {"Burst", "Burst", SCOPE_PHOTO, 3},                         // PHOTO_BURST
    {"Starlapse", "Stars", SCOPE_PHOTO, 7},                     // PHOTO_STAR_LAPSE
};
const size_t CAMERA_MODE_COUNT = sizeof(CAMERA_MODES) / sizeof(CAMERA_MODES[0]);

const uint16_t STATUS_OPTIONS[] = {OPT_BATTERY_STATUS, OPT_STORAGE_STATE, OPT_PHOTO_SUB_MODE,
                                   OPT_VIDEO_SUB_MODE};
const size_t STATUS_OPTION_COUNT = sizeof(STATUS_OPTIONS) / sizeof(STATUS_OPTIONS[0]);

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

namespace {

bool readVarint(const uint8_t*& p, const uint8_t* end, uint64_t& value) {
  value = 0;
  for (int shift = 0; shift < 64 && p < end; shift += 7) {
    const uint8_t byte = *p++;
    value |= uint64_t(byte & 0x7f) << shift;
    if (!(byte & 0x80)) return true;
  }
  return false;
}

// One protobuf field. Varint fields set `value`; length-delimited fields set
// `data`/`size`. Fixed-width fields are skipped over but reported.
struct PbField {
  uint32_t number;
  uint8_t wire;
  uint64_t value;
  const uint8_t* data;
  size_t size;
};

// Advances `p` past one field. False on a malformed or truncated field.
bool nextField(const uint8_t*& p, const uint8_t* end, PbField& f) {
  uint64_t key = 0;
  if (!readVarint(p, end, key)) return false;
  f.number = uint32_t(key >> 3);
  f.wire = uint8_t(key & 7);
  f.value = 0;
  f.data = nullptr;
  f.size = 0;
  switch (f.wire) {
    case 0:
      return readVarint(p, end, f.value);
    case 1:
      if (end - p < 8) return false;
      p += 8;
      return true;
    case 2: {
      uint64_t n = 0;
      if (!readVarint(p, end, n) || n > uint64_t(end - p)) return false;
      f.data = p;
      f.size = size_t(n);
      p += n;
      return true;
    }
    case 5:
      if (end - p < 4) return false;
      p += 4;
      return true;
    default:
      return false;
  }
}

// Finds the first length-delimited field `number`. The whole message must parse.
bool findMessage(const uint8_t* body, size_t len, uint32_t number, const uint8_t*& data, size_t& size) {
  const uint8_t* p = body;
  const uint8_t* end = body + len;
  bool found = false;
  PbField f;
  while (p < end) {
    if (!nextField(p, end, f)) return false;
    if (!found && f.wire == 2 && f.number == number) {
      data = f.data;
      size = f.size;
      found = true;
    }
  }
  return found;
}

// Reads proto3 varint fields 1..3 of a small message; absent fields are 0.
bool readVarints3(const uint8_t* body, size_t len, uint64_t out[3]) {
  out[0] = out[1] = out[2] = 0;
  const uint8_t* p = body;
  const uint8_t* end = body + len;
  PbField f;
  while (p < end) {
    if (!nextField(p, end, f)) return false;
    if (f.wire == 0 && f.number >= 1 && f.number <= 3) out[f.number - 1] = f.value;
  }
  return true;
}

int8_t clampPct(uint64_t v) { return int8_t(v > 100 ? 100 : v); }

}  // namespace

bool parseOptionsResponse(const uint8_t* body, size_t len, CameraStatus& status) {
  const uint8_t* options = nullptr;
  size_t optionsLen = 0;
  if (!body || !findMessage(body, len, 2, options, optionsLen)) return false;
  // Parse into a copy so a malformed message leaves `status` untouched.
  CameraStatus next = status;
  const uint8_t* p = options;
  const uint8_t* end = options + optionsLen;
  PbField f;
  while (p < end) {
    if (!nextField(p, end, f)) return false;
    uint64_t v[3];
    if (f.number == OPT_BATTERY_STATUS && f.wire == 2) {
      if (!readVarints3(f.data, f.size, v)) return false;
      next.charging = v[0] == 1;
      next.battery_pct = clampPct(v[1]);
    } else if (f.number == OPT_STORAGE_STATE && f.wire == 2) {
      if (!readVarints3(f.data, f.size, v)) return false;
      next.card_state = int8_t(v[0] > 5 ? 5 : v[0]);
      next.free_bytes = v[1];
      next.total_bytes = v[2];
    } else if (f.number == OPT_PHOTO_SUB_MODE && f.wire == 0) {
      next.photo_sub = int16_t(f.value);
    } else if (f.number == OPT_VIDEO_SUB_MODE && f.wire == 0) {
      next.video_sub = int16_t(f.value);
    }
  }
  status = next;
  return true;
}

static bool applyCaptureStatus(const uint8_t* body, size_t len, CameraStatus& status) {
  uint64_t v[3];
  if (!readVarints3(body, len, v)) return false;
  status.capture_state = int8_t(v[0] > 127 ? 127 : v[0]);
  status.capture_secs = uint32_t(v[1]);
  return true;
}

bool parseCaptureStatusResponse(const uint8_t* body, size_t len, CameraStatus& status) {
  const uint8_t* inner = nullptr;
  size_t innerLen = 0;
  if (!body || !findMessage(body, len, 1, inner, innerLen)) return false;
  return applyCaptureStatus(inner, innerLen, status);
}

NotifyEffect applyNotification(uint16_t code, const uint8_t* body, size_t len, CameraStatus& status) {
  if (!body) len = 0;
  switch (code) {
    case NOTIFY_CAPTURE_STATUS:
      return applyCaptureStatus(body, len, status) ? NOTIFY_UPDATED : NOTIFY_IGNORED;
    case NOTIFY_CAPTURE_STOPPED:
      status.capture_state = 0;
      status.capture_secs = 0;
      return NOTIFY_UPDATED;
    case NOTIFY_SUBMODE_CHANGE: {
      uint64_t v[3];
      if (!readVarints3(body, len, v)) return NOTIFY_IGNORED;
      status.photo_sub = int16_t(v[0]);
      status.video_sub = int16_t(v[1]);
      return NOTIFY_UPDATED;
    }
    case NOTIFY_STORAGE_UPDATE:
    case NOTIFY_BATTERY_UPDATE:
      return NOTIFY_REFRESH_OPTIONS;
    default:
      return NOTIFY_IGNORED;
  }
}

const char* cameraModeName(const CameraStatus& status) {
  uint8_t scope;
  int sub;
  if (status.photo_sub >= 0 && status.photo_sub != SUB_MODE_NONE) {
    scope = SCOPE_PHOTO;
    sub = status.photo_sub;
  } else if (status.video_sub >= 0 && status.video_sub != SUB_MODE_NONE) {
    scope = SCOPE_VIDEO;
    sub = status.video_sub;
  } else {
    return nullptr;
  }
  for (size_t i = 0; i < CAMERA_MODE_COUNT; ++i) {
    if (CAMERA_MODES[i].scope == scope && CAMERA_MODES[i].subMode == sub) return CAMERA_MODES[i].label;
  }
  return nullptr;
}

// TakePictureResponse{image=1: Photo{uri=1}} and StopCaptureResp{video=1:
// Video{uri=1}} share a shape: field 1 message holding a field 1 string.
static bool readSavedFile(const uint8_t* body, size_t len, char* out, size_t cap) {
  const uint8_t* media = nullptr;
  size_t mediaLen = 0;
  if (!body || !findMessage(body, len, 1, media, mediaLen)) return false;
  const uint8_t* uri = nullptr;
  size_t uriLen = 0;
  if (!findMessage(media, mediaLen, 1, uri, uriLen) || uriLen == 0) return false;
  size_t start = 0;
  for (size_t i = 0; i < uriLen; ++i)
    if (uri[i] == '/') start = i + 1;
  size_t n = uriLen - start;
  if (n == 0) return false;
  if (n > cap - 1) n = cap - 1;
  memcpy(out, uri + start, n);
  out[n] = '\0';
  return true;
}

void applyCommandSent(uint16_t cmd, CameraStatus& status) {
  if (cmd == CMD_TAKE_PICTURE) status.photo_state = PHOTO_PENDING;
  if (cmd == CMD_START_CAPTURE) status.capture_request = CAPTURE_STARTING;
  if (cmd == CMD_STOP_CAPTURE) status.capture_request = CAPTURE_STOPPING;
}

void applyCommandOk(uint16_t cmd, const uint8_t* body, size_t len, CameraStatus& status) {
  switch (cmd) {
    case CMD_START_CAPTURE:
      status.capture_state = 1;
      status.capture_secs = 0;
      status.capture_request = CAPTURE_NONE;
      status.last_file[0] = '\0';
      break;
    case CMD_STOP_CAPTURE:
      status.capture_state = 0;
      status.capture_secs = 0;
      status.capture_request = CAPTURE_NONE;
      readSavedFile(body, len, status.last_file, sizeof(status.last_file));
      break;
    case CMD_TAKE_PICTURE:
      status.photo_state = PHOTO_SAVED;
      if (!readSavedFile(body, len, status.last_file, sizeof(status.last_file))) status.last_file[0] = '\0';
      break;
    default:
      break;
  }
}

void applyCommandFailed(uint16_t cmd, CameraStatus& status) {
  if (cmd == CMD_TAKE_PICTURE) status.photo_state = PHOTO_FAILED;
  if (cmd == CMD_START_CAPTURE || cmd == CMD_STOP_CAPTURE) status.capture_request = CAPTURE_NONE;
}

uint32_t liveCaptureSecs(const CameraStatus& status, uint32_t reportMs, uint32_t nowMs) {
  if (status.capture_state <= 0) return status.capture_secs;
  return status.capture_secs + (nowMs - reportMs) / 1000;
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
