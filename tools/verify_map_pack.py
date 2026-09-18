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
    columns=named_columns=named_segments=named_tiles=0
    for name,digest in manifest['sha256'].items():
        path=root/name
        if path.suffix not in ('.ocp','.ocn') or path.parent!=root/'14':raise ValueError('invalid column path')
        data=path.read_bytes();total+=len(data)
        if hashlib.sha256(data).hexdigest()!=digest:raise ValueError(f'checksum mismatch: {name}')
        if path.suffix=='.ocn':
            named_columns+=1
            if len(data)<12:raise ValueError(f'invalid named column: {name}')
            magic,index_count,pool_size=struct.unpack_from('<4sII',data)
            if magic!=b'OCN1' or index_count>16384 or pool_size>65535:raise ValueError(f'invalid named column: {name}')
            pool_start=12+index_count*12
            if pool_start+pool_size>len(data):raise ValueError(f'invalid named length: {name}')
            last=-1;expected=0;rows=[]
            for i in range(index_count):
                y,offset,n=struct.unpack_from('<III',data,12+i*12)
                if y<=last or y>=16384 or n>20000 or offset!=expected:raise ValueError(f'invalid named tile: {name}/{y}')
                if pool_start+pool_size+offset+n*10>len(data):raise ValueError(f'invalid named tile: {name}/{y}')
                expected+=n*10;last=y;named_segments+=n;named_tiles+=1;rows.append((offset,n))
            if pool_start+pool_size+expected!=len(data):raise ValueError(f'invalid named length: {name}')
            if pool_size and data[pool_start+pool_size-1]!=0:raise ValueError(f'unterminated name pool: {name}')
            for offset,n in rows:
                base=pool_start+pool_size+offset
                for j in range(n):
                    name_offset,=struct.unpack_from('<H',data,base+j*10+8)
                    if name_offset>=pool_size:raise ValueError(f'invalid name offset: {name}')
            continue
        columns+=1
        magic,count=struct.unpack_from('<4sI',data)
        if magic!=b'OCP1' or count>16384:raise ValueError(f'invalid column: {name}')
        last=-1;expected=8+count*12
        for i in range(count):
            y,offset,n=struct.unpack_from('<III',data,8+i*12)
            if y<=last or y>=16384 or n>20000 or offset!=expected:raise ValueError(f'invalid tile: {name}/{y}')
            expected+=n*9;last=y;segments+=n;tiles+=1
        if expected!=len(data):raise ValueError(f'invalid length: {name}')
    if segments!=manifest['segments'] or tiles!=manifest['populated_tiles']:raise ValueError('manifest counts differ')
    if named_segments!=manifest.get('named_segments',0) or named_tiles!=manifest.get('named_tiles',0):
        raise ValueError('manifest named counts differ')
    return {'columns':columns,'tiles':tiles,'segments':segments,'column_bytes':total,
            'named_columns':named_columns,'named_tiles':named_tiles,'named_segments':named_segments,'verified':True}

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('maps',type=Path)
    print(json.dumps(verify(parser.parse_args().maps),indent=2))
