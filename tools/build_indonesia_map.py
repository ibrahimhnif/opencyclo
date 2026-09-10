#!/usr/bin/env python3
"""Stream a Geofabrik PBF into sparse, indexed OpenCyclo map columns.

pip install -r tools/map_requirements.txt
python tools/build_indonesia_map.py indonesia.osm.pbf output
The output/maps directory is copied to the SD root. No public tile scraping.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sqlite3
import struct
import tempfile
import time
from build_offline_map import project, clip


def export_columns(db, destination, bbox):
    west,south,east,north=bbox
    left,bottom=project(west,south);right,top=project(east,north)
    target=Path(destination)/'maps'
    target.mkdir(parents=True,exist_ok=False)
    (target/'14').mkdir()
    segments=tiles=0
    x0,x1=math.floor(left/256),math.floor(right/256)
    for tx in range(x0,x1+1):
        # One column at a time: bounded RAM even for an entire country.
        groups={}
        for ty,blob in db.execute('SELECT ty,data FROM segments WHERE tx=? ORDER BY ty',(tx,)):
            groups.setdefault(ty,bytearray()).extend(blob)
        entries=[];offset=8+len(groups)*12
        for ty,data in sorted(groups.items()):
            count=len(data)//9
            if count>20000: raise ValueError(f'tile {tx}/{ty} exceeds 20,000 segments')
            entries.append((ty,offset,count));offset+=len(data)
            segments+=count;tiles+=1
        if offset>=2**32: raise ValueError('column exceeds FAT32 limit')
        with (target/'14'/f'{tx}.ocp').open('wb') as out:
            out.write(struct.pack('<4sI',b'OCP1',len(entries)))
            for entry in entries:out.write(struct.pack('<III',*entry))
            for _,data in sorted(groups.items()):out.write(data)
    # Written last: absent coverage means an incomplete pack, never a blank map.
    (target/'coverage.bin').write_bytes(struct.pack('<4s4d',b'OCB1',left,top,right,bottom))
    return {'segments':segments,'populated_tiles':tiles,'columns':x1-x0+1}


def build(source,destination,bbox):
    import osmium
    west,south,east,north=bbox
    if not (-180<=west<east<=180 and -85<=south<north<=85):raise ValueError('invalid bbox')
    if (Path(destination)/'maps').exists():raise ValueError('choose a fresh output directory')
    left,bottom=project(west,south);right,top=project(east,north)
    started=time.monotonic()
    with tempfile.TemporaryDirectory(prefix='opencyclo-pbf-') as tmp:
        db=sqlite3.connect(str(Path(tmp)/'segments.db'))
        db.execute('PRAGMA journal_mode=OFF');db.execute('PRAGMA synchronous=OFF')
        db.execute('CREATE TABLE segments(tx INTEGER,ty INTEGER,data BLOB)')
        class Roads(osmium.SimpleHandler):
            ways=0
            pending=[]
            def way(self,w):
                road=w.tags.get('highway')
                if not road or road in ('proposed','construction'):return
                style=2 if road in ('cycleway','path','track','footway') else 1 if road in ('motorway','trunk','primary','secondary','tertiary') else 0
                grouped={};previous=None
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
                    previous=point
                self.pending.extend((x,y,bytes(data)) for (x,y),data in grouped.items())
                if len(self.pending)>10000:self.flush()
                self.ways+=1
                if self.ways%100000==0:print(f'{self.ways:,} roads, {time.monotonic()-started:.0f}s',flush=True)
            def flush(self):
                db.executemany('INSERT INTO segments VALUES (?,?,?)',self.pending);self.pending.clear();db.commit()
        roads=Roads()
        roads.apply_file(str(source),locations=True,idx=f'sparse_file_array,{tmp}/locations')
        roads.flush()
        print('Indexing tile columns...',flush=True)
        db.execute('CREATE INDEX tile ON segments(tx,ty)');db.commit()
        report=export_columns(db,destination,bbox);db.close()
    target=Path(destination)/'maps'
    report.update(format='OCP1',zoom=14,bbox=bbox,source=Path(source).name,attribution='© OpenStreetMap contributors',license='https://www.openstreetmap.org/copyright')
    (target/'ATTRIBUTION.txt').write_text('Map data © OpenStreetMap contributors\nhttps://www.openstreetmap.org/copyright\nOpen Database License (ODbL)\n')
    report['bytes']=sum(p.stat().st_size for p in target.rglob('*') if p.is_file())
    report['sha256']={str(p.relative_to(target)):hashlib.sha256(p.read_bytes()).hexdigest() for p in (target/'14').glob('*.ocp')}
    (target/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='sha256'},indent=2),flush=True)
    return report


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path);parser.add_argument('destination',type=Path)
    parser.add_argument('--bbox',default='94,-12,142,7')
    args=parser.parse_args()
    build(args.source,args.destination,[float(x) for x in args.bbox.split(',')])
