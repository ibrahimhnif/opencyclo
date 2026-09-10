#!/usr/bin/env python3
"""Validate every column, tile offset, length and checksum of a completed pack."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def verify(root):
    root=Path(root);manifest=json.loads((root/'manifest.json').read_text())
    coverage=(root/'coverage.bin').read_bytes()
    if len(coverage)!=36 or coverage[:4]!=b'OCB1':raise ValueError('invalid coverage')
    total=segments=tiles=0
    for name,digest in manifest['sha256'].items():
        path=root/name
        if path.suffix!='.ocp' or path.parent!=root/'14':raise ValueError('invalid column path')
        data=path.read_bytes();total+=len(data)
        if hashlib.sha256(data).hexdigest()!=digest:raise ValueError(f'checksum mismatch: {name}')
        magic,count=struct.unpack_from('<4sI',data)
        if magic!=b'OCP1' or count>16384:raise ValueError(f'invalid column: {name}')
        last=-1;expected=8+count*12
        for i in range(count):
            y,offset,n=struct.unpack_from('<III',data,8+i*12)
            if y<=last or y>=16384 or n>20000 or offset!=expected:raise ValueError(f'invalid tile: {name}/{y}')
            expected+=n*9;last=y;segments+=n;tiles+=1
        if expected!=len(data):raise ValueError(f'invalid length: {name}')
    if segments!=manifest['segments'] or tiles!=manifest['populated_tiles']:raise ValueError('manifest counts differ')
    return {'columns':len(manifest['sha256']),'tiles':tiles,'segments':segments,'column_bytes':total,'verified':True}

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('maps',type=Path)
    print(json.dumps(verify(parser.parse_args().maps),indent=2))
