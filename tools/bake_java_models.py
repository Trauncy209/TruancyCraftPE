#!/usr/bin/env python3
"""Bake supplied Java JSON block assets into TCPE quads; no uploaded code is executed."""
import argparse,json,math,pathlib,functools,collections
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('--assets',type=pathlib.Path,required=True);p.add_argument('--states',type=pathlib.Path,required=True);p.add_argument('--overlay',type=pathlib.Path);p.add_argument('--preserve-atlas',type=pathlib.Path);p.add_argument('--preserve-layout',type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
root=a.assets/'assets/minecraft'; out=a.output;out.mkdir(parents=True,exist_ok=True)
def asset(relative):
 overlay=a.overlay/'assets/minecraft'/relative if a.overlay else None
 return overlay if overlay is not None and overlay.is_file() else root/relative
def resource(s):
 ns,sep,name=s.partition(':')
 if sep and ns!='minecraft':raise ValueError('unsupported namespace '+ns)
 return name if sep else ns
@functools.lru_cache(None)
def model(name):
 obj=json.loads((asset(pathlib.Path('models')/(resource(name)+'.json'))).read_text()); base={}
 if obj.get('parent') and not obj['parent'].startswith('builtin/'):
  base=model(obj['parent'])
 merged=dict(base);merged.update(obj);merged['textures']={**base.get('textures',{}),**obj.get('textures',{})};return merged
def match(cond,props):
 if 'OR' in cond:return any(match(c,props) for c in cond['OR'])
 if 'AND' in cond:return all(match(c,props) for c in cond['AND'])
 return all(props.get(k) in str(v).split('|') for k,v in cond.items())
def select(obj,props):
 if 'variants' in obj:
  for key,value in obj['variants'].items():
   cond=dict(t.split('=',1) for t in key.split(',') if t)
   if match(cond,props):return [value[0] if isinstance(value,list) else value]
  return []
 return [(v['apply'][0] if isinstance(v['apply'],list) else v['apply']) for v in obj.get('multipart',[]) if 'when' not in v or match(v['when'],props)]
# Counterclockwise quads viewed from outside, matching the original GL_QUADS tesselator.
dirs=['down','up','north','south','west','east'];delta=[(0,-1,0),(0,1,0),(0,0,-1),(0,0,1),(-1,0,0),(1,0,0)]
def corners(f,b,t):
 x,y,z=b;X,Y,Z=t
 return {'down':[(x,y,Z),(x,y,z),(X,y,z),(X,y,Z)],'up':[(x,Y,z),(x,Y,Z),(X,Y,Z),(X,Y,z)],'north':[(X,Y,z),(X,y,z),(x,y,z),(x,Y,z)],'south':[(x,Y,Z),(x,y,Z),(X,y,Z),(X,Y,Z)],'west':[(x,Y,z),(x,y,z),(x,y,Z),(x,Y,Z)],'east':[(X,Y,Z),(X,y,Z),(X,y,z),(X,Y,z)]}[f]
def rotate(v,axis,angle,origin=(8,8,8),rescale=False):
 v=[v[i]-origin[i] for i in range(3)];angle=math.radians(angle);c,s=math.cos(angle),math.sin(angle);i={'x':0,'y':1,'z':2}[axis];j,k=(i+1)%3,(i+2)%3
 v[j],v[k]=v[j]*c-v[k]*s,v[j]*s+v[k]*c
 if rescale:
  v[j]/=abs(c);v[k]/=abs(c)
 return tuple(v[d]+origin[d] for d in range(3))
def variant_rotate(v,part):
 # Java block-state rotations are clockwise around their axes.
 for axis in ['x','y']:
  if part.get(axis):v=rotate(v,axis,-part[axis])
 return v
texture_names=set();failures=collections.Counter();uvlock=0;animated=set();weighted=0
@functools.lru_cache(None)
def bake_part(encoded):
 global uvlock
 part=json.loads(encoded);obj=model(part['model']);quads=[]
 if part.get('uvlock'):uvlock+=1
 for e in obj.get('elements',[]):
  b=e['from'];t=e['to'];rotation=e.get('rotation')
  for face,spec in e.get('faces',{}).items():
   texture=spec['texture'];seen=set()
   while texture.startswith('#'):
    if texture in seen:raise ValueError('cyclic texture')
    seen.add(texture);texture=obj['textures'][texture[1:]]
   texture=resource(texture)
   path=asset(pathlib.Path('textures')/(texture+'.png'))
   if not path.is_file():raise ValueError('missing texture '+texture)
   texture_names.add(texture)
   if path.with_suffix('.png.mcmeta').exists():animated.add(texture)
   vertices=corners(face,b,t)
   if rotation:vertices=[rotate(v,rotation['axis'],rotation['angle'],rotation.get('origin',(8,8,8)),rotation.get('rescale',False)) for v in vertices]
   vertices=[variant_rotate(v,part) for v in vertices]
   uv=spec.get('uv')
   if uv is None:
    x,y,z=b;X,Y,Z=t
    uv={'down':[x,16-Z,X,16-z],'up':[x,z,X,Z],'north':[16-X,16-Y,16-x,16-y],'south':[x,16-Y,X,16-y],'west':[z,16-Y,Z,16-y],'east':[16-Z,16-Y,16-z,16-y]}[face]
   u,v,U,V=uv;uvs=[(u,v),(u,V),(U,V),(U,v)];r=spec.get('rotation',0)//90;uvs=uvs[r:]+uvs[:r]
   cull=-1
   if spec.get('cullface'):
    vec=delta[dirs.index(spec['cullface'])];vec=variant_rotate(tuple(8+d for d in vec),part);vec=tuple(round(d-8) for d in vec)
    if vec in delta:cull=delta.index(vec)
   normal=variant_rotate(tuple(8+d for d in delta[dirs.index(face)]),part);shade=dirs.index(face)
   for index,d in enumerate(delta):
    if tuple(round(q-8) for q in normal)==d:shade=index
   quads.append((tuple(vertices),tuple(uvs),texture,cull,spec.get('tintindex',-1),shade))
 return tuple(quads)
@functools.lru_cache(None)
def transparent(texture):
 image=Image.open(asset(pathlib.Path('textures')/(texture+'.png'))).convert('RGBA');return image.getchannel('A').getextrema()[0]<255
states=json.loads(a.states.read_text());sets=[];intern={};mapping=[];coverage=collections.Counter();fallback=[]
for state in states:
 name=state['name'];props=state['properties'];path=asset(pathlib.Path('blockstates')/(name+'.json'))
 try:
  obj=json.loads(path.read_text());parts=select(obj,props)
  quads=tuple(q for part in parts for q in bake_part(json.dumps(part,sort_keys=True)))
  if not quads and name not in ('air','cave_air','void_air'):raise ValueError('no JSON model geometry')
  layer=2 if any(s in name for s in ('glass','ice','portal','slime','honey')) else (1 if any(transparent(q[2]) for q in quads) else 0)
  key=(quads,layer)
  if key not in intern:intern[key]=len(sets);sets.append(key)
  mapping.append(intern[key]);coverage['direct_states']+=1
 except (OSError,ValueError,KeyError) as error:
  mapping.append(-1);fallback.append({'state':state['id'],'name':name,'reason':str(error)});coverage['translated_states']+=1;failures[str(error)]+=1
# Preserve original source texels and first animation frame; shelf-pack with one-pixel extrusion.
side=2048;atlas=Image.new('RGBA',(side,side));positions={};images={}
if bool(a.preserve_atlas)!=bool(a.preserve_layout):p.error('preserved atlas and layout must be supplied together')
if a.preserve_atlas:
 preserved=json.loads(a.preserve_layout.read_text())
 if preserved['size']!=side:raise ValueError('preserved atlas size mismatch')
 atlas=Image.open(a.preserve_atlas).convert('RGBA')
 if atlas.size!=(side,side):raise ValueError('preserved atlas dimensions mismatch')
 positions={name:tuple(rect) for name,rect in preserved['textures'].items()}

for name in texture_names:
 image=Image.open(asset(pathlib.Path('textures')/(name+'.png'))).convert('RGBA');size=image.width
 images[name]=image.crop((0,0,size,size))
x=1;y=max((y+size+2 for x,y,size in positions.values()),default=1);row_height=0
for name in sorted(images,key=lambda n:(-images[n].width,n)):
 if name in positions:continue
 image=images[name];size=image.width
 if x+size+1>side:x=1;y+=row_height;row_height=0
 if y+size+1>side:raise ValueError('atlas overflow')
 atlas.paste(image,(x,y));atlas.paste(image.crop((0,0,1,size)),(x-1,y));atlas.paste(image.crop((size-1,0,size,size)),(x+size,y));atlas.paste(image.crop((0,0,size,1)),(x,y-1));atlas.paste(image.crop((0,size-1,size,size)),(x,y+size))
 positions[name]=(x,y,size);x+=size+2;row_height=max(row_height,size+2)
(out/'java-atlas-layout.json').write_text(json.dumps({'size':side,'textures':positions},indent=2))
atlas.save(out/'java-terrain.png')
def number(v):
 s=f'{v:.7f}'.rstrip('0').rstrip('.');return (s if '.' in s else s+'.0')+'f'
with (out/'JavaBakedModels.inc').open('w') as f:
 f.write('// Generated from supplied Java assets and exact 1.21.11 block-state registry.\nstatic const JavaBakedQuad javaQuads[]={\n');ranges=[];offset=0
 for quads,layer in sets:
  ranges.append((offset,len(quads),layer));offset+=len(quads)
  for vertices,uvs,texture,cull,tint,shade in quads:
   x,y,size=positions[texture];verts=[n/16 for v in vertices for n in v];uvcoords=[q for u,v in uvs for q in ((x+u*size/16)/side,(y+v*size/16)/side)]
   f.write('{{'+','.join(map(number,verts))+'},{'+','.join(map(number,uvcoords))+'},'+f'{cull},{tint},{shade}'+'},\n')
 f.write('};\nstatic const JavaBakedModel javaModels[]={\n');f.writelines('{%d,%d,%d},\n'%r for r in ranges);f.write('};\nstatic const int javaStateModels[]={\n');f.write(','.join(map(str,mapping)));f.write('\n};\n')
report={**coverage,'registry_states':len(states),'unique_models':len(sets),'quads':offset,'textures':len(texture_names),'atlas_size':side,'source_pixels_preserved':True,'fallback_states':fallback,'limitations':['Weighted model variants currently select first choice.','UV-lock variants currently preserve rotated model UVs.','Animated textures use supplied first frame.','Tint indices use provisional fixed colors until biome packets are forwarded.','Fluid and builtin entity-rendered block models use TCPE translation.','Server resource packs are not yet applied.'],'uvlock_models':uvlock,'animated_textures':sorted(animated)}
(out/'java-model-bake-report.json').write_text(json.dumps(report,indent=2));print(json.dumps({k:v for k,v in report.items() if k not in ('fallback_states','animated_textures')},indent=2))
