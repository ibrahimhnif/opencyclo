# Offline maps and GPX navigation

OpenCyclo has two independent map modes. Free ride follows the current GPS fix
and shows the road network; while recording it overlays a recent green trail.
GPX navigation adds a cyan route, remaining distance, upcoming cue/turn arrow,
an instruction list and an off-route warning. There is no destination search or
automatic rerouting. Recording and navigation selection are independent.

## Device controls

- Tap the title/GPS (left) portion of the dashboard status bar to open the map.
- Tap `< ride` to return to the dashboard; tap the route title to select a saved
  route or free ride. Up to 64 saved routes are listed, four per page.
- Drag the map to pan continuously while your finger is down (6-pixel dead zone).
  Use `-`, `+` and `follow` along the bottom. Zoom is 13–17;
  north remains up. Pan disables following until `follow` is tapped.
- `cues` opens the instruction list. Distance there is measured from route start.
- Missing map data and invalid GPS are explicitly indicated. Without a fix,
  progression is frozen and the live position marker is hidden.
- Before the first GPS fix, the map previews real SD data around Monas/Jakarta
  (or the loaded route's start), labelled `preview - waiting for GPS`. Zoom and
  pan work in preview; the first real fix automatically recenters and follows.
  Preview coordinates never enter telemetry or ride recordings.
- Long-press BOOT still opens the power menu.

## Finish and save a ride

Open **ride controls** using the dashboard's top-right recording/battery status,
the large action button on supported layouts, or **ride** at the top right of
the map. Choose **FINISH RIDE**, then **SAVE GPX & FINISH**. Cancel leaves the ride
unchanged. The logger closes the GPX in the background; wait for **GPX saved** and
the filename under `/rides/` before removing the card. Save failures show **RETRY
SAVE GPX** and keep the file open for retry, without duplicating partially written
XML. When there is no file and SD/logging is unavailable, the UI explicitly says
there is no open GPX to save rather than claiming success.

Finishing freezes the ride totals and suppresses auto-start until **START NEW
RIDE** is pressed. Starting a new ride resets totals and uses a distinct filename.
Manual pause also stays paused until resumed. Free-ride/GPX navigation remains
independent of recording. Save controls are on the device; no new Flutter screen
or BLE command is required for this change.

Trackpoints require a valid GPS position. Timestamps come from GPS UTC; without
a valid clock filenames use uptime plus collision suffixes and points omit the
time element. A ride ended before any valid GPS points can produce an empty GPX.

The recent trail is limited to 512 points in RAM; full ride recording remains in
`/rides/*.gpx`. GPS-based heading is not a magnetic compass. At a crossing the
matcher searches within 100 m behind / 500 m ahead of its last match to avoid
jumping to the end of a loop. Reselect the route to attach somewhere far away.
Off-route threshold is 50 m; arrival is within 20 m of the remaining route.

## Flutter workflow

Open **routes**, import a GPX, inspect the geometry preview and cues, then **sync
route to device**. Once saved, **start GPX navigation** selects it on the device.
The GPX is parsed on a background isolate. Sync acknowledges each offset and
checks a CRC32 and exact byte count; 100% is reported only after the device says
the validated file has been saved. Cancellation, disconnect and a 30-second
inactivity timeout release the transfer. An incomplete `.part` is ignored and
overwritten by the next transfer. Re-sync of identical bytes is idempotent.

The app checks that the map bounding box and column files cover every route
point. This detects absent/partial map installation; it does not verify road
completeness or that each road is currently passable. A GPX can be navigated
without a basemap, with a visible missing-map warning.

Supported input: one continuous `trkseg` or one `rte`, up to 8 MB / 12,000 distinct
points / 256 cues. Multi-segment tracks are rejected instead of joined across
gaps. Latitude range is -85 to 85; antimeridian crossing is unsupported.
GPX point `desc` fields supply explicit cue text. Otherwise geometric bends are
computed over approximately 25 m on each side and spaced at least 60 m apart.
These are labelled **route bends**, not verified intersection instructions.
Plain GPX does not supply street names, junction topology or turn restrictions.
Text on the device is ASCII, at most 47 bytes for a route name and 42 per cue.

## Build the Indonesia map

Download the Indonesia extract from
[Geofabrik](https://download.geofabrik.de/asia/indonesia.html). The extract also
includes Timor-Leste. This pipeline reads `.osm.pbf` directly; do not expand the
whole country to XML. It keeps node locations and intermediate geometry on disk.

```sh
python3 -m venv .venv-map
.venv-map/bin/pip install -r tools/map_requirements.txt
.venv-map/bin/python tools/build_indonesia_map.py indonesia-latest.osm.pbf artifacts/indonesia
python3 tools/verify_map_pack.py artifacts/indonesia/maps
```

The default bounding box is 94,-12,142,7. To build another regional extract, pass
`--bbox west,south,east,north`. The input must cover the requested region. Use a
fresh destination for every build; `coverage.bin` and the final manifest mark a
completed pack. Allow several GB of temporary disk space in addition to input
and output. The actual output size is printed at completion.

With the device powered off, copy the resulting **maps directory** to a FAT32
microSD root, preserving its structure. Do not format a card containing rides.
The card should contain:

```text
/maps/manifest.json
/maps/coverage.bin
/maps/ATTRIBUTION.txt
/maps/14/<column>.ocp
/routes/<checksum>.ocr
/rides/<ride>.gpx
```

The map stores road and path geometry, including curves and intersections; it
does not include satellite imagery, terrain, building fills, street labels or
a routable road graph. Major roads are brighter grey, local roads darker grey,
paths/cycleways teal. Road visibility is not a claim of bicycle access.

Data is from OpenStreetMap under ODbL. Keep the attribution and license files
when distributing packs; the device also displays attribution. This builder
does not fetch the public OSM tile service, whose policy prohibits offline bulk
downloads: https://operations.osmfoundation.org/policies/tiles/ .

## Binary contracts

All integers are little endian. `OCR1`: 60-byte header (magic, uint32 point count,
uint32 cue count, 48-byte null-terminated name), then points (int32 latitude and
longitude in degrees × 1e7), then 48-byte cues (uint32 point index, int8 direction,
43-byte null-terminated text). Directions -1/+1 denote left/right, 0 unspecified.
CRC32 is the IEEE polynomial 0xedb88320 with initial/final XOR 0xffffffff.

BLE service 1900 adds 1904 (control write/read) and 1905 (data write). Control:
1 + size + CRC begins; 2 commits; 3 cancels; 4 + CRC selects; 5 enters free ride;
6 + CRC checks map coverage. Data: uint32 offset + bytes. Replies: `OK <offset>`,
`SAVED <path>`, `MAP covered/missing/unknown`, or `ERR <reason>`. Writes use
responses; packet size respects negotiated MTU. Route files are staged on SD
before rename and read back for checksum verification. OTA and shutdown are
excluded during route transfer.

`OCP1`: 8-byte header (magic + entry count), sorted 12-byte entries (tile y,
absolute byte offset, segment count), followed by segments. Each segment is
9 bytes: four uint16 tile-local coordinates in 1/256 pixel units and one road
class byte. Column x is the filename. Projection is Web Mercator at zoom 14.
Each tile is limited to 20,000 segments. Empty tiles have no entry; empty columns
still have a header. `OCB1` coverage: magic + four float64 projected bounds
(left, top, right, bottom). Cache: 16 tiles in PSRAM; route arrays also use PSRAM
with checked allocation. The legacy small-area `OCM1` files remain readable.

Road rasterization runs on a core-0 worker, separately from UI/touch on core 1.
Two 480×480 RGB565 PSRAM images (~900 KiB total) cache the background with extra
space around the viewport. Small drags shift the cached image; a new raster is
requested after 80 pixels of movement or a zoom change. Requests are coalesced,
and offscreen road segments are culled. The worker releases the SD lock before
drawing. Navigation state has its own mutex, so tile SD reads do not block cached
map movement. UI state updates use nonblocking acquisition during route sync and
retain touch deltas across busy samples. Navigation has a 33 ms minimum frame interval instead of
200 ms; this is a scheduling target, not a guaranteed hardware frame rate.
Fast drags beyond the cache show `loading map...` until data catches up, distinct
from a missing map. No SD map conversion or re-copy is needed for this renderer.

## Verification

```sh
pio run -e esp32-s3-devkitc-1
pio test -e native
python3 tests/run_native_tests.py
python3 tests/run_navigation_tests.py
python3 tests/run_ride_tests.py
python3 tests/test_offline_map.py
cd app
flutter analyze
flutter test
```

Host integration tests compile the actual firmware navigation implementation
with fake SD/BLE/display peripherals and the undefined-behavior sanitizer.
Hardware acceptance still requires the physical board: pan/zoom latency in dense
areas, simultaneous ride logging, SD removal/full-card behavior, BLE reconnect,
and following a real ride with GPS loss and off-route recovery. No simulated
test can certify display timing or radio reliability on the board.
