#include "JavaLevel.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/dimension/Dimension.h"
#include <memory>
#include "world/level/LightLayer.h"
#include "world/entity/Entity.h"
namespace {
struct BlockView{unsigned char tile,data;bool compatible;};
const BlockView palette[]={
#include "JavaBlockPalette.inc"
};
BlockView block(const JavaChunkStore& store,int x,int y,int z){uint16_t state;if(!store.state(x,y,z,state))return {0,0,true};if(state>=sizeof(palette)/sizeof(palette[0]))return {248,0,false};return palette[state];}
class JavaEntityChunk:public LevelChunk {
public:JavaEntityChunk(Level* level,int x,int z):LevelChunk(level,new unsigned char[ChunkBlockCount](),x,z){dontSave=true;createdFromSave=true;}
 ~JavaEntityChunk(){deleteBlockData();}
};
class JavaChunkSource:public ChunkSource {
 Level* level;std::map<std::pair<int,int>,std::unique_ptr<JavaEntityChunk>> chunks;
public:
 explicit JavaChunkSource(Level* level):level(level){}
 bool hasChunk(int x,int z)override{return static_cast<JavaLevel*>(level)->serverChunks().chunk(x,z)!=nullptr;}
 LevelChunk* getChunk(int x,int z)override{auto key=std::make_pair(x,z);auto& chunk=chunks[key];if(!chunk)chunk.reset(new JavaEntityChunk(level,x,z));return chunk.get();}
 LevelChunk* create(int x,int z)override{return getChunk(x,z);}
 void postProcess(ChunkSource*,int,int)override{}
 bool tick()override{return false;}bool shouldSave()override{return false;}
 Biome::MobList getMobsAt(const MobCategory&,int,int,int)override{return Biome::MobList();}
 std::string gatherStats()override{return "Java server";}
};
}
// The fourth Level argument is the generator type, not the storage format.
// Java targeting must use loaded chunks at negative coordinates too.
JavaLevel::JavaLevel(LevelStorage* storage,const JavaChunkStore& initial):MultiPlayerLevel(storage,"Java server",LevelSettings(0,GameType::Survival,LGV_INFINITE),LGV_INFINITE),view(initial),minY(-64),maxY(320){
 isClientSide=true;delete _chunkSource;_chunkSource=new JavaChunkSource(this);
 if(!view.entries().empty()){const auto& chunk=*view.entries().begin()->second;minY=chunk.minY;maxY=chunk.minY+chunk.height;}
}
int JavaLevel::getTile(int x,int y,int z){return block(view,x,y,z).tile;}
EntityList& JavaLevel::getEntities(Entity* except,const AABB& bounds){
 // Server snapshots update bounds independently of legacy chunk ticking. The
 // original chunk entity buckets also stop at Y=128; Java dimensions do not.
 queriedEntities.clear();
 for(Entity* entity:entities)
  if(entity!=except&&!entity->removed&&entity->bb.intersects(bounds))queriedEntities.push_back(entity);
 return queriedEntities;
}
int JavaLevel::getData(int x,int y,int z){return block(view,x,y,z).data;}
int JavaLevel::getRawBrightness(int x,int y,int z){return getRawBrightness(x,y,z,true);}
int JavaLevel::getRawBrightness(int x,int y,int z,bool){return view.rawBrightness(x,y,z,skyDarken);}
int JavaLevel::getBrightness(const LightLayer& layer,int x,int y,int z){uint8_t value=layer.surrounding;view.light(&layer==&LightLayer::Sky,x,y,z,value);return value;}
float JavaLevel::getBrightness(int x,int y,int z){return dimension->brightnessRamp[getRawBrightness(x,y,z)];}
void JavaLevel::synchronize(const JavaChunkStore& chunks){
 if(chunks.version()==view.version())return;
 view=chunks;
 // Bootstrap may precede the first chunk. Learn this dimension's bounds when
 // data arrives, and retain them if its final loaded chunk is later unloaded.
 if(!view.entries().empty()){
  const auto& chunk=*view.entries().begin()->second;
  minY=chunk.minY;maxY=chunk.minY+chunk.height;
 }
}
