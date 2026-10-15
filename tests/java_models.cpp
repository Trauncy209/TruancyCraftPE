// Validate the generated asset data independently of OpenGL/server availability.
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iostream>
struct JavaBakedQuad {float xyz[12],uv[8];int cull,tint,shade;};
struct JavaBakedModel {unsigned offset,count;int layer;};
#include "JavaBakedModels.inc"
int main(){
 const size_t states=sizeof(javaStateModels)/sizeof(*javaStateModels);
 const size_t models=sizeof(javaModels)/sizeof(*javaModels);
 const size_t quads=sizeof(javaQuads)/sizeof(*javaQuads);
 assert(states==29671);size_t direct=0,translated=0;
 for(int index:javaStateModels){if(index<0){++translated;continue;}assert(size_t(index)<models);++direct;}
 for(const auto& m:javaModels){assert(m.offset<=quads&&m.count<=quads-m.offset);assert(m.layer>=0&&m.layer<3);}
 for(const auto& q:javaQuads){assert(q.cull>=-1&&q.cull<6);assert(q.shade>=0&&q.shade<6);for(float v:q.xyz)assert(std::isfinite(v));for(float uv:q.uv)assert(std::isfinite(uv)&&uv>=0&&uv<=1);}
 // Stone is a supplied six-face cube; exterior winding must survive baking.
 const auto& stone=javaModels[javaStateModels[1]];assert(stone.count==6&&stone.layer==0);unsigned mask=0;
 const int normal[6][3]={{0,-1,0},{0,1,0},{0,0,-1},{0,0,1},{-1,0,0},{1,0,0}};
 for(size_t i=0;i<stone.count;++i){const auto& q=javaQuads[stone.offset+i];mask|=1u<<q.cull;float a[3],b[3];for(int d=0;d<3;++d){a[d]=q.xyz[3+d]-q.xyz[d];b[d]=q.xyz[6+d]-q.xyz[d];}float cross[3]={a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};float dot=0;for(int d=0;d<3;++d)dot+=cross[d]*normal[q.cull][d];assert(dot>0);}
 assert(mask==63);assert(javaModels[javaStateModels[562]].layer==2); // glass
 assert(javaStateModels[86]==-1); // water uses the documented fluid translation
 std::cout<<states<<" states, "<<direct<<" direct, "<<translated<<" translated, "<<quads<<" bounded quads: PASS\n";
}
