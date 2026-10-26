#ifndef OPENCYCLO_HARDWARE_CAMERA_PROTOCOL_H
#define OPENCYCLO_HARDWARE_CAMERA_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

// Reverse-engineered Insta360 "OneDriver" control protocol, verified on an
// Ace Pro 2 over BLE. The official Android SDK speaks this over BLE (service
// 0000be80-..., write 0000be81-..., notify 0000be82-...) and WiFi (TCP
// 192.168.42.1:6666); OpenCyclo uses the BLE path -- see camera_control.cpp.
//
// Every packet is [u32 LE total length][payload], where the length counts
// itself. Payloads: SYNC (0x06..), KEEPALIVE (0x05..), or a PHONE_COMMAND
// (0x04 0x00 0x00 + id:u16 + 0x02 + seq:u24 + 0x80 0x00 0x00 + protobuf).
//
// Mode switching is SetOptions (cmd 7) with video_sub_mode / photo_sub_mode
// (plus main_mode first to cross the photo/video boundary). Verified live on
// the camera: setting video_sub_mode changes the mode, and reading it back via
// GetOptions reflects the change. NOTE: SetPhotographyOptions (cmd 9) does NOT
// switch modes on the Ace Pro 2 -- it rejects a lone function_mode with 400 and
// returns 200 without effect otherwise. See docs/insta360-camera-control.md.
//
// Pure logic (no Arduino/NimBLE) so it is host-tested by
// tests/test_camera_protocol.cpp.

