# Map Road Names & Road-Width Rendering

**Date:** 2026-09-18
**Status:** Approved for Implementation Planning

## 1. Overview & Purpose

Bring opencyclo's offline navigation screen closer to a reference commercial
bike computer (iGPSPORT) in two ways:

1. **Road width by class** — major roads render visibly thicker than local
   roads/paths on the map (today every road is a uniform 1px line; only
   color varies by class).
2. **Current street name banner** — the header above the map shows the name
   of the road the rider is currently on/nearest to, updating as they move,
   matching the reference photo's single top banner. This is deliberately
   **not** a full on-map multi-label system: only one name is ever shown
   (the nearest one), so no label-placement/collision-avoidance logic is
   needed — the hardest part of "road names on a map" is designed away.

Both changes require a new, additive map data format and a full rebuild of
the Indonesia map pack (`tools/build_indonesia_map.py`), since the existing
`OCP1` tile format has no name strings and no "which segments belong to the
same road" grouping at all — way identity is discarded at build time today.

---

## 2. Road Width by Class

`src/navigation/map_renderer.cpp`'s `rasterize()` currently draws every
segment with `image.drawLine(ax,ay,bx,by,color)` — width is implicitly 1px
for every road class. `canvas.drawWideLine(x,y,x,y,width,color)` is already
used elsewhere in this codebase (the turn-arrow icon in `navigation.cpp`),
so this is a drop-in swap:

- Style `1` (major: motorway/trunk/primary/secondary/tertiary) →
  `drawWideLine(..., 3, 0x8410)`
- Style `2` (path/cycleway/track/footway) → `drawWideLine(..., 1, 0x35ad)`
  (unchanged width — these should stay thin/minor visually)
- Style `0` (everything else / local roads) → `drawWideLine(..., 2, 0x4208)`

No data format change is needed for this part — it's a pure rendering
change. It's bundled into the same release as the road-names format bump
only because both require regenerating/re-testing map rendering together,
not because they're technically coupled.

---

## 3. Road Names: Data Format (`OCN1`)

### 3.1 Design decision: additive, not a rework of `OCP1`

`OCP1` (existing, unchanged) continues to carry **all** road segments for
line rendering, with no name data — untouched, so existing rendering can't
regress. A new, separate per-column file carries **only named roads**:

`/maps/14/<tx>.ocn` — present only for columns that contain at least one
named way. Firmware treats a missing `.ocn` file as "no named roads in this
column" (graceful degradation — old map packs without this file still work,
banner just never has anything to show).

### 3.2 `OCN1` binary layout

Mirrors `OCP1`'s existing header/index/binary-search-by-row pattern (same
convention this codebase's map loader already implements), with a string
pool inserted between the index and the segment data:

```
Header (12 bytes):
  magic          4 bytes   "OCN1"
  index_count    uint32 LE  number of (row) index entries that follow
  pool_size      uint32 LE  size in bytes of the string pool

Index (index_count × 12 bytes, sorted by row y — same binary-search
convention as OCP1):
  y              uint32 LE  tile row
  offset         uint32 LE  byte offset into the segment-data section
                             (i.e. relative to the end of the string pool)
  count          uint32 LE  number of named-road segments in this row

String pool (pool_size bytes):
  UTF-8 road names, each NUL-terminated, concatenated. A segment's name
  is referenced by its byte offset into this pool. Deduplicated per
  column at build time (a road with the same name appearing many times
  only stores the string once). Pool must stay ≤ 65535 bytes per column
  (fits the 2-byte offset field below) — comfortably sufficient since
  only named ways contribute, deduplicated.

Segment data (sequential per row, sorted to match the index; each
segment 10 bytes):
  ax, ay, bx, by   4 × uint16 LE   tile-local coords, same /256.0 pixel
                                    scaling convention as OCP1
  name_offset      uint16 LE       byte offset into the string pool
```

### 3.3 Build tool changes (`tools/build_indonesia_map.py`)

In `Roads.way()`, alongside the existing per-node segment clipping/grouping
into `grouped` (unchanged, still feeds `OCP1`), add a second pass: if
`w.tags.get('name')` is present, run the same per-tile clipping logic
against a **second** grouping dict keyed the same way `(tx,ty)`, storing
`(ax,ay,bx,by,name)` tuples instead of `(...,style)`. The name-to-pool-offset
mapping is column-local, built the same way `export_columns()` currently
finalizes `OCP1` per-column: dedupe names into a pool, resolve each
segment's offset, then write index + pool + segment data in that order,
skipping columns whose named-segment count is zero (no `.ocn` file written
for that column at all).

This is entirely additive to `build_indonesia_map.py` — the existing
`OCP1`-writing path in `export_columns()` is not modified, only extended.

### 3.4 Rebuild requirement

Once this format lands, the Indonesia map pack must be regenerated in full
via the existing (unchanged) build process — this is the slow, disk-heavy
step already documented in `docs/offline-navigation.md`, not something new
this feature introduces.

---

## 4. Firmware: Loading Named-Road Tiles

