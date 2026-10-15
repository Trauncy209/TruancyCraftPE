from pathlib import Path
import json,re
from PIL import Image
root=Path(__file__).resolve().parents[1];atlas=Image.open(root/'original-tcpe/data/images/java-items.png').convert('RGBA');rows={}
for line in (root/'original-tcpe/src/network/java/JavaItemIcons.inc').read_text().splitlines():
 m=re.match(r'\{"minecraft:([^\"]+)",\{([^}]+)\}',line)
 if m:rows[m[1]]=[float(n.rstrip('f')) for n in m[2].split(',')]
for material in ['wooden','stone','copper','iron','golden','diamond','netherite']:
 name=material+'_spear';uv=rows[name];handuv=rows[name+'_in_hand'];assert uv!=handuv,(name,'GUI incorrectly chooses hand context')
 pixels=tuple(round(v*atlas.width) for v in uv);got=atlas.crop(pixels);expected=Image.open(root/'java-assets/assets/minecraft/textures/item'/ (name+'.png')).convert('RGBA');expected=expected.crop((0,0,expected.width,expected.width));assert got.size==expected.size and got.tobytes()==expected.tobytes(),name
print('All7 spear GUI atlas crops equal authored GUI texture pixels and differ from held-context UVs PASS')
