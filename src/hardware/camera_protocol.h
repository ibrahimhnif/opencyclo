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

// FreeFrame Video is a real Ace Pro 2 mode ("acepro2_freeframe" is the camera's
// own internal profile name) but has no dedicated value in the proto sub-mode
// enums of SDK 2.1.5/2.2.2. Until its exact sub-mode value is captured on-device
// (tools/insta360_probe.py), the Freeframe entry selects normal video. Change
// this one constant once confirmed.
constexpr uint8_t VIDEO_SUB_FREEFRAME_PROVISIONAL = 0;  // video normal

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
