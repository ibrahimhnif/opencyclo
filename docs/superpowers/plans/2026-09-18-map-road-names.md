# Map Road Names & Road-Width Rendering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Render major roads thicker than local roads on the offline map, and show the name of the road the rider is currently on/near as a banner on the navigation screen.

**Architecture:** A new, additive binary format (`OCN1`) carries only named-road geometry + a deduplicated string pool per map column, built alongside the existing unchanged `OCP1` line-rendering data by `tools/build_indonesia_map.py`. Firmware loads `.ocn` files through a small parallel tile cache (mirroring the existing `OCP1` loader), matches the rider's GPS position against loaded named segments using the existing `nav::segmentDistance` helper, and shows the nearest match (within 40m) in the navigation screen's existing header label slot — no new UI layout, no on-map label placement.

**Tech Stack:** Python 3 (`tools/build_indonesia_map.py`, `pyosmium`), C++17 (ESP32-S3 firmware, PlatformIO), Unity (native test env), LovyanGFX (`drawWideLine`).

**Spec:** `docs/superpowers/specs/2026-09-18-map-road-names-design.md`

## Global Constraints

- `OCN1` binary layout (exact, from the spec):
  - Header (12 bytes): `magic` (4 bytes, `"OCN1"`), `index_count` (uint32 LE), `pool_size` (uint32 LE).
  - Index (`index_count` × 12 bytes, sorted by row `y`): `y` (uint32 LE), `offset` (uint32 LE, relative to the start of the segment-data section, i.e. after the pool), `count` (uint32 LE).
  - String pool (`pool_size` bytes): UTF-8 names, each NUL-terminated, concatenated, deduplicated per column. Must be ≤ 65535 bytes per column.
  - Segment data (sequential per row, 10 bytes each): `ax,ay,bx,by` (4 × uint16 LE, tile-local `/256.0` pixel coords — same convention as `OCP1`), `name_offset` (uint16 LE, byte offset into the pool).
- Nearest-road match threshold: **40 meters**.
- Banner recompute throttle: **at most once per 1000ms** (not every render frame).
- Road width by class in `rasterize()`: style `1` (major) → `drawWideLine` width `3`; style `0` (local) → width `2`; style `2` (path/cycleway) stays width `1` (unchanged, still `drawLine`-equivalent visually).
- `OCP1` (existing line-rendering format) is never modified by this plan — `OCN1` is purely additive, missing `.ocn` files must degrade gracefully (banner simply has nothing to show).
- No on-map multi-label rendering, no compass/rotation toggle, no bottom stat-tile redesign, no zoom-button repositioning — all explicitly out of scope per the spec.

---

### Task 1: Road width by class

**Files:**
- Modify: `src/navigation/map_renderer.cpp:104`

**Interfaces:**
- Consumes: nothing new (existing `t.bytes[i+8]` style byte, existing `out.image` `LGFX_Sprite`).
- Produces: nothing consumed by later tasks — purely visual, independent of Tasks 2-5.

Not unit-testable (touches SD/PSRAM/display rendering, no test harness exists for `map_renderer.cpp` in this repo) — verified by firmware build succeeding; visual confirmation happens in Task 7's hardware pass.

- [ ] **Step 1: Change the line-drawing call to vary width by class**

Edit `src/navigation/map_renderer.cpp`, replace this line (104):

```cpp
      out.image.drawLine(ax,ay,bx,by,t.bytes[i+8]==2?0x35ad:(t.bytes[i+8]==1?0x8410:0x4208));
```

with:

```cpp
      const uint8_t style=t.bytes[i+8];
      const int width=style==1?3:style==0?2:1;
      const uint16_t color=style==2?0x35ad:(style==1?0x8410:0x4208);
      out.image.drawWideLine(ax,ay,bx,by,width,color);
```

- [ ] **Step 2: Build the firmware target to confirm it compiles**

