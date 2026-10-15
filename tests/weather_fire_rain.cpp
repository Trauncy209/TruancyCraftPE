#include "world/level/Level.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/level/biome/BiomeSource.h"
#include "world/level/dimension/Dimension.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/FireTile.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/chunk/EmptyLevelChunk.h"
#include <map>
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include <cassert>
#include <iostream>
struct Climate:BiomeSource{Biome* selected=Biome::plains;Biome* getBiome(int,int)override{return selected;}};
struct BlankSource:ChunkSource{
 Level* level;EmptyLevelChunk* border=nullptr;std::map<std::pair<int,int>,LevelChunk*> chunks;explicit BlankSource(Level* l):level(l){}
 bool hasChunk(int x,int z)override{return chunks.count({x,z});}
 LevelChunk* getChunk(int x,int z)override{if(x<0||z<0||x>=16||z>=16){if(!border)border=new EmptyLevelChunk(level,x,z);return border;}assert(hasChunk(x,z));return chunks[{x,z}];}
 LevelChunk* create(int x,int z)override{auto* c=new LevelChunk(level,new unsigned char[32768](),x,z);chunks[{x,z}]=c;return c;}
 void postProcess(ChunkSource*,int,int)override{} bool tick()override{return false;}bool shouldSave()override{return false;}
 Biome::MobList getMobsAt(const MobCategory&,int,int,int)override{return {};}
 std::string gatherStats()override{return "isolated blank fire fixture";}
};
struct FireWorld:Level{using Level::Level;bool java=false;bool isJavaServerLevel()const override{return java;}
 void useBlank(BlankSource* source){delete _chunkSource;_chunkSource=source;}
};
int main(){
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
 auto* level=new FireWorld(new MemoryLevelStorage(),"rain-fire",LevelSettings(123,GameType::Survival,LGV_ORIGINAL),LGV_ORIGINAL);level->setUpdateLights(false);
 auto* blank=new BlankSource(level);level->useBlank(blank);
 for(int z=1;z<=5;++z)for(int x=1;x<=5;++x)blank->create(x,z);
 auto* climate=new Climate();level->dimension->biomeSource=climate;
 const int x=40,y=100;int z=37;auto& weather=level->getLevelData()->weather;weather.rain=1;
 auto setup=[&](int roof){
  z+=3; // Isolate each scheduled queue from earlier test fires.
  level->java=false;level->isClientSide=false;climate->selected=Biome::plains;weather.rain=1;
  for(int xx=x-2;xx<=x+2;++xx)for(int zz=z-2;zz<=z+2;++zz){level->setTileNoUpdate(xx,y-1,zz,Tile::rock->id);for(int yy=y;yy<128;++yy)level->setTileNoUpdate(xx,yy,zz,0);}
  if(roof==1)level->setTileNoUpdate(x,y+4,z,Tile::rock->id);
  if(roof==2)for(int xx=x-1;xx<=x+1;++xx)for(int zz=z-1;zz<=z+1;++zz)level->setTileNoUpdate(xx,y+4,zz,Tile::rock->id);
  level->setTile(x,y,z,Tile::fire->id);level->setDataNoUpdate(x,y,z,0);
 };
 auto scheduled=[&]{for(int i=0;i<11;++i){level->setTime(level->getTime()+1);level->tickPendingTicks(false);}};
 setup(0);scheduled();assert(level->getTile(x,y,z)==0);
 setup(1);scheduled();assert(level->getTile(x,y,z)==0); // Open side reaches sheltered center.
 setup(2);scheduled();assert(level->getTile(x,y,z)==Tile::fire->id);
 setup(0);climate->selected=Biome::desert;scheduled();assert(level->getTile(x,y,z)==Tile::fire->id);
 setup(0);climate->selected=Biome::tundra;scheduled();assert(level->getTile(x,y,z)==Tile::fire->id);
 setup(0);level->isClientSide=true;scheduled();assert(level->getTile(x,y,z)==Tile::fire->id);
 setup(0);level->java=true;scheduled();assert(level->getTile(x,y,z)==Tile::fire->id);
 setup(0);weather.rain=0;scheduled();assert(level->getTile(x,y,z)==Tile::fire->id);
 setup(0);level->dimension->hasCeiling=true;scheduled();assert(level->getTile(x,y,z)==Tile::fire->id);level->dimension->hasCeiling=false;
 // Netherrack protects fire from rain and natural age exhaustion; ongoing
 // scheduled ticks must still notice when its support changes later.
 setup(0);level->setTileNoUpdate(x,y-1,z,Tile::netherrack->id);
 for(int n=0;n<25;++n)scheduled();
 assert(level->getTile(x,y,z)==Tile::fire->id&&level->getData(x,y,z)==15);
 level->setTileNoUpdate(x,y-1,z,Tile::rock->id);scheduled();assert(level->getTile(x,y,z)==0);
 // Actual queued rain-fire callbacks run at every finite edge/corner without
 // requiring a loaded chunk outside 0..15. Fixture border reads mirror TCPE's
 // immutable EmptyLevelChunk; no outside chunk is registered or generated.
 for(auto p:{std::pair<int,int>{0,40},{255,40},{40,0},{40,255},{0,0},{255,255}}){
  const int fx=p.first,fz=p.second;
  for(int cz=std::max(0,(fz-8)/16);cz<=std::min(15,(fz+8)/16);++cz)
   for(int cx=std::max(0,(fx-8)/16);cx<=std::min(15,(fx+8)/16);++cx)
    if(!blank->hasChunk(cx,cz))blank->create(cx,cz);
  level->setTileNoUpdate(fx,y-1,fz,Tile::rock->id);
  level->setTile(fx,y,fz,Tile::fire->id);scheduled();assert(level->getTile(fx,y,fz)==0);
 }
 for(auto c:blank->chunks)assert(c.first.first>=0&&c.first.first<16&&c.first.second>=0&&c.first.second<16);
 level->java=true;level->setTile(0,y,40,Tile::fire->id);scheduled();assert(level->getTile(0,y,40)==Tile::fire->id);level->java=false;
 level->setTileNoUpdate(0,y,40,0);level->getLevelData()->setGeneratorVersion(LGV_INFINITE);
 level->setTile(0,y,40,Tile::fire->id);scheduled();assert(level->getTile(0,y,40)==Tile::fire->id);level->getLevelData()->setGeneratorVersion(LGV_ORIGINAL);
 std::cout<<"Netherrack remains through age15/rain, support removal extinguishes; all finite edges/corners execute without outside chunks; Java/infinite loading guards unchanged PASS\n";
 // A scheduled tick at an unloaded seam is skipped by the Level scheduler.
 setup(0);blank->create(6,2);blank->create(6,3);
 level->setTileNoUpdate(95,y-1,40,Tile::rock->id);level->setTile(95,y,40,Tile::fire->id);
 blank->chunks.erase({6,2});blank->chunks.erase({6,3});
 assert(!level->hasChunkNow(6,2));scheduled();assert(!level->hasChunkNow(6,2)&&level->getTile(95,y,40)==Tile::fire->id);
 std::cout<<"Actual scheduled fire rain: exposed/cardinal extinguish; full roof/dry/cold/clear/client/Java retain; unloaded seam skipped without generation PASS\n";
}
