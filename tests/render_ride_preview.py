"""Rasterize production UI draw calls using the device's actual bitmap fonts.

Host layout check, not a simulation of touch hardware or display timing.
"""
from pathlib import Path
import re
import struct
import sys
import zlib

root=Path(__file__).resolve().parents[1]
fonts=root/".pio/libdeps/esp32-s3-devkitc-1/LovyanGFX/src/lgfx/Fonts"
tables={}
for index,size in enumerate([9,12,24],1):
    source=(fonts/f"GFXFF/FreeSansBold{size}pt7b.h").read_text()
    bitmap=bytes(int(v,16) for v in re.findall(r"0x([0-9A-Fa-f]{2})",source.split("};")[0]))
    glyphs=[tuple(map(int,g)) for g in re.findall(r"\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(-?\d+),\s*(-?\d+)\s*\}",source)]
    tables[index]=(bitmap,glyphs,max(-g[5] for g in glyphs[:-1]))
source=(fonts/"glcdfont.h").read_text()
source=re.sub(r"/\*.*?\*/|//[^\n]*","",source,flags=re.S)
classic=bytes(int(v,16) for v in re.findall(r"0x([0-9a-fA-F]{2})",source))
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
screens=[];pixels=None;name="";rectangles=[];clip=None
def rgb(c):
    return bytes([((c>>11)&31)*255//31,((c>>5)&63)*255//63,(c&31)*255//31])
def dot(x,y,c):
    if clip and not(clip[0]<=x<clip[0]+clip[2] and clip[1]<=y<clip[1]+clip[3]):return
    assert 0<=x<240 and 0<=y<320,(name,"pixel outside screen",x,y)
    pos=(y*240+x)*3;pixels[pos:pos+3]=c
def png(path,data,w,h):
    def chunk(t,b):return struct.pack(">I",len(b))+t+b+struct.pack(">I",zlib.crc32(t+b))
    raw=b"".join(b"\0"+data[y*w*3:(y+1)*w*3] for y in range(h))
    path.write_bytes(b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",w,h,8,2,0,0,0))+chunk(b"IDAT",zlib.compress(raw))+chunk(b"IEND",b""))
def save():
    if pixels is not None:
        png(out/f"{name}.png",pixels,240,320);screens.append(bytes(pixels))
for line in sys.stdin:
    p=line.rstrip("\n").split("|")
    if p[0]=="SCREEN":
        save();name=p[1];pixels=bytearray(240*320*3);rectangles=[];clip=None
    elif p[0]=="C":clip=tuple(map(int,p[1:]))
    elif p[0]=="U":clip=None
    elif p[0]=="R":
        x,y,w,h,r,c=map(int,p[1:]);c=rgb(c)
        if r>0:rectangles.append((x,y,w,h))
        for py in range(y,y+h):
            for px in range(x,x+w):
                dx=max(x+r-px,0,px-(x+w-1-r));dy=max(y+r-py,0,py-(y+h-1-r))
                if dx*dx+dy*dy<=r*r:dot(px,py,c)
    elif p[0]=="L":
        x,y,xx,yy,c=map(int,p[1:]);c=rgb(c)
        dx,dy=abs(xx-x),-abs(yy-y);sx=1 if x<xx else -1;sy=1 if y<yy else -1;err=dx+dy
        while True:
            dot(x,y,c)
            if x==xx and y==yy:break
            twice=2*err
            if twice>=dy:err+=dy;x+=sx
            if twice<=dx:err+=dx;y+=sy
    elif p[0]=="T":
        font,x,y,c=map(int,p[1:5]);c=rgb(c)
        cell=next((r for r in reversed(rectangles) if r[0]<=x<r[0]+r[2] and r[1]<=y<r[1]+r[3]),None)
        def text_dot(px,py,color):
            if cell:assert cell[0]<=px<cell[0]+cell[2] and cell[1]<=py<cell[1]+cell[3],(name,"text outside button",p[5],px,py,cell)
            dot(px,py,color)
        for ch in p[5]:
            if font==0:
                for col in range(5):
                    bits=classic[ord(ch)*5+col]
                    for row in range(8):
                        if bits&(1<<row):text_dot(x+col,y+row,c)
                x+=6
            else:
                bitmap,glyphs,baseline=tables[font]
                offset,w,h,advance,dx,dy=glyphs[ord(ch)-32]
                for bit in range(w*h):
                    if bitmap[offset+bit//8]&(128>>(bit%8)):
                        text_dot(x+dx+bit%w,y+baseline+dy+bit//w,c)
                x+=advance
save()
sheet_height=((len(screens)+2)//3)*320
sheet=bytearray(720*sheet_height*3)
for i,data in enumerate(screens):
    for y in range(320):
        pos=((i//3*320+y)*720+i%3*240)*3
        sheet[pos:pos+720]=data[y*720:(y+1)*720]
png(out/"all.png",sheet,720,sheet_height)
print(f"Verified {len(screens)} screens with device bitmap fonts: {out/'all.png'}")
