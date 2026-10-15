#include "JavaChunkStore.h"
#include <limits>
#include <algorithm>
namespace {
bool validShape(const TcpeJavaEvent& event){return event.height>0&&event.height<=4096&&static_cast<int64_t>(event.min_y)+event.height<=std::numeric_limits<int>::max();}
bool hasLights(const TcpeJavaEvent& event){return event.light_sections_len||event.sky_light_len||event.block_light_len;}
bool mergeLights(JavaChunkStore::Chunk& chunk,const TcpeJavaEvent& event){
 if(!hasLights(event))return true;
 const size_t count=(event.height+15)/16,bytes=static_cast<size_t>(event.height)*128;
 if(!event.light_sections||event.light_sections_len!=count)return false;
 bool sky=false,block=false;
 for(size_t s=0;s<count;++s){if(event.light_sections[s]&~3u)return false;sky|=(event.light_sections[s]&1)!=0;block|=(event.light_sections[s]&2)!=0;}
 if(sky&&(!event.sky_light||event.sky_light_len!=bytes))return false;
 if(block&&(!event.block_light||event.block_light_len!=bytes))return false;
 if((event.sky_light_len&&(!event.sky_light||event.sky_light_len!=bytes))||(event.block_light_len&&(!event.block_light||event.block_light_len!=bytes)))return false;
 if(chunk.lightSections.size()!=count){chunk.lightSections.assign(count,0);chunk.skyLight.assign(bytes,0);chunk.blockLight.assign(bytes,0);}
 for(size_t s=0;s<count;++s){const size_t begin=s*2048,end=std::min(begin+2048,bytes);uint8_t flags=event.light_sections[s];
  if(flags&1)std::copy(event.sky_light+begin,event.sky_light+end,chunk.skyLight.begin()+begin);
  if(flags&2)std::copy(event.block_light+begin,event.block_light+end,chunk.blockLight.begin()+begin);
  chunk.lightSections[s]|=flags;
 }
 return true;
}
int floorChunk(int v){return v/16-(v%16<0);}
int local(int v){return v%16<0?v%16+16:v%16;}
}
bool JavaChunkStore::apply(const TcpeJavaEvent& event){
 if(event.kind!=TCPE_JAVA_CHUNK||!event.states||!validShape(event))return false;
 if(event.states_len!=static_cast<size_t>(event.height)*256)return false;
 Chunk updated;updated.minY=event.min_y;updated.height=static_cast<int>(event.height);updated.states.assign(event.states,event.states+event.states_len);
 if(const auto* previous=chunk(event.x,event.z))if(previous->minY==updated.minY&&previous->height==updated.height){updated.skyLight=previous->skyLight;updated.blockLight=previous->blockLight;updated.lightSections=previous->lightSections;}
 if(!mergeLights(updated,event))return false;
 chunks[std::make_pair(event.x,event.z)]=std::make_shared<Chunk>(std::move(updated));++revision;return true;
}
bool JavaChunkStore::applyLight(const TcpeJavaEvent& event){
 if(event.kind!=TCPE_JAVA_LIGHT||!validShape(event)||!hasLights(event))return false;
 const auto* previous=chunk(event.x,event.z);if(!previous||previous->minY!=event.min_y||previous->height!=static_cast<int>(event.height))return false;
 Chunk updated=*previous;if(!mergeLights(updated,event))return false;
 if(updated.lightSections==previous->lightSections&&updated.skyLight==previous->skyLight&&updated.blockLight==previous->blockLight)return true;
 chunks[std::make_pair(event.x,event.z)]=std::make_shared<Chunk>(std::move(updated));++revision;return true;
}
const JavaChunkStore::Chunk* JavaChunkStore::chunk(int x,int z)const{auto found=chunks.find(std::make_pair(x,z));return found==chunks.end()?nullptr:found->second.get();}
bool JavaChunkStore::state(int x,int y,int z,uint16_t& result)const{
 const auto* found=chunk(floorChunk(x),floorChunk(z));if(!found)return false;
 const int64_t relative=static_cast<int64_t>(y)-found->minY;if(relative<0||relative>=found->height)return false;
 result=found->states[(static_cast<size_t>(local(x))*16+local(z))*found->height+static_cast<size_t>(relative)];return true;
}
bool JavaChunkStore::light(bool sky,int x,int y,int z,uint8_t& result)const{
 const auto* found=chunk(floorChunk(x),floorChunk(z));if(!found)return false;
 const int64_t relative=static_cast<int64_t>(y)-found->minY;if(relative<0||relative>=found->height)return false;
 const size_t section=static_cast<size_t>(relative)/16;
 if(section>=found->lightSections.size()||!(found->lightSections[section]&(sky?1:2)))return false;
 const auto& plane=sky?found->skyLight:found->blockLight;
 const size_t index=(static_cast<size_t>(relative)*16+local(z))*16+local(x);
 if(index/2>=plane.size())return false;
 result=(plane[index/2]>>((index&1)*4))&15;return true;
}
int JavaChunkStore::rawBrightness(int x,int y,int z,int skyDarken)const{
 const auto* column=chunk(floorChunk(x),floorChunk(z));
 if(column&&static_cast<int64_t>(y)<column->minY)return 0;
 uint8_t sky=15,block=0;light(true,x,y,z,sky);light(false,x,y,z,block);
 return std::max(int(block),std::max(0,int(sky)-std::max(0,std::min(15,skyDarken))));
}
