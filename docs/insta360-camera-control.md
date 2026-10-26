# Insta360 direct camera control

OpenCyclo talks to an Insta360 Ace Pro 2 over two independent BLE channels:

1. **Emulated "GPS Remote"** (`src/hardware/ble_camera_remote.cpp`) — the camera
   connects to *us* and treats the device as its physical remote. Shutter,
   screen toggle, power off and the wake beacon. Confirmed on real hardware.
2. **Direct control** (`src/hardware/camera_control.cpp`) — *we* connect to the
   camera's own control service as a BLE central and speak its protocol. This
   selects capture modes the physical remote's Mode button cannot reach.

This document covers channel 2. The protocol is a community reverse
engineering of the official Android SDK (`com.arashivision.sdk:sdk-camera`),
verified live against an Ace Pro 2.

## Transports

The SDK supports BLE, WiFi, WiFi Aware and USB. OpenCyclo uses **BLE** for
control — no WiFi join, lower power. Both transports carry the *same* framing.

| Transport | Address | Notes |
| --- | --- | --- |
| BLE | service `0000be80-0000-1000-8000-00805f9b34fb`, write `…be81`, notify `…be82` | What the firmware uses. |
| WiFi | TCP `192.168.42.1:6666` | Camera AP; default password `88888888` on older models, per-device on newer ones (delivered over BLE). |

The `be80` control service is a *different role* from the `ce80` service the
firmware advertises as the GPS Remote: there the camera is the central, here we
are. Both work at once — verified against a camera simultaneously connected to
the emulated remote.

## Frame format

Every packet, both directions, is `[u32 LE total length][payload]`. The length
counts itself (payload length + 4).

```
SYNC       06 00 00  "syNceNdinS"                      (sent once on connect)
KEEPALIVE  05 00 00                                    (every ~2 s, both sides)

COMMAND    04 00 00  <cmd:u16 LE>  02  <seq:u24 LE>  80 00 00  <protobuf>
RESPONSE   04 00 00  <code:u16 LE> 02  <seq:u24 LE>  80  <b2>      <protobuf>
```

`code` is `200` (OK), `400` (bad request), `500` (error) or a notification code
(e.g. `8208` = capture status). Responses echo the request's 24-bit sequence
number.

### Command ids

| Id | Name | Id | Name |
| --- | --- | --- | --- |
| 1 | StartLiveStream | 8 | GetOptions |
| 2 | StopLiveStream | 9 | SetPhotographyOptions |
| 3 | TakePicture | 10 | GetPhotographyOptions |
| 4 | StartCapture | 11 | GetFileExtra |
| 5 | StopCapture | 12 | DeleteFiles |
| 6 | CancelCapture | 13 | GetFileList |
| 7 | **SetOptions** | 15 | GetCurrentCaptureStatus |

## Selecting a mode

Mode selection is **`SetOptions` (cmd 7)** with the sub-mode OptionType. This
was established empirically on an Ace Pro 2:

- `SetPhotographyOptions` (cmd 9) does **not** switch modes. A lone
  `function_mode` is rejected with `400`; adding `option_types` returns `200`
  but the camera does not change (a `GetOptions` read-back is unchanged).
- `SetOptions` with `video_sub_mode` **does** switch and the change reads back.

Relevant `insta360.messages.OptionType` values:

| OptionType | Value | Meaning |
| --- | --- | --- |
| `PHOTO_SUB_MODE` | 40 | Photo sub-mode |
| `VIDEO_SUB_MODE` | 41 | Video sub-mode |
| `MAIN_MODE` | 107 | 0 = video, 1 = photo |

The request is `SetOptions{option_types=[t], value{t=v}}`, e.g. video normal
(`0a 01 29` = option_types `[41]`, then `12 03 c8 02 00` = value `{41:0}`):

```
04 00 00 07 00 02 <seq> 80 00 00  0a 01 29 12 03 c8 02 00
```

The firmware builds this with `camera_proto::pbSetOption` / `buildCommand`; to
cross the photo/video boundary it sends `main_mode` first, then the sub-mode.

### Sub-mode values

