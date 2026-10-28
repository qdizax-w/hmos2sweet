#!/usr/bin/env python3
"""Minimal DTBO image packer (AOSP dt_table format v0)."""
import struct,sys
out,page,dtbs=sys.argv[1],int(sys.argv[2]),sys.argv[3:]
blobs=[open(f,'rb').read() for f in dtbs]
hdr_size,ent_size=32,32
off=hdr_size+ent_size*len(blobs); ents=b''; data=b''
for b in blobs:
    ents+=struct.pack('>IIII4I',len(b),off+len(data),0,0,0,0,0,0); data+=b
total=off+len(data)
hdr=struct.pack('>8I',0xd7b7ab1e,total,hdr_size,ent_size,len(blobs),hdr_size,page,0)
img=hdr+ents+data
open(out,'wb').write(img); print('dtbo.img',len(img),'bytes,',len(blobs),'entries')
