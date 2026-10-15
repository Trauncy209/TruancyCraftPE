#include "world/level/Level.h"
#include "world/level/Region.h"
#include "world/level/chunk/ChunkCache.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include <cassert>
#include <iostream>
struct Counts{int generated=0,populated=0,loaded=0,saved=0,ticked=0;};
struct Storage:ChunkStorage{Counts& c;Storage(Counts& c):c(c){}LevelChunk* load(Level*,int,int)override{++c.loaded;return nullptr;}void save(Level*,LevelChunk*)override{++c.saved;}void tick()override{++c.ticked;}};
struct Generator:ChunkSource{Level* level;Counts& c;Generator(Level* l,Counts& c):level(l),c(c){}bool hasChunk(int,int)override{return true;}LevelChunk* getChunk(int x,int z)override{++c.generated;auto*b=new unsigned char[LevelChunk::ChunkBlockCount]();b[0]=Tile::rock->id;return new LevelChunk(level,b,x,z);}LevelChunk* create(int x,int z)override{return getChunk(x,z);}void postProcess(ChunkSource* parent,int x,int z)override{++c.populated;parent->getChunk(x,z)->setTile(1,1,1,Tile::dirt->id);}bool tick()override{++c.ticked;return false;}bool shouldSave()override{return true;}Biome::MobList getMobsAt(const MobCategory&,int,int,int)override{return {};}std::string gatherStats()override{return "spy";}};
struct World:Level{World(MemoryLevelStorage* s,bool java=false):Level(s,"cache",LevelSettings(17,GameType::Survival,LGV_INFINITE),LGV_INFINITE),java(java){setUpdateLights(false);}bool java;bool isJavaServerLevel()const override{return java;}void replace(ChunkSource* c){delete _chunkSource;_chunkSource=c;}};
void run(bool client,bool java){Counts counts;MemoryLevelStorage memory;World world(&memory,java);world.isClientSide=client;Storage disk(counts);auto* cache=new ChunkCache(&world,&disk,new Generator(&world,counts));world.replace(cache);
 auto* first=world.getChunk(-2,1);if(client&&!java){assert(first->isEmpty()&&!world.hasChunkNow(-2,1));first=cache->create(-2,1);}assert(!first->isEmpty());
 if(client&&!java){assert(first->getTile(0,0,0)==0&&first->skyLight.get(0,127,0)==15);first->setTile(3,60,4,Tile::rock->id);Region region(&world,-640,0,-640,640,127,640);assert(region.getTile(-29,60,20)==Tile::rock->id);for(int z=-5;z<5;++z)for(int x=-5;x<5;++x)cache->getChunk(x,z);assert(!world.hasChunkNow(4,4));cache->postProcess(cache,-2,1);cache->tick();cache->saveAll(false);assert(first->getTile(3,60,4)==Tile::rock->id);assert(counts.generated==0&&counts.populated==0&&counts.loaded==0&&counts.saved==0&&counts.ticked==0);std::cout<<"Native renderer-neighbor reads: zero generation/storage/postprocess, authoritative terrain retained PASS\n";}
 else {assert(first->getTile(0,0,0)==Tile::rock->id&&counts.generated==1&&counts.loaded==1);std::cout<<"Local/Java cache behavior retained client="<<client<<" java="<<java<<" PASS\n";}}
int main(){Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();run(true,false);run(false,false);run(true,true);}