`map_renderer.cpp` already has a tile cache (`Tile tiles[16]`, keyed by
`x,y`, LRU-by-round-robin) specific to `OCP1`'s 9-byte segment format. A
**parallel** cache is added for `OCN1` data — same shape (fixed-size ring
buffer, same open/binary-search-by-row pattern against `<tx>.ocn`), but its
own struct since entry size (10 bytes vs 9) and content (name offsets vs
style bytes) differ. This reuses the existing loader pattern rather than
inventing a new one.

---

## 5. Firmware: Nearest-Named-Road Lookup & Banner

### 5.1 Where it runs

`renderNavigation(const TelemetryState& state)` already has `state.lat`,
`state.lon` (double) and `state.gps_has_fix` (bool) each frame, and already
converts position to a `nav::Point` (`{int32_t(lat*1e7), int32_t(lon*1e7)}`)
for route matching. The nearest-road lookup uses this same `here` point.

Recompute is throttled to roughly once per GPS update (~1Hz) rather than
every render frame — cache the last result and reuse it between GPS
updates, since position barely changes at render-loop frequency.

### 5.2 Matching algorithm

For the currently-loaded map column/row (same tile the renderer is already
displaying), iterate that row's `OCN1` named segments and compute
`nav::segmentDistance(here, a, b, fraction)` — **already implemented** in
`src/navigation/geo.h`, the same point-to-segment helper the route
off-distance logic already uses, applied here to named-road segments
instead of route segments. Track the minimum distance and its name.

- Nearest match **within 40 meters** → that name is the banner text.
- No match within 40 meters (or no `.ocn` data for this tile, or no GPS
  fix) → fall back to existing behavior (see §5.3).

### 5.3 Where the banner renders

The header area (`y=0..44`) is already fully occupied: a Back icon at
`(10,10)`, a route-name/"Free ride" label clipped to `(48,0,144,44)` at
`(48,12)`, and a Ride icon at `(206,10)` — this only happens in the map
view branch (not `choosing`/`cueList`). Rather than growing the header band
(which would shrink the already-tight `240×206` map viewport further), the
street-name banner **replaces the content of the existing route-name label
slot** when a named-road match exists:

- Named road matched → draw the road name in that slot instead of
  `routeName`/`"Free ride"`.
- No match → unchanged existing behavior (`routeName` truncated to 16
  chars, or `"Free ride"`).

Same truncation convention as the existing label (`substring(0,16)` or
similar) applies to road names too, so a long name is cut off rather than
overflowing into the Ride icon — this matches the reference photo's own
apparent truncation ("...an Raya Cirebon - Brebes").

---

## 6. Out of Scope

- No on-map multi-label rendering, no label placement/collision avoidance —
  deliberately avoided per §1.
- No compass/north-up-vs-course-up toggle — map stays north-up, unchanged.
- No bottom Speed/Heading stat-tile redesign, no zoom-button repositioning
  — existing bottom row and side controls are unchanged.
- No street names for roads without an OSM `name` tag (most rural
  paths/tracks) — banner simply shows nothing/falls back for those.
- No live/online geocoding — this is 100% offline, sourced from the same
  OSM extract already used for `OCP1`.

---

## 7. Implementation Milestones

1. **Build tool**: extend `tools/build_indonesia_map.py` to emit `OCN1`
   per-column files alongside existing `OCP1` output.
2. **Firmware: OCN1 tile cache & loader** in `map_renderer.cpp`, mirroring
   the existing `OCP1` cache pattern.
3. **Firmware: road-width rendering** — swap `drawLine` for `drawWideLine`
   by class in `rasterize()`.
4. **Firmware: nearest-named-road matching** using `nav::segmentDistance`
   against loaded `OCN1` data for the current position.
5. **Firmware: banner integration** in `navigation.cpp`'s header-label
   logic, replacing route-name content when a match exists.
6. **Map data rebuild**: regenerate the full Indonesia map pack with the
   new build tool, verify `.ocn` files are present for populated regions
   and `.ocp` output is byte-identical where no named-road logic touches
   it (i.e. confirm the additive change didn't alter existing render data).
7. **On-hardware verification**: confirm banner text updates as the device
   moves along a real or simulated route with known street names, confirm
   graceful fallback in areas with no named-road data, confirm road width
   visually matches class (major roads visibly thicker).

---

## 8. Spec Self-Review Checklist
- [x] **Placeholder Scan:** No "TBD"/"TODO"; the `OCN1` byte layout, the 40m
      match threshold, and the banner-slot reuse are all explicit rather
      than left open.
- [x] **Internal Consistency:** `OCN1`'s header/index pattern matches the
      existing `OCP1` convention confirmed in `map_renderer.cpp`'s actual
      parsing code (4-byte magic, uint32 LE index entries, tile-local
      uint16 coords) — not just the Python writer's side.
- [x] **Scope Check:** Two related but independently-shippable pieces
      (road width, road names) bundled per explicit user request; each
      has its own milestone and could theoretically ship separately if
      the plan later needs to split them.
- [x] **Ambiguity Check:** The single-banner (not multi-label) design
      decision, the additive-not-reworked format decision, and the
      header-slot-reuse decision are all stated with their rationale, not
      left for the implementer to guess.