namespace camera_proto {

// PHONE_COMMAND ids.
constexpr uint16_t CMD_TAKE_PICTURE = 3;
constexpr uint16_t CMD_START_CAPTURE = 4;
constexpr uint16_t CMD_STOP_CAPTURE = 5;
constexpr uint16_t CMD_SET_OPTIONS = 7;
constexpr uint16_t CMD_GET_OPTIONS = 8;
constexpr uint16_t CMD_GET_CAPTURE_STATUS = 15;
// Power off. EXPERIMENTAL: the official enum names 0x37 GET_SYNC_CAPTURE_MODE
// and the SDK documents closeCamera for X5+ only; not verified on the Ace Pro 2.
constexpr uint16_t CMD_POWER_OFF = 0x37;
constexpr uint16_t CMD_HEADER_TYPE_MESSAGE = 0x04;  // payload[0] of a command/response

constexpr uint16_t RESP_OK = 200;
constexpr uint16_t RESP_ERROR = 500;
constexpr uint16_t RESP_BAD_REQUEST = 400;

// insta360.messages.OptionType values we use.
constexpr uint16_t OPT_PHOTO_SUB_MODE = 40;
constexpr uint16_t OPT_VIDEO_SUB_MODE = 41;
constexpr uint16_t OPT_MAIN_MODE = 107;

// insta360.messages.MainMode values.
constexpr int MAIN_MODE_VIDEO = 0;
constexpr int MAIN_MODE_PHOTO = 1;

// Which sub-mode enum a UI entry belongs to.
enum CameraModeScope : uint8_t { SCOPE_VIDEO = 0, SCOPE_PHOTO = 1 };

// FreeFrame Video is video_sub_mode 5 on the Ace Pro 2: verified on-device by
// insta360-ace-control (the camera answers 0x2053 "08 64 10 05"). Older
// X-series cameras use 5 for "Super", which the Ace Pro 2 does not have.
constexpr uint8_t VIDEO_SUB_FREEFRAME = 5;

// insta360.messages.OptionType values read for the status indicator.
constexpr uint16_t OPT_BATTERY_STATUS = 11;
constexpr uint16_t OPT_STORAGE_STATE = 20;
extern const uint16_t STATUS_OPTIONS[];  // battery, storage, photo/video sub-mode
extern const size_t STATUS_OPTION_COUNT;

// Unsolicited notification codes (>= 0x2000) handled by applyNotification().
constexpr uint16_t NOTIFY_BATTERY_UPDATE = 0x2003;
constexpr uint16_t NOTIFY_STORAGE_UPDATE = 0x2006;
constexpr uint16_t NOTIFY_CAPTURE_STOPPED = 0x2009;
constexpr uint16_t NOTIFY_CAPTURE_STATUS = 0x2010;
constexpr uint16_t NOTIFY_SUBMODE_CHANGE = 0x2053;
constexpr uint16_t NOTIFY_FIRST = 0x2000;

// PhotoSubMode / VideoSubMode value meaning "not in this scope".
constexpr int SUB_MODE_NONE = 100;

// What the camera last told us. -1 = not reported yet this connection.
struct CameraStatus {
  int8_t battery_pct = -1;
  bool charging = false;       // BatteryStatus.power_type == ADAPTER
  int8_t card_state = -1;      // StorageState.CardState: 0 ok, 1 no card, 2 full, 3+ error
  uint64_t free_bytes = 0;
  uint64_t total_bytes = 0;
  int8_t capture_state = -1;   // CameraCaptureState: 0 idle, else capturing
  uint32_t capture_secs = 0;
  int16_t photo_sub = -1;      // SUB_MODE_NONE when in a video mode
  int16_t video_sub = -1;
};

// Fills battery/storage/sub-modes from a GetOptions (cmd 8) response body.
// Fields the response lacks are left untouched. False if no Options message.
bool parseOptionsResponse(const uint8_t* body, size_t len, CameraStatus& status);

// Fills capture state/time from a GetCurrentCaptureStatus (cmd 15) response.
bool parseCaptureStatusResponse(const uint8_t* body, size_t len, CameraStatus& status);

enum NotifyEffect : uint8_t {
  NOTIFY_IGNORED = 0,      // a code we don't track (face detect, WiFi, ...)
  NOTIFY_UPDATED,          // `status` changed
  NOTIFY_REFRESH_OPTIONS,  // battery/storage changed: send GetOptions again
};
NotifyEffect applyNotification(uint16_t code, const uint8_t* body, size_t len, CameraStatus& status);

// CAMERA_MODES label for the active sub-mode, or nullptr when unknown.
const char* cameraModeName(const CameraStatus& status);

// The one Shutter button over the direct link: STOP_CAPTURE while capturing,
// TAKE_PICTURE in a photo mode, otherwise START_CAPTURE (records in the
// camera's current video mode, also when the mode is not known yet).
uint16_t shutterCommand(const CameraStatus& status);

// Updates `status` when the camera answers 200 to `cmd`, so the UI reacts
// before the 0x2010 capture-status notification arrives.
void applyCommandOk(uint16_t cmd, CameraStatus& status);

struct CameraModeEntry {
  const char* label;
  uint8_t scope;    // SCOPE_VIDEO / SCOPE_PHOTO
  uint8_t subMode;  // VideoSubMode / PhotoSubMode value
};
extern const CameraModeEntry CAMERA_MODES[];
extern const size_t CAMERA_MODE_COUNT;

// Frame builders. Each returns bytes written (including the 4-byte length
// prefix), or 0 if `out` is too small.
size_t buildSync(uint8_t* out, size_t cap);
size_t buildKeepalive(uint8_t* out, size_t cap);
size_t buildCommand(uint8_t* out, size_t cap, uint16_t cmdId, uint32_t seq,
                    const uint8_t* pb, size_t pbLen);

// insta360.messages.SetOptions{option_types=1, value=2}: sets one OptionType to
// one varint value. e.g. pbSetOption(out, cap, OPT_VIDEO_SUB_MODE, 0).
size_t pbSetOption(uint8_t* out, size_t cap, uint16_t optionType, int32_t value);

// insta360.messages.GetOptions{option_types=1}: requests the given OptionTypes.
size_t pbGetOptions(uint8_t* out, size_t cap, const uint16_t* optionTypes, size_t count);

// Totals the 4-byte little-endian length prefix. False if <4 bytes or the value
// is malformed.
bool parseFrameLength(const uint8_t* data, size_t len, uint32_t& total);

struct ResponseHeader {
  uint8_t type;   // 0x04 for a phone-command response
  uint16_t code;  // RESP_OK / RESP_ERROR / RESP_BAD_REQUEST / notification
  uint32_t seq;   // echoes the request's sequence number
};
// Parses the 12-byte response header of a payload with its length prefix
// already stripped.
bool parseResponseHeader(const uint8_t* payload, size_t len, ResponseHeader& out);

}  // namespace camera_proto

#endif  // OPENCYCLO_HARDWARE_CAMERA_PROTOCOL_H