`VIDEO_SUB_MODE` follows `insta360.messages.VideoSubMode`; `PHOTO_SUB_MODE`
follows `PhotoSubMode`. The picker's entries (`camera_proto::CAMERA_MODES`):

| Label | Scope | Value | | Label | Scope | Value |
| --- | --- | --- | --- | --- | --- | --- |
| Video | video | 0 | | Pure | video | 11 |
| Freeframe | video | *provisional 0* | | Timelapse | video | 2 |
| HDR | video | 3 | | Dashcam | video | 14 |
| Timeshift | video | 4 | | Photo | photo | 0 |
| Super | video | 5 | | HDR Photo | photo | 1 |
| Loop | video | 6 | | Interval | photo | 2 |
| Fpv | video | 7 | | Burst | photo | 3 |
| Movie | video | 8 | | Starlapse | photo | 7 |
| Slowmo | video | 9 | | | | |

Verified live: `video_sub_mode` 0/3/4/5 all switch and read back correctly.

## FreeFrame — the open item

FreeFrame Video is a real Ace Pro 2 mode ("acepro2_freeframe" is the camera's
own internal profile name) but has **no value in the proto sub-mode enums** of
SDK 2.1.5 or 2.2.2, and the SDK's `FunctionMode` Kotlin enum cannot select it
either. Until its exact value is captured, the `Freeframe` entry selects normal
video (`camera_proto::VIDEO_SUB_FREEFRAME_PROVISIONAL`).

To find it (Phase A):

1. Run `python3 tools/insta360_probe.py --sweep` (BLE) while watching the camera
   screen. The sweep sets `video_sub_mode` 0..24 and reads each back; a value the
   camera accepts but that isn't in the table above is a candidate.
2. Or capture the official app switching to FreeFrame (BLE HCI snoop or the WiFi
   socket) and decode the `SetOptions` body.
3. Once known, set `VIDEO_SUB_FREEFRAME_PROVISIONAL` in
   `src/hardware/camera_protocol.h` to the confirmed value.

## Firmware integration

- `src/hardware/camera_protocol.{h,cpp}` — pure framing, the `SetOptions` /
  `GetOptions` protobuf encoders and the mode table. No Arduino/NimBLE, unit
  tested on the host.
- `src/hardware/camera_control.{h,cpp}` — the BLE central: pair (scan for
  `be80`), connect, subscribe `be82`, SYNC + KEEPALIVE, ~2 s keepalive,
  reconnect every 12 s, and `setCameraMode()`. Runs inside `bleTaskLoop()`; the
  blocking connect happens in `tickCameraControl()`, never in a scan callback.
- `src/hardware/ble_task.cpp` — calls `initCameraControl()` at setup,
  `tickCameraControl()` each loop, and feeds scan results to
  `cameraControlOnAdvertised()` (service UUID or camera name match).
- `src/ui/engine/widget_registry.cpp` — the Camera widget's Options → Modes
  page is the paged picker; Pair opens both channels.
- `src/storage/settings.*` — `camera_mac` / `camera_addr_type` persist the
  paired camera.

On the device: open the Camera widget → **Pair** (camera on and advertising),
then **Options → Modes** and pick a mode. The serial log prints `[CAM] …` lines.

## Verification

```bash
python3 tests/run_native_tests.py   # byte-exact frames, SetOptions and mode table
python3 tests/run_ride_tests.py     # camera widget: dispatches + mode picker
~/.platformio/penv/bin/pio run      # firmware build
```

On hardware: the ESP32 connects to the camera as central (header reads "Camera
connected"), and selecting a mode changes the camera's on-screen mode.
Shutter/screen/wake still work through the GPS Remote channel.

## References

- `hypoxic/insta360-ble-api` — Python WiFi + BLE client, framing and headers.
- `Daiki-Iijima/insta360-go-ultra-sdk/docs/protocol.md` — port 6666 protocol.
- Insta360 "Packet Anatomy" (insta360.whitebox.aero) — packet layout.
- `Insta360Develop/Android-SDK` and the `sdk-camera` sources — the
  `insta360.messages` protobuf definitions (Wire-generated Kotlin).
