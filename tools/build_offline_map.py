#!/usr/bin/env python3
"""Build OpenCyclo vector tiles from a LOCAL OSM XML extract (no tile scraping).

Usage: python3 tools/build_offline_map.py region.osm output/ --bbox west,south,east,north
Copy output/maps to SD root. Input must cover the entire requested bounding box.
For .osm.pbf, first convert a regional extract with `osmium cat region.osm.pbf -o region.osm`.
"""
import argparse
import json
import math
from pathlib import Path
import sqlite3
import struct
import tempfile
import xml.etree.ElementTree as ET

WORLD = 4194304


def project(lon, lat):
    a = math.radians(max(-85, min(85, lat)))
    return (lon + 180) / 360 * WORLD, (1 - math.log(math.tan(a) + 1 / math.cos(a)) / math.pi) / 2 * WORLD


def clip(ax, ay, bx, by, left, top, right, bottom):
    """Liang-Barsky: do not invent links or duplicate unclipped long segments."""
    dx, dy = bx - ax, by - ay
    lo, hi = 0.0, 1.0
    for p, q in ((-dx, ax-left), (dx, right-ax), (-dy, ay-top), (dy, bottom-ay)):
        if p == 0:
            if q < 0:
                return None
        elif p < 0:
            lo = max(lo, q/p)
        else:
            hi = min(hi, q/p)
        if lo > hi:
            return None
    return ax+lo*dx, ay+lo*dy, ax+hi*dx, ay+hi*dy


def build(source, destination, bbox):
    west, south, east, north = bbox
    if not (-180 <= west < east <= 180 and -85 <= south < north <= 85):
        raise ValueError('Invalid bbox, or unsupported antimeridian crossing')
    left, bottom = project(west, south)
    right, top = project(east, north)
    x0, x1 = math.floor(left/256), math.floor(right/256)
    y0, y1 = math.floor(top/256), math.floor(bottom/256)
    if (x1-x0+1)*(y1-y0+1) > 20000:
        raise ValueError('Area exceeds 20,000 tiles; build smaller regions')
    target = Path(destination) / 'maps'
    # A new output directory makes failed builds distinguishable from complete packs.
    if target.exists():
        raise ValueError(f'{target} already exists; choose a fresh output directory')
    with tempfile.TemporaryDirectory(prefix='opencyclo-map-') as tmp:
        db = sqlite3.connect(str(Path(tmp)/'nodes.db'))
        db.execute('CREATE TABLE nodes (id INTEGER PRIMARY KEY, x REAL, y REAL)')
        db.execute('CREATE TABLE segments (tx INTEGER, ty INTEGER, data BLOB)')
        # Two streaming passes handle extracts with ways before nodes too.
        for _, e in ET.iterparse(source, events=('end',)):
            if e.tag == 'node':
                lon, lat = float(e.attrib['lon']), float(e.attrib['lat'])
                if not math.isfinite(lon) or not math.isfinite(lat):
                    raise ValueError('Non-finite coordinate')
                x, y = project(lon, lat)
                db.execute('INSERT OR REPLACE INTO nodes VALUES (?,?,?)', (int(e.attrib['id']),x,y))
            if e.tag in ('node', 'way', 'relation'):
                e.clear()
        db.commit()
        count = 0
        for _, e in ET.iterparse(source, events=('end',)):
            if e.tag == 'way':
                tags = {t.attrib['k']:t.attrib['v'] for t in e.findall('tag')}
                highway = tags.get('highway')
                if highway and highway not in ('proposed', 'construction'):
                    style = 2 if highway in ('cycleway','path','track','footway') else 1 if highway in ('motorway','trunk','primary','secondary','tertiary') else 0
                    previous = None
                    for n in e.findall('nd'):
                        row = db.execute('SELECT x,y FROM nodes WHERE id=?',(int(n.attrib['ref']),)).fetchone()
                        if previous and row:
                            segment = clip(*previous,*row,left,top,right,bottom)
                            if segment:
                                ax,ay,bx,by=segment
                                for tx in range(math.floor(min(ax,bx)/256),math.floor(max(ax,bx)/256)+1):
                                    for ty in range(math.floor(min(ay,by)/256),math.floor(max(ay,by)/256)+1):
                                        cut=clip(ax,ay,bx,by,tx*256,ty*256,(tx+1)*256,(ty+1)*256)
                                        if cut:
                                            coords=[max(0,min(65535,round((v-(tx if i%2==0 else ty)*256)*256))) for i,v in enumerate(cut)]
                                            db.execute('INSERT INTO segments VALUES (?,?,?)',(tx,ty,struct.pack('<4HB',*coords,style)))
                                            count+=1
                        previous=row
            if e.tag in ('node','way','relation'):
                e.clear()
        db.execute('CREATE INDEX tile ON segments(tx,ty)')
        db.commit()
        # Include empty tiles inside coverage so an empty rural tile is not
        # confused with a missing map. Boundary tiles outside bbox stay partial.
        for tx in range(x0,x1+1):
            folder=target/'14'/str(tx)
            folder.mkdir(parents=True,exist_ok=True)
            for ty in range(y0,y1+1):
                rows=db.execute('SELECT data FROM segments WHERE tx=? AND ty=?',(tx,ty)).fetchall()
                if len(rows)>20000:
                    raise ValueError(f'Tile {tx}/{ty} exceeds device limit; reduce road detail')
                (folder/f'{ty}.ocm').write_bytes(struct.pack('<4sI',b'OCM1',len(rows))+b''.join(r[0] for r in rows))
        db.close()
    manifest={'format':'OCM1','zoom':14,'bbox':bbox,'segments':count,'attribution':'© OpenStreetMap contributors','license':'https://www.openstreetmap.org/copyright','source':Path(source).name}
    (target/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    (target/'ATTRIBUTION.txt').write_text('Map data © OpenStreetMap contributors\nhttps://www.openstreetmap.org/copyright\nOpen Database License (ODbL)\n')
    return manifest


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path)
    parser.add_argument('destination',type=Path)
    parser.add_argument('--bbox',required=True,help='west,south,east,north, all inside your extract')
    args=parser.parse_args()
    print(json.dumps(build(args.source,args.destination,[float(v) for v in args.bbox.split(',')]),indent=2))
