# Hybrid UI: approved direction and firmware parity

The approved direction is B (icon + short label) for ordinary actions, C
(icon-only) for map controls. The initial button-only pass did not complete
this direction. This parity pass addresses the main ESP32 page renderer too.

## Applied to ESP32

- Shared design colors in src/ui/colors.h, quantized to RGB565. Accent,
  secondary panels, muted text and status colors are no longer separate
  approximations in ride, camera, dashboard and navigation renderers.
- All dashboard templates reserve a 44px top header: Map icon, current page
  title, Ride icon. The title is clipped, not rewritten. The former hidden
  GPS/status taps are removed. Live GPS/satellite count, recording state and
  battery occupy a separate bottom status row; page position remains visible.
- The hero dashboard has explicit Map and Ride shortcut buttons. Existing
  custom page order, widget types, calculations and layout schema are retained.
  Only the first-row/full-container pixel rectangles change for the header.
- Ride buttons align with the review's y=198/256, 48px height. Primary status
  and metric labels use readable FreeSansBold9pt rather than 8px Font0.
  Save/finish/confirmation/retry dispatch and logger acknowledgement stay.
  Tiny bottom text is secondary metadata, not the only indication of state.
- Camera has Capture (Pair / Shutter / Options) and Options (Back plus Mode /
  Screen / Wake / Off). These are UI states only. Pair and the five original
  BLE commands are unchanged, including wake setup and pairing/wake timing.
- Sensors use separated rows and 44px Forget targets; scan remains the same
  operation. Settings content is fitted below the new header; its existing
  editable/read-only fields and Power entry are preserved.
- Map keeps icon-only controls; header type and GPS/off-route/arrival guidance
  are larger. GPS/map state and recording remain independent. No map-engine,
  route-matching, SD-cache or track-recording logic changes.

## Flutter parity and preservation

- Camera ID 16 is recognized and survives layout read/edit/write. Unknown
  future widget IDs fail parsing instead of silently becoming NONE.
- Default layouts include the existing camera page; no default is pushed or
  loaded onto the physical device by this code change.
- DevicePreview uses current firmware slot positions, short widget labels,
  Map/Ride controls, management screens and Camera Capture/Options.
- Preview interactions are local presentation only, not BLE commands.
- Flutter uses bundled Arimo/Material icons; firmware uses bitmap FreeSans and
  code-native icons. The app preview is geometry-aligned, not pixel-identical.
- The original HTML A/B/C page remains an alternatives/design-reference page,
  not a screenshot of the flashed firmware.

## Verification and remaining hardware checks

Run:
- python3 tests/run_ride_tests.py /private/tmp/opencyclo-parity-preview
- python3 tests/run_navigation_tests.py
- python3 tests/run_native_tests.py
- pio test -e native
- In app/: flutter test and flutter analyze
- pio run -e esp32-s3-devkitc-1

The host harness renders 7 page/template fixtures, 9 ride states and 6 camera
states from production C++ using device bitmap fonts. It checks text bounds
and camera dispatch. Flutter tests compare all 22 slot rectangles directly
against C++, check camera round-trip preservation, unknown-ID rejection, and
exercise preview Options/Back without changing the layout model.

These checks do not certify physical LCD/touch behavior, sunlight readability
or live camera radio response. Native page preview PNGs are the closest visual
evidence; map rendering has regression tests, not a pixel-matched full-map
golden. This pass builds firmware but does not automatically reflash.
