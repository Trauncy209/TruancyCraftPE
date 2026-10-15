#include "JavaBlockModels.h"
#include "JavaLevel.h"
#include "client/renderer/Tesselator.h"
#include <cstddef>
#include <cmath>
#include <algorithm>
struct JavaBakedQuad {float xyz[12],uv[8];int cull,tint,shade;};
struct JavaBakedModel {unsigned offset,count;int layer;};
#include "JavaBakedModels.inc"
namespace {
const JavaBakedModel* model(uint16_t state){
 if(state>=sizeof(javaStateModels)/sizeof(javaStateModels[0]))return nullptr;
 int index=javaStateModels[state];return index<0?nullptr:&javaModels[index];
}
const int dx[]={0,0,0,0,-1,1},dy[]={-1,1,0,0,0,0},dz[]={0,0,-1,1,0,0};
}
int JavaBlockModels::layer(uint16_t state){const auto* m=model(state);return m?m->layer:-1;}
bool JavaBlockModels::fullCube(uint16_t state){
 const auto* m=model(state);if(!m||m->layer!=0||m->count!=6)return false;
 unsigned mask=0;
 for(unsigned i=0;i<m->count;++i){const auto& q=javaQuads[m->offset+i];if(q.cull<0)return false;mask|=1u<<q.cull;
  for(float v:q.xyz)if(v!=0.0f&&v!=1.0f)return false;
 }
 return mask==63;
}
bool JavaBlockModels::tesselate(JavaLevel& level,Tesselator& t,int x,int y,int z,uint16_t state){
 const auto* m=model(state);if(!m)return false;bool emitted=false;
 const float shades[]={.5f,1.f,.8f,.8f,.6f,.6f};
 for(unsigned i=0;i<m->count;++i){const auto& q=javaQuads[m->offset+i];
  if(q.cull>=0){uint16_t adjacent;if(level.serverChunks().state(x+dx[q.cull],y+dy[q.cull],z+dz[q.cull],adjacent)){
   if(fullCube(adjacent)||(adjacent==state&&m->layer==2))continue;
  }}
  // Solid-block centers contain zero server light: shade from the visible neighboring cell.
  float brightness=level.getBrightness(x+dx[q.shade],y+dy[q.shade],z+dz[q.shade]);
  if(q.cull<0){float center=level.getBrightness(x,y,z);if(center>brightness)brightness=center;}
  float light=brightness*shades[q.shade];
  // Use provisional vanilla grass tint until server biome data is forwarded.
  if(q.tint>=0)t.color(light*.57f,light*.74f,light*.32f);else t.color(light,light,light);
  for(int v=0;v<4;++v)t.vertexUV(x+q.xyz[v*3],y+q.xyz[v*3+1],z+q.xyz[v*3+2],q.uv[v*2],q.uv[v*2+1]);
  emitted=true;
 }
 return emitted;
}

const char* JavaBlockModels::atlasTexture(){return "java-terrain.png";}
bool JavaBlockModels::tesselateDisplay(Tesselator& t,uint16_t state,float brightness){
 const auto* m=model(state);if(!m||!m->count)return false;
 brightness=std::isfinite(brightness)?std::max(0.f,std::min(1.f,brightness)):1.f;
 const float shades[]={.5f,1.f,.8f,.8f,.6f,.6f};
 for(unsigned i=0;i<m->count;++i){const auto& q=javaQuads[m->offset+i];float light=brightness*shades[q.shade];
  if(q.tint>=0)t.color(light*.57f,light*.74f,light*.32f);else t.color(light,light,light);
  for(int v=0;v<4;++v)t.vertexUV(q.xyz[v*3],q.xyz[v*3+1],q.xyz[v*3+2],q.uv[v*2],q.uv[v*2+1]);
 }
 return true;
}
