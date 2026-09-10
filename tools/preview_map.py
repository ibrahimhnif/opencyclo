#!/usr/bin/env python3
"""Render a PNG of actual OCP1 road data to inspect conversion, without dependencies."""
import argparse
import math
from pathlib import Path
import struct
import zlib
from build_offline_map import project,clip

def preview(root,output,lat,lon,zoom=15):
    w,h=480,480
    pixels=bytearray(w*h*3)
    cx,cy=project(lon,lat);scale=2**(zoom-14)
    def pixel(x,y,color):
        if 0<=x<w and 0<=y<h:pixels[(y*w+x)*3:(y*w+x)*3+3]=bytes(color)
    def line(ax,ay,bx,by,color):
        cut=clip(ax,ay,bx,by,0,0,w-1,h-1)
        if not cut:return
        ax,ay,bx,by=map(round,cut)
        n=max(abs(bx-ax),abs(by-ay),1)
        for i in range(n+1):pixel(round(ax+(bx-ax)*i/n),round(ay+(by-ay)*i/n),color)
    roads=0
    for tx in range(math.floor((cx-w/2/scale)/256),math.floor((cx+w/2/scale)/256)+1):
        path=Path(root)/'14'/f'{tx}.ocp'
        if not path.exists():continue
        with path.open('rb') as file:
            magic,count=struct.unpack('<4sI',file.read(8))
            if magic!=b'OCP1':raise ValueError('invalid column')
            entries=[struct.unpack('<III',file.read(12)) for _ in range(count)]
            for ty,offset,n in entries:
                if (ty+1)*256<cy-h/2/scale or ty*256>cy+h/2/scale:continue
                file.seek(offset)
                for _ in range(n):
                    a,b,c,d,kind=struct.unpack('<4HB',file.read(9))
                    color=(49,182,107) if kind==2 else (132,130,132) if kind==1 else (66,65,66)
                    line((tx*256+a/256-cx)*scale+w/2,(ty*256+b/256-cy)*scale+h/2,(tx*256+c/256-cx)*scale+w/2,(ty*256+d/256-cy)*scale+h/2,color)
                    roads+=1
    for y in range(h//2-5,h//2+6):
        for x in range(w//2-5,w//2+6):
            if (x-w//2)**2+(y-h//2)**2<=25:pixel(x,y,(0,210,255))
    def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
    raw=b''.join(b'\0'+pixels[y*w*3:(y+1)*w*3] for y in range(h))
    Path(output).write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))
    return roads

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('maps');p.add_argument('output');p.add_argument('--lat',type=float,default=-6.1754);p.add_argument('--lon',type=float,default=106.8272);p.add_argument('--zoom',type=int,default=15)
    a=p.parse_args();print(f'Rendered {preview(a.maps,a.output,a.lat,a.lon,a.zoom)} segments')
