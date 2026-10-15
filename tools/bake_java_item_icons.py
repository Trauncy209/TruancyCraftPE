#!/usr/bin/env python3
import pathlib,json,functools,argparse,zipfile
from PIL import Image
root=pathlib.Path(__file__).resolve().parents[1];assets=root/'java-assets/assets/minecraft';target=root/'original-tcpe'
parser=argparse.ArgumentParser(description='Bake supplied vanilla base item icons and native special-model GUI renders')
parser.add_argument('--official-jar',type=pathlib.Path,help='Authorized local Java1.21.11 client JAR asset overlay')
args=parser.parse_args()
if args.official_jar:
 with zipfile.ZipFile(args.official_jar) as jar:
  for name in jar.namelist():
   if not name.startswith('assets/minecraft/'):continue
   rel=name[len('assets/minecraft/'):]
   if (rel.endswith('.json') and (rel.startswith('models/') or rel.startswith('items/'))) or (rel.endswith('.png') and (rel.startswith('textures/item/') or rel.startswith('textures/block/') or rel.startswith('textures/entity/bed/'))):
    destination=assets/rel;destination.parent.mkdir(parents=True,exist_ok=True);destination.write_bytes(jar.read(name))
states=json.loads((root/'java-assets/java-1.21.11-block-states.json').read_text());state_ids={}
for s in states:
 props=s['properties'];score=sum(props.get(k)==v for k,v in {'axis':'y','half':'bottom','shape':'straight','waterlogged':'false','facing':'north','snowy':'false'}.items())
 if s['name'] not in state_ids or score>state_ids[s['name']][0]:state_ids[s['name']]=(score,s['id'])
def path(s):return s.removeprefix('minecraft:')
@functools.lru_cache(None)
def model(name):
 obj=json.loads((assets/'models'/(path(name)+'.json')).read_text());parent=obj.get('parent','');base={} if parent.startswith('builtin/') or parent in ('item/generated','item/handheld','item/handheld_rod') or not parent else model(parent)
 return {**base,**obj,'textures':{**base.get('textures',{}),**obj.get('textures',{})}}
def choose(node):
 if isinstance(node,str):return node
 if not isinstance(node,dict):return None
 if isinstance(node.get('model'),str):return node['model']
 # GUI display-context cases override the held fallback.
 if node.get('property','').removeprefix('minecraft:')=='display_context':
  for case in node.get('cases',[]):
   when=case.get('when',[]);when=[when] if isinstance(when,str) else when
   if 'gui' in when:return choose(case['model'])
 for key in ['fallback','on_false','model','base']:
  if key in node:
   v=choose(node[key])
   if v:return v
 for key in ['entries','cases','models']:
  for n in node.get(key,[]):
   v=choose(n)
   if v:return v
 return None
icons={};fallback=[];textures={}
for file in sorted((assets/'textures/item').glob('*.png')):textures['item/'+file.stem]=Image.open(file).convert('RGBA');icons[file.stem]=('item/'+file.stem,None)
for file in sorted((assets/'items').glob('*.json')):
 try:
  selected=choose(json.loads(file.read_text())['model'])
  if not selected:raise ValueError('dynamic/builtin model')
  obj=model(selected);texture=obj.get('textures',{}).get('layer0')
  if texture:
   texture=path(texture);seen=set()
   while texture.startswith('#'):
    if texture in seen:raise ValueError('texturecycle')
    seen.add(texture);texture=obj['textures'][texture[1:]]
   image=Image.open(assets/'textures'/(texture+'.png')).convert('RGBA');textures[texture]=image;icons[file.stem]=(texture,None)
  else:
   blockname=path(selected).split('/')[-1]
   for suffix in ['_inventory','_bottom','_top']:
    if blockname.endswith(suffix):blockname=blockname[:-len(suffix)]
   if file.stem in state_ids:blockname=file.stem
   if blockname not in state_ids:raise ValueError('no ordinary block/item geometry')
   icons[file.stem]=(None,state_ids[blockname][1])
 except (OSError,KeyError,ValueError) as error:
  if file.stem not in icons:fallback.append({'item':file.stem,'reason':str(error)})
# Native GUI bakes preserve authored special/block meshes, UVs and base display transforms.
# Sentinel -2 is GUI-only, never a fabricated held/dropped billboard.
special_bakes=[]
for file in sorted((root/'java-assets/baked-special-items').glob('*.png')):
 name='special/'+file.stem;textures[name]=Image.open(file).convert('RGBA');icons[file.stem]=(name,state_ids[file.stem][1] if file.stem in state_ids else -2);special_bakes.append(file.stem)
fallback=[entry for entry in fallback if entry['item'] not in special_bakes]
side=1024;atlas=Image.new('RGBA',(side,side));layout={};x=y=1;row=0
for name,image in sorted(textures.items(),key=lambda p:(-p[1].width,p[0])):
 size=image.width;image=image.crop((0,0,size,size))
 if x+size+1>side:x=1;y+=row;row=0
 if y+size+1>side:raise ValueError('atlasoverflow')
 atlas.paste(image,(x,y));layout[name]=(x,y,size);x+=size+2;row=max(row,size+2)
(root/'java-assets/java-item-atlas-layout.json').write_text(json.dumps({'size':side,'textures':layout},indent=2))
atlas.save(target/'data/images/java-items.png')
for mobile in ['android','android_java']:atlas.save(target/f'project/{mobile}/assets/java-items.png')
with (target/'src/network/java/JavaItemIcons.inc').open('w') as f:
 for name,(texture,state) in sorted(icons.items()):
  if texture:
   x,y,size=layout[texture];uv=[x/side,y/side,(x+size)/side,(y+size)/side]
  else:uv=[0,0,0,0]
  f.write('{"minecraft:'+name+'",{'+','.join(f'{v:.8f}f' for v in uv)+'},'+str(state if state is not None else -1)+'},\n')
report={'flat_provided_textures':len(list((assets/'textures/item').glob('*.png'))),'icon_identifiers':len(icons),'flat_icon_identifiers':sum(v[0] is not None for v in icons.values()),'block_icon_identifiers':sum(v[1] is not None for v in icons.values()),'missing':fallback,'native_special_gui_bakes':special_bakes,'official_jar_overlay':str(args.official_jar) if args.official_jar else None,'item_definition_coverage':{'definitions':len(list((assets/'items').glob('*.json'))),'resolved_identifier_rows':sum(file.stem in icons for file in (assets/'items').glob('*.json'))},'limitations':['Conditional/range item models select static supplied frame.','This base atlas does not itself apply runtime components, glint or server pack overrides.','Block item previews select a canonical registry state and require runtime geometry coverage.','Special GUI bakes represent component-free static defaults; held/dropped geometry, banner patterns and animated special poses are separate.']}
(root/'java-assets/java-item-icon-report.json').write_text(json.dumps(report,indent=2));print(json.dumps({k:v for k,v in report.items() if k!='missing'},indent=2))
