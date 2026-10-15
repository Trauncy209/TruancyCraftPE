#!/usr/bin/env python3
"""Derive Java bitmap metrics from the user's supplied asset providers.
Keeps original TCPE font assets untouched; copies separate Java-only atlases.
"""
from pathlib import Path
import json,math,shutil
from PIL import Image
root=Path(__file__).resolve().parents[1]
source=root/'java-assets/assets/minecraft'
providers=json.loads((source/'font/include/default.json').read_text())['providers']
glyphs={32:(-1,0,0,0,0,1,1,0,0,4),0x200c:(-1,0,0,0,0,1,1,0,0,0)}
targets=[root/'original-tcpe/data/images/java-font',root/'original-tcpe/project/android/assets/java-font']
for atlas,provider in enumerate(providers):
 namespace,file=provider['file'].split(':',1)
 assert namespace=='minecraft'
 path=source/'textures'/file
 image=Image.open(path).convert('RGBA');width,height=image.size
 rows=provider['chars'];columns=len(rows[0]);assert all(len(row)==columns for row in rows)
 assert width%columns==0 and height%len(rows)==0
 cw,ch=width//columns,height//len(rows);scale=provider.get('height',8)/ch;ascent=provider['ascent']
 for target in targets:
  target.mkdir(parents=True,exist_ok=True);image.save(target/f'base-{atlas}.png')
 for row,line in enumerate(rows):
  for column,char in enumerate(line):
   cp=ord(char)
   if cp==0 or cp in glyphs:continue
   x,y=column*cw,row*ch;visible=0
   for px in range(cw-1,-1,-1):
    if any(image.getpixel((x+px,y+py))[3] for py in range(ch)):
     visible=px+1;break
   advance=math.floor(visible*scale+0.5)+1
   glyphs[cp]=(atlas,x,y,cw,ch,width,height,scale,ascent,advance)
output=root/'original-tcpe/src/client/gui/JavaBaseFontGlyphs.inc'
with output.open('w') as stream:
 stream.write('// Generated from user-provided Java 1.21.4 bitmap providers. Original TCPE font untouched.\n')
 for cp,g in sorted(glyphs.items()):
  atlas,x,y,cw,ch,width,height,scale,ascent,advance=g
  stream.write('{0x%x,%d,%d,%d,%d,%d,%d,%d,%.9gf,%.9gf,%.9gf},\n'%(cp,atlas,x,y,cw,ch,width,height,scale,ascent,advance))
# C++ floating literals need decimal points for integral values.
text=output.read_text()
import re
text=re.sub(r'(?<![\w.])(\d+)f',r'\1.0f',text);output.write_text(text)
print(f'Prepared {len(glyphs)} Java bitmap glyphs and three independent atlases')