Run: `pio run -e esp32-s3-devkitc-1` (match the actual env name in `platformio.ini` if it differs)
Expected: PASS — no compile errors (`drawWideLine` is already used elsewhere in this codebase, e.g. `src/navigation/navigation.cpp`'s turn-arrow icon, so the signature is already proven compatible with this LovyanGFX version).

- [ ] **Step 3: Commit**

```bash
git add src/navigation/map_renderer.cpp
git commit -m "feat(map): draw major roads thicker than local roads"
```

---

### Task 2: Inverse Web Mercator projection (`nav::unproject`)

**Files:**
- Modify: `src/navigation/geo.h`
- Test: `test/test_navigation/test_navigation.cpp`

**Interfaces:**
- Consumes: `nav::Point`, `nav::pi`, `nav::world` (all already defined in `geo.h`).
- Produces: `nav::Point unproject(double worldX, double worldY)` — converts a Web Mercator world-pixel coordinate (same space as `nav::x(Point)`/`nav::y(Point)`'s output, zoom-14 pixel units) back into a `nav::Point` (degrees × 1e7). Task 4 calls this to convert `OCN1`'s on-disk tile-local pixel coordinates back into lat/lon for use with the existing `nav::segmentDistance`.

- [ ] **Step 1: Write the failing test**

Edit `test/test_navigation/test_navigation.cpp` — add a new test function and register it in `main()`:

```cpp
#include <unity.h>
#include "navigation/geo.h"
void projection() {
  nav::Point p{0,0};TEST_ASSERT_DOUBLE_WITHIN(0.01,2097152,nav::x(p));TEST_ASSERT_DOUBLE_WITHIN(0.01,2097152,nav::y(p));
}
void unprojection() {
  nav::Point origin{0,0};
  nav::Point back=nav::unproject(nav::x(origin),nav::y(origin));
  TEST_ASSERT_INT32_WITHIN(5,0,back.lat);TEST_ASSERT_INT32_WITHIN(5,0,back.lon);
  nav::Point jakarta{-60000000,1067000000};
  nav::Point back2=nav::unproject(nav::x(jakarta),nav::y(jakarta));
  TEST_ASSERT_INT32_WITHIN(5,jakarta.lat,back2.lat);TEST_ASSERT_INT32_WITHIN(5,jakarta.lon,back2.lon);
}
void matching() {
  double fraction;
  double d=nav::segmentDistance({1000,5000},{0,0},{0,10000},fraction);
  TEST_ASSERT_DOUBLE_WITHIN(0.001,0.5,fraction);TEST_ASSERT_DOUBLE_WITHIN(0.1,11.12,d);
  nav::segmentDistance({0,-5000},{0,0},{0,10000},fraction);TEST_ASSERT_EQUAL_DOUBLE(0,fraction);
}
void packets() {
  nav::Header h{};memcpy(h.magic,"OCR1",4);h.points=2;
  TEST_ASSERT_TRUE(nav::headerValid(h,76));TEST_ASSERT_FALSE(nav::headerValid(h,75));
  h.points=nav::maxPoints+1;TEST_ASSERT_FALSE(nav::headerValid(h,76));
  TEST_ASSERT_EQUAL_HEX32(0xcbf43926,nav::crc32((const uint8_t*)"123456789",9)^0xffffffff);
  TEST_ASSERT_FALSE(nav::valid({900000000,0}));
}
int main() {UNITY_BEGIN();RUN_TEST(projection);RUN_TEST(unprojection);RUN_TEST(matching);RUN_TEST(packets);return UNITY_END();}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `pio test -e native -f test_navigation`
Expected: FAIL — `nav::unproject` undefined (compile error).

- [ ] **Step 3: Implement `unproject`**

Edit `src/navigation/geo.h`, add right after the existing `y(Point p)` function (after line 12):

```cpp
inline Point unproject(double worldX, double worldY) {
  double lonDeg = worldX / world * 360.0 - 180.0;
  double n = pi * (1.0 - 2.0 * worldY / world);
  double latDeg = std::atan(std::sinh(n)) * 180.0 / pi;
  return Point{int32_t(latDeg * 1e7), int32_t(lonDeg * 1e7)};
}
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `pio test -e native -f test_navigation`
Expected: PASS — 2/2 assertions in `unprojection`, plus the pre-existing `projection`/`matching`/`packets` tests still pass.

- [ ] **Step 5: Run the full native suite to confirm no regressions**

Run: `pio test -e native`
Expected: PASS (34/34 — 33 existing + 1 new test function; test count, not test-case count, since `unprojection` adds 4 new assertions inside one Unity test function).

- [ ] **Step 6: Commit**

```bash
git add src/navigation/geo.h test/test_navigation/test_navigation.cpp
git commit -m "feat(nav): add inverse Web Mercator projection"
```

---

### Task 3: `OCN1` named-road export (`tools/build_indonesia_map.py`)

**Files:**
- Modify: `tools/build_indonesia_map.py`
- Test: `tests/test_offline_map.py`

**Interfaces:**
- Consumes: nothing from earlier tasks (independent of the C++ tasks).
- Produces: `export_named_columns(db, destination, x0, x1)` — writes `/maps/14/<tx>.ocn` for every column in `[x0, x1]` that has at least one named road, reading from a new `named_segments(tx, ty, name, data)` SQLite table (`data` = 8-byte tile-local `ax,ay,bx,by` uint16 LE coordinates, no style byte). Returns `{'named_segments': int, 'named_tiles': int}`. Later tasks (4) read the file format this produces but don't call this function directly (it only runs at map-build time, not on-device).

- [ ] **Step 1: Write the failing test**

Add to `tests/test_offline_map.py`, inside `class MapTests(unittest.TestCase)` (after `test_indexed_pack_and_corruption_detection`):

```python
    def test_named_columns_pack_and_lookup(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            db=sqlite3.connect(':memory:')
            db.execute('CREATE TABLE named_segments(tx INTEGER,ty INTEGER,name TEXT,data BLOB)')
            db.execute('INSERT INTO named_segments VALUES (8192,8192,?,?)',
                       ('Jalan Test',struct.pack('<4H',0,0,65535,65535)))
            export_named_columns(db,root,8192,8192)
            path=root/'maps'/'14'/'8192.ocn'
            self.assertTrue(path.exists())
            data=path.read_bytes()
            magic,index_count,pool_size=struct.unpack_from('<4sII',data,0)
            self.assertEqual(magic,b'OCN1');self.assertEqual(index_count,1)
            y,offset,count=struct.unpack_from('<III',data,12)
            self.assertEqual((y,offset,count),(8192,0,1))
            pool_start=12+index_count*12
            pool=data[pool_start:pool_start+pool_size]
            self.assertEqual(pool[:len(b'Jalan Test')],b'Jalan Test')
            self.assertEqual(pool[len(b'Jalan Test')],0)  # NUL terminator
            seg_start=pool_start+pool_size
            ax,ay,bx,by,name_offset=struct.unpack_from('<4HH',data,seg_start)
            self.assertEqual((ax,ay,bx,by,name_offset),(0,0,65535,65535,0))

    def test_named_columns_skips_unnamed_columns(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            db=sqlite3.connect(':memory:')
            db.execute('CREATE TABLE named_segments(tx INTEGER,ty INTEGER,name TEXT,data BLOB)')
            export_named_columns(db,root,8192,8192)
            self.assertFalse((root/'maps'/'14'/'8192.ocn').exists())
```

Add `export_named_columns` to the existing import line near the top of the file:

```python
from build_indonesia_map import export_columns, export_named_columns
```

- [ ] **Step 2: Run it to verify it fails**

Run: `cd tests && python3 -m unittest test_offline_map.MapTests.test_named_columns_pack_and_lookup test_offline_map.MapTests.test_named_columns_skips_unnamed_columns -v`
Expected: FAIL — `ImportError: cannot import name 'export_named_columns'`.

- [ ] **Step 3: Implement `export_named_columns` and wire it into `build()`**

Edit `tools/build_indonesia_map.py` — add this new function right after `export_columns` (after line 46):

```python
def export_named_columns(db, destination, x0, x1):
    target=Path(destination)/'maps'/'14'
    named_segments=named_tiles=0
    for tx in range(x0,x1+1):
        groups={}
        for ty,name,blob in db.execute('SELECT ty,name,data FROM named_segments WHERE tx=? ORDER BY ty',(tx,)):
            groups.setdefault(ty,{}).setdefault(name,bytearray()).extend(blob)
        if not groups:continue
        pool=bytearray();offsets={}
        for by_name in groups.values():
            for name in by_name:
                if name not in offsets:
                    offsets[name]=len(pool);pool.extend(name.encode('utf-8'));pool.append(0)
        if len(pool)>65535:raise ValueError(f'column {tx} name pool exceeds 65535 bytes')
        entries=[];offset=0;segment_data=bytearray()
        for ty,by_name in sorted(groups.items()):
            row=bytearray()
            for name,coords in by_name.items():
                name_offset=offsets[name]
                for i in range(0,len(coords),8):
                    row.extend(coords[i:i+8]);row.extend(struct.pack('<H',name_offset))
            count=len(row)//10
            if count>20000:raise ValueError(f'named tile {tx}/{ty} exceeds 20,000 segments')
            entries.append((ty,offset,count));offset+=len(row)
            segment_data.extend(row)
            named_segments+=count;named_tiles+=1
        with (target/f'{tx}.ocn').open('wb') as out:
            out.write(struct.pack('<4sII',b'OCN1',len(entries),len(pool)))
            for entry in entries:out.write(struct.pack('<III',*entry))
            out.write(pool)
            out.write(segment_data)
    return {'named_segments':named_segments,'named_tiles':named_tiles}
```

Edit the `Roads` class inside `build()` (lines 60-87) to also capture named-way geometry. Replace the whole class with:

```python
        class Roads(osmium.SimpleHandler):
            ways=0
            pending=[]
            namedPending=[]
            def way(self,w):
                road=w.tags.get('highway')
                if not road or road in ('proposed','construction'):return
                style=2 if road in ('cycleway','path','track','footway') else 1 if road in ('motorway','trunk','primary','secondary','tertiary') else 0
                name=w.tags.get('name')
                grouped={};named={};previous=None
                for node in w.nodes:
                    if not node.location.valid():previous=None;continue
                    point=project(node.lon,node.lat)
                    if previous:
                        segment=clip(*previous,*point,left,top,right,bottom)
                        if segment:
                            ax,ay,bx,by=segment
                            for tx in range(math.floor(min(ax,bx)/256),math.floor(max(ax,bx)/256)+1):
                                for ty in range(math.floor(min(ay,by)/256),math.floor(max(ay,by)/256)+1):
                                    cut=clip(ax,ay,bx,by,tx*256,ty*256,(tx+1)*256,(ty+1)*256)
                                    if cut:
                                        coords=[max(0,min(65535,round((v-(tx if i%2==0 else ty)*256)*256))) for i,v in enumerate(cut)]
                                        grouped.setdefault((tx,ty),bytearray()).extend(struct.pack('<4HB',*coords,style))
                                        if name:named.setdefault((tx,ty),bytearray()).extend(struct.pack('<4H',*coords))
                    previous=point
                self.pending.extend((x,y,bytes(data)) for (x,y),data in grouped.items())
                if name:self.namedPending.extend((x,y,name,bytes(data)) for (x,y),data in named.items())
                if len(self.pending)>10000:self.flush()
                if len(self.namedPending)>10000:self.flushNamed()
                self.ways+=1
                if self.ways%100000==0:print(f'{self.ways:,} roads, {time.monotonic()-started:.0f}s',flush=True)
            def flush(self):
                db.executemany('INSERT INTO segments VALUES (?,?,?)',self.pending);self.pending.clear();db.commit()
            def flushNamed(self):
                db.executemany('INSERT INTO named_segments VALUES (?,?,?,?)',self.namedPending);self.namedPending.clear();db.commit()
```

Edit the `db.execute('CREATE TABLE segments...')` line (line 59) to also create the named-segments table right after it:

```python
        db.execute('CREATE TABLE segments(tx INTEGER,ty INTEGER,data BLOB)')
        db.execute('CREATE TABLE named_segments(tx INTEGER,ty INTEGER,name TEXT,data BLOB)')
```

Edit the `roads.flush()` call (line 90) to also flush the named-segments buffer, and add an index:

```python
        roads.apply_file(str(source),locations=True,idx=f'sparse_file_array,{tmp}/locations')
        roads.flush();roads.flushNamed()
        print('Indexing tile columns...',flush=True)
        db.execute('CREATE INDEX tile ON segments(tx,ty)')
        db.execute('CREATE INDEX named_tile ON named_segments(tx,ty)')
        db.commit()
        report=export_columns(db,destination,bbox)
        report.update(export_named_columns(db,destination,x0,x1))
        db.close()
```

Note: `x0,x1` are not currently available in `build()`'s scope (only inside `export_columns`). Add them right after the existing `left,bottom=project(...);right,top=project(...)` line in `build()` (around line 54):

```python
    left,bottom=project(west,south);right,top=project(east,north)
    x0,x1=math.floor(left/256),math.floor(right/256)
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `cd tests && python3 -m unittest test_offline_map.MapTests.test_named_columns_pack_and_lookup test_offline_map.MapTests.test_named_columns_skips_unnamed_columns -v`
Expected: PASS — 2/2.

- [ ] **Step 5: Run the full Python test suite to confirm no regressions**

Run: `cd tests && python3 -m unittest test_offline_map -v`
Expected: PASS — all existing tests (`test_indexed_pack_and_corruption_detection`, `test_clipping`, `test_build_cross_tile_road_and_empty_coverage`) plus the 2 new ones.

- [ ] **Step 6: Commit**

```bash
git add tools/build_indonesia_map.py tests/test_offline_map.py
git commit -m "feat(map): export named-road OCN1 columns from OSM way names"
```

---

### Task 4: Firmware named-road tile cache and lookup (`nearestRoadName`)

**Files:**
- Modify: `src/navigation/map_renderer.h`
- Modify: `src/navigation/map_renderer.cpp`

**Interfaces:**
- Consumes: `nav::Point`, `nav::valid`, `nav::x`, `nav::y`, `nav::unproject`, `nav::segmentDistance` (Task 2 adds `unproject`, the rest already exist in `geo.h`). `PsBuffer<uint8_t>` (existing).
- Produces: `bool nearestRoadName(double lat, double lon, char* name, size_t nameLen)` — declared in `map_renderer.h`, called by Task 5 from `navigation.cpp`.

Not unit-testable (SD/PSRAM-dependent, same as the existing `tile()`/`drawMapBackground` in this file — no test harness exists for this file in this repo) — verified by firmware build; functional correctness verified end-to-end in Task 7 using real `.ocn` data produced by Task 3's tool.

- [ ] **Step 1: Declare the new public function**

Edit `src/navigation/map_renderer.h` to:

```cpp
#pragma once
#include <cstddef>
enum class MapStatus { Loading, Ready, Missing, NoMemory };
// UI-only: request/coalesce a viewport and blit the latest completed raster.
MapStatus drawMapBackground(double x, double y, int zoom);
// Looks up the nearest named road within 40m of (lat,lon) using the OCN1
// data for that position's map tile (current tile only, no cross-tile-
// boundary search). Returns true and NUL-terminates `name` (writing at
// most nameLen-1 characters + terminator) on a match; returns false and
// leaves `name` untouched otherwise (no .ocn file for this column, no
// match within range, or an invalid/absent GPS fix).
bool nearestRoadName(double lat, double lon, char* name, size_t nameLen);
```

- [ ] **Step 2: Add the `NamedTile` cache and loader**

Edit `src/navigation/map_renderer.cpp` — add right after the existing `Tile& tile(int x,int y) { ... }` function (after line 52, before `struct View`):

```cpp
struct NamedTile { int x=-1,y=-1; bool present=false; PsBuffer<uint8_t> pool; PsBuffer<uint8_t> segments; };
NamedTile namedTiles[4];
int nextNamedTile=0;
NamedTile& namedTile(int x,int y) {
  for(auto& t:namedTiles) if(t.x==x&&t.y==y) return t;
  NamedTile& t=namedTiles[nextNamedTile++%4];t.x=x;t.y=y;t.pool.clear();t.segments.clear();t.present=false;
  char path[64];snprintf(path,sizeof(path),"/maps/14/%d.ocn",x);
  File f=SD_MMC.open(path);
  if(!f)return t; // No named-road data for this column -- not an error.
  uint8_t h[12];
  if(f.read(h,12)!=12 || memcmp(h,"OCN1",4))return t;
  uint32_t indexCount,poolSize;memcpy(&indexCount,h+4,4);memcpy(&poolSize,h+8,4);
  if(indexCount>16384 || poolSize>65535 || f.size()<12+uint64_t(indexCount)*12+poolSize)return t;
  uint32_t lo=0,hi=indexCount;
  while(lo<hi) {
    uint32_t mid=(lo+hi)/2,entry[3];
    if(!f.seek(12+mid*12)||f.read((uint8_t*)entry,12)!=12)return t;
    if(entry[0]<(uint32_t)y)lo=mid+1;
    else if(entry[0]>(uint32_t)y)hi=mid;
    else {
      if(entry[2]>20000 || 12+uint64_t(indexCount)*12+poolSize+uint64_t(entry[1])+entry[2]*10>f.size())return t;
      if(!t.pool.resize(poolSize) || !t.segments.resize(entry[2]*10))return t;
      if(!f.seek(12+indexCount*12) || (poolSize && f.read(t.pool.data(),poolSize)!=poolSize))return t;
      if(!f.seek(12+indexCount*12+poolSize+entry[1]) || (entry[2] && f.read(t.segments.data(),t.segments.size())!=t.segments.size()))return t;
      t.present=true;return t;
    }
  }
  t.present=true; // Empty row inside a completed pack -- no named roads here.
  return t;
}
```

- [ ] **Step 3: Add the public `nearestRoadName` function**

Edit `src/navigation/map_renderer.cpp` — add right after the closing `}` of the anonymous namespace (after line 157, before `MapStatus drawMapBackground(...)`):

```cpp
bool nearestRoadName(double lat,double lon,char* name,size_t nameLen) {
  if(nameLen==0)return false;
  nav::Point here{int32_t(lat*1e7),int32_t(lon*1e7)};
  if(!nav::valid(here))return false;
  double wx=nav::x(here),wy=nav::y(here);
  int tx=int(std::floor(wx/256)),ty=int(std::floor(wy/256));
  NamedTile* loaded=nullptr;
  {
    SdGuard sd;
    if(sd.locked && g_sd_ready && !isPowerOffRequested())loaded=&namedTile(tx,ty);
  }
  if(!loaded || !loaded->present || loaded->segments.size()==0)return false;
  double best=1e18;uint16_t bestOffset=0;bool found=false;
  for(size_t i=0;i<loaded->segments.size();i+=10) {
    uint16_t p[4];uint16_t nameOffset;
    memcpy(p,loaded->segments.data()+i,8);
    memcpy(&nameOffset,loaded->segments.data()+i+8,2);
    nav::Point a=nav::unproject(tx*256.0+p[0]/256.0,ty*256.0+p[1]/256.0);
    nav::Point b=nav::unproject(tx*256.0+p[2]/256.0,ty*256.0+p[3]/256.0);
    double fraction;double d=nav::segmentDistance(here,a,b,fraction);
    if(d<best && nameOffset<loaded->pool.size()) {best=d;bestOffset=nameOffset;found=true;}
  }
  if(!found || best>40)return false;
  const char* poolStr=(const char*)loaded->pool.data()+bestOffset;
  size_t maxLen=loaded->pool.size()-bestOffset;
  size_t len=strnlen(poolStr,maxLen);
  if(len>=nameLen)len=nameLen-1;
  memcpy(name,poolStr,len);name[len]=0;
  return true;
}
```

Add `#include <cstring>` to the top of `src/navigation/map_renderer.cpp` (after the existing `#include <algorithm>` on line 8) for `strnlen`/`memcpy`/`memcmp`.

- [ ] **Step 4: Build the firmware target to confirm it compiles**

Run: `pio run -e esp32-s3-devkitc-1` (match the actual env name in `platformio.ini`)
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/navigation/map_renderer.h src/navigation/map_renderer.cpp
git commit -m "feat(map): add OCN1 named-road tile cache and nearest-road lookup"
```

---

### Task 5: Street-name banner integration

**Files:**
- Modify: `src/navigation/navigation.cpp`

**Interfaces:**
- Consumes: `nearestRoadName(double, double, char*, size_t)` (Task 4).

Not unit-testable (rendering code, no test harness for `navigation.cpp` in this repo) — verified by firmware build; functional behavior verified in Task 7.

- [ ] **Step 1: Replace the header label with a throttled, cached lookup**

Edit `src/navigation/navigation.cpp`, replace this block (lines 576-582):

```cpp
  canvas.fillScreen(TFT_BLACK);canvas.setTextSize(1);canvas.setFont(&fonts::Font0);canvas.setTextPadding(0);
  ui::drawIcon(canvas,ui::Icon::Back,10,10,TFT_WHITE);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setClipRect(48,0,144,44);
  label(choosing?"Routes":routeName.length()?routeName.substring(0,16):"Free ride",48,12,ui::accent);
  canvas.clearClipRect();
  ui::drawIcon(canvas,ui::Icon::Ride,206,10,ui::success);
```

with:

```cpp
  canvas.fillScreen(TFT_BLACK);canvas.setTextSize(1);canvas.setFont(&fonts::Font0);canvas.setTextPadding(0);
  ui::drawIcon(canvas,ui::Icon::Back,10,10,TFT_WHITE);
  canvas.setFont(&fonts::FreeSansBold9pt7b);
  canvas.setClipRect(48,0,144,44);
  static uint32_t lastRoadLookupMs=0;
  static char cachedRoadName[24]={0};
  static bool cachedOnNamedRoad=false;
  uint32_t nowMs=millis();
  if(nowMs-lastRoadLookupMs>=1000) {
    lastRoadLookupMs=nowMs;
    cachedOnNamedRoad=fix && nearestRoadName(state.lat,state.lon,cachedRoadName,sizeof(cachedRoadName));
  }
  label(choosing?"Routes":cachedOnNamedRoad?cachedRoadName:routeName.length()?routeName.substring(0,16):"Free ride",48,12,ui::accent);
  canvas.clearClipRect();
  ui::drawIcon(canvas,ui::Icon::Ride,206,10,ui::success);
```

- [ ] **Step 2: Build the firmware target to confirm it compiles**

Run: `pio run -e esp32-s3-devkitc-1` (match the actual env name in `platformio.ini`)
Expected: PASS.

- [ ] **Step 3: Run the full native suite to confirm no regressions**

Run: `pio test -e native`
Expected: PASS (unaffected — `navigation.cpp` isn't part of the native test env).

- [ ] **Step 4: Commit**

```bash
git add src/navigation/navigation.cpp
git commit -m "feat(nav): show nearest named road in the navigation header banner"
```

---

### Task 6: Rebuild the Indonesia map pack

**Files:** none (operational step, produces map data, not source code)

No code changes — this task regenerates the on-SD map pack with the new `OCN1` files using the tool built in Task 3.

- [ ] **Step 1: Run the build tool against the same Indonesia OSM PBF extract used for the current production map pack**

Run: `python tools/build_indonesia_map.py indonesia.osm.pbf output` (adjust `indonesia.osm.pbf` to the actual source file path used for the existing pack; per `tools/build_indonesia_map.py`'s own docstring, `pip install -r tools/map_requirements.txt` first if not already installed)

Expected: completes without raising (per the existing `build()` function's own error paths — e.g. `ValueError` if a tile/column exceeds its byte budget); prints a JSON report including both `segments`/`populated_tiles` (existing `OCP1` stats) and the new `named_segments`/`named_tiles` counts from Task 3.

- [ ] **Step 2: Verify the existing `.ocp` output is unaffected**

Run: `python tools/verify_map_pack.py output/maps`
Expected: `{"verified": true, ...}` — confirms `OCP1` column files, checksums, and manifest counts are all still internally consistent (this script only ever inspects `.ocp` files, so it implicitly proves the additive `OCN1` change didn't corrupt existing render data).

- [ ] **Step 3: Spot-check `.ocn` files exist for a populated region**

Run: `ls output/maps/14/*.ocn | wc -l`
Expected: a nonzero count (some columns should have named roads) — if zero, stop and investigate before proceeding (likely means the OSM extract's ways don't carry `name` tags, or Task 3's wiring has a bug).

- [ ] **Step 4: Copy the rebuilt pack to the device's SD card**

Copy `output/maps/` to the SD card root, replacing the existing `maps/` directory, per the existing copy step documented in `docs/offline-navigation.md`.

No commit for this task (it produces map data files, not source code, and those live on the SD card, not in this repository).

---

### Task 7: On-hardware verification

**Files:** none (manual verification against real firmware + real map data)

No code changes — this task confirms Tasks 1-6 work together on real hardware, since none of the SD/rendering integration is exercisable in the native test suite.

- [ ] **Step 1: Flash firmware**

Run: `pio run -e esp32-s3-devkitc-1 -t upload` (match the actual env name in `platformio.ini`)

- [ ] **Step 2: Verify road width**

With the rebuilt map pack (Task 6) on the SD card, open the navigation screen over a known area with both major roads and small local streets. Confirm major roads render visibly thicker than local roads, and paths/cycleways stay thin.

- [ ] **Step 3: Verify the street-name banner**

Ride or simulate movement (e.g. walk with the device, or use a GPS replay if available) along a road known to have an OSM `name` tag. Confirm the header banner (where the route name / "Free ride" text normally shows) switches to the road's name within ~1 second of GPS updating, and updates again as the rider moves onto a different named road.

- [ ] **Step 4: Verify graceful fallback**

Move to (or simulate) a position with no nearby named road (e.g. an unnamed rural track, or an area with no `.ocn` file at all). Confirm the banner falls back to the existing `routeName`/"Free ride" text rather than showing stale or garbage text.

- [ ] **Step 5: Record results**

No commit for this task — if any step fails, file it as a follow-up rather than editing this plan retroactively.

---

## Self-Review

**Spec coverage:** §2 (road width) → Task 1. §3 (`OCN1` format + build tool) → Task 3. §4 (firmware tile cache) → Task 4. §5 (nearest-road lookup + banner) → Tasks 2, 4, 5. §6 (out of scope) → nothing implements these, correctly. §7 milestones 1-7 map 1:1 to Tasks 3, 4, 1, 4, 5, 6, 7.

**Placeholder scan:** no TBD/TODO; every code step has the actual diff content; the two operational tasks (6, 7) have concrete commands/expected outputs rather than vague instructions.

**Type consistency:** `nav::unproject(double,double)` (Task 2) is called with the exact same signature in Task 4's `nearestRoadName`. `nearestRoadName(double,double,char*,size_t)` (Task 4's header declaration) matches its call site in Task 5 exactly (`nearestRoadName(state.lat,state.lon,cachedRoadName,sizeof(cachedRoadName))`). `export_named_columns(db,destination,x0,x1)` (Task 3) matches its test call site and its `build()` integration call site.
