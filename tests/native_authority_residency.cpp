#include "client/Minecraft.h"
#include "client/gamemode/SurvivalMode.h"
#include "server/ServerPlayer.h"
#include "network/RakNetInstance.h"
#include "world/level/Level.h"
#include "world/level/chunk/ChunkCache.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include "world/level/biome/Biome.h"
#include "util/Mth.h"
#include <iostream>
#include <cassert>
#include <unistd.h>
#include "world/level/storage/ExternalFileLevelStorage.h"
struct Game:Minecraft {};
int main(int argc,char** argv){
 const bool generated=true;
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
 auto* game=new Game();RakNetInstance wire;game->raknetInstance=&wire;game->gameMode=new SurvivalMode(game);
 char dir[]="/tmp/tcpe-authority-residency-XXXXXX";assert(mkdtemp(dir));auto* storage=new ExternalFileLevelStorage("authority",dir);
 auto* level=new Level(storage,"two-player-alias",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);
 game->level=level;level->raknetInstance=&wire;level->setUpdateLights(false);
 // Construct blank resident snapshots without running the separately known
 // biome-noise generation path. The final authoritative cache state is the
 // same two-player ring collision reached by loading these server chunks.
 level->isClientSide=!generated;auto* cache=dynamic_cast<ChunkCache*>(level->getChunkSource());
 if(generated)cache->getChunk(0,0);else cache->create(0,0);auto* first=new ServerPlayer(game,level);first->name="ResidencyFirst";first->owner=RakNet::RakNetGUID(1);first->moveTo(8,70,8,0,0);level->addEntity(first);
 if(generated)cache->getChunk(32,0);else cache->create(32,0);auto* second=new ServerPlayer(game,level);second->name="ResidencySecond";second->owner=RakNet::RakNetGUID(2);second->moveTo(520,70,8,0,0);level->addEntity(second);
 level->isClientSide=false;first->inventoryV1=true;first->health=10;
 ItemInstance apple(Item::apple,2);first->inventory->setItem(9,&apple);first->inventory->linkSlot(0,9,false);first->startUsingItem(apple,32);
 const int before=first->getUseItemDuration();
 level->tick(first,true);
 const int after=first->getUseItemDuration();
 std::cout<<"generated="<<generated<<" authoritative="<<!level->isClientSide<<" players="<<level->players.size()<<" firstResident="<<level->hasChunkNow(0,0)<<" secondResident="<<level->hasChunkNow(32,0)<<" firstInChunk="<<first->inChunk<<" foodBefore="<<before<<" foodAfter="<<after<<std::endl;
 if(after>=before){std::cout<<"FAIL: stationary active player food timer freezes when another player's chunk aliases the ring slot\n";return 2;}
 assert(level->hasChunkNow(0,0)&&level->hasChunkNow(32,0));
 auto* firstChunk=cache->getChunk(0,0);auto* secondChunk=cache->getChunk(32,0);
 for(int i=0;i<16;++i){assert(cache->getChunk(0,0)==firstChunk);assert(cache->getChunk(32,0)==secondChunk);}
 firstChunk->setTileRaw(3,90,3,Tile::wood->id);firstChunk->unsaved=true;
 for(int i=0;i<1250;++i){auto* travel=cache->getChunk(1024+i,1000);if(i==0){travel->setTileRaw(8,90,8,Tile::wood->id);travel->data.set(8,90,8,3);travel->unsaved=true;}level->collectRetiredChunks();assert(cache->residentChunkCount()<=cache->residentCapacity());}
 assert(cache->getChunk(0,0)==firstChunk&&cache->getChunk(32,0)==secondChunk);
 // Exercise unpinned recency, including repeated last-fast-path reads. This
 // ordering makes coordinate-ordered/FIFO eviction fail deterministically.
 auto* recent=cache->getChunk(20000,20000);cache->getChunk(20001,20000);
 cache->getChunk(20000,20000);cache->getChunk(20000,20000);
 level->collectRetiredChunks();
 assert(cache->getChunk(20000,20000)==recent);
 assert(cache->hasChunkNow(20000,20000));
 assert(firstChunk->getTile(3,90,3)==Tile::wood->id);
 assert(cache->ownedChunkCount()<=cache->residentCapacity()+2);
 const int again=first->getUseItemDuration();level->tick(first,true);assert(first->getUseItemDuration()<again);
 level->getChunkSource()->saveAll(true);
 std::cout<<"PASS: generated far-separated players tick, coordinate identities survive alternating reads/travel, pinned chunks and bounded LRU retain edits\n";
 game->level=nullptr;delete level;delete storage;game->raknetInstance=nullptr;
 auto* reopenedStorage=new ExternalFileLevelStorage("authority",dir);
 auto* reopened=new Level(reopenedStorage,"authority",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);reopened->setUpdateLights(false);
 assert(reopened->getChunk(0,0)->getTile(3,90,3)==Tile::wood->id);
 auto* evicted=reopened->getChunk(1024,1000);assert(evicted->getTile(8,90,8)==Tile::wood->id&&evicted->getData(8,90,8)==3);
 delete reopened;delete reopenedStorage;
 auto* clientStorage=new MemoryLevelStorage();auto* client=new Level(clientStorage,"client",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);client->isClientSide=true;client->setUpdateLights(false);
 auto* clientCache=dynamic_cast<ChunkCache*>(client->getChunkSource());clientCache->create(0,0);clientCache->create(32,0);
 assert(!client->hasChunkNow(0,0)&&client->hasChunkNow(32,0)&&clientCache->residentCapacity()==1024);
 delete client;delete clientStorage;

 auto* lruStorage=new MemoryLevelStorage();auto* lruLevel=new Level(lruStorage,"lru",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);lruLevel->setUpdateLights(false);
 auto* lru=dynamic_cast<ChunkCache*>(lruLevel->getChunkSource());
 auto* hot=lru->getChunk(0,0);for(int x=1;x<int(lru->residentCapacity());++x)lru->getChunk(x,0);
 lru->getChunk(0,0);lru->getChunk(0,0);lru->getChunk(int(lru->residentCapacity()),0);lruLevel->collectRetiredChunks();
 assert(lru->hasChunkNow(0,0)&&!lru->hasChunkNow(1,0)&&lru->getChunk(0,0)==hot);
 delete lruLevel;delete lruStorage;
 std::cout<<"PASS: unpinned recently-read chunk survives while older chunk retires (including last fast path)\n";
 std::cout<<"PASS: evicted and resident edits reopen from disk; native client retains original32x32ring\n";

}
