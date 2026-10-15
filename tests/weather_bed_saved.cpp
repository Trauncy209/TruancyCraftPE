#include "server/ServerLevel.h"
#include "network/RakNetInstance.h"
#include "network/packet/WeatherPacket.h"
#include "world/level/storage/ExternalFileLevelStorage.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/BedTile.h"
#include "world/entity/MobCategory.h"
#include "world/entity/player/Player.h"
#include "world/item/Item.h"
#include <cassert>
#include <iostream>
struct BedWorld:ServerLevel { using ServerLevel::ServerLevel; using Level::updateSkyDarken; };
struct Wire:IRakNetInstance{
 int clear=0;
 void send(Packet& p)override{auto* state=dynamic_cast<WeatherPacket*>(&p);if(state&&!state->state.raining&&!state->state.thundering)++clear;}
};
int main(int argc,char** argv){
 assert(argc==2);
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
 auto* storage=new ExternalFileLevelStorage("SleepClear",argv[1]);auto* level=new BedWorld(storage,"beds",LevelSettings(123,GameType::Survival,LGV_ORIGINAL),LGV_ORIGINAL);
 Wire wire;level->raknetInstance=&wire;level->setUpdateLights(false);
 for(int z=3;z<=5;++z)for(int x=3;x<=5;++x)level->getChunk(x,z);
 for(int z=62;z<=68;++z)for(int x=62;x<=72;++x){level->setTileNoUpdate(x,79,z,Tile::rock->id);for(int y=80;y<85;++y)level->setTileNoUpdate(x,y,z,0);}
 for(int x:{64,68}){level->setTileAndDataNoUpdate(x,80,64,Tile::bed->id,0);level->setTileAndDataNoUpdate(x,80,65,Tile::bed->id,8);}
 auto* first=new Player(level,false);auto* second=new Player(level,false);level->addEntity(first);level->addEntity(second);
 first->setPos(64.5f,81.6f,64.5f);second->setPos(68.5f,81.6f,64.5f);
 auto& weather=level->getLevelData()->weather;assert(weather.command("thunder",240000));weather.restoreIntensity();
 level->setTime(3*Level::TICKS_PER_DAY/4);level->updateSkyDarken();assert(!level->isDay());
 assert(first->startSleepInBed(64,80,64)==BedSleepingResult::OK);
 for(int i=0;i<105;++i){first->tick();level->tick();}
 assert(first->isSleeping()&&!second->isSleeping()&&weather.raining&&weather.thundering);
 assert(second->startSleepInBed(68,80,64)==BedSleepingResult::OK);
 int wake=0;for(;wake<110&&first->isSleeping();++wake){first->tick();second->tick();level->tick();}
 assert(wake>=100&&wake<=105&&!first->isSleeping()&&!second->isSleeping());
 assert(!weather.raining&&!weather.thundering&&weather.rainTicks==0&&weather.thunderTicks==0&&weather.rain>0.9f&&wire.clear>=1);
 assert(level->getTime()%Level::TICKS_PER_DAY==0);
 for(int i=0;i<101;++i)level->tick();assert(weather.rain==0&&weather.thunder==0);
 level->players.clear(); // Exclude fixture identities from the saved player tag.
 level->saveGame();level->getChunkSource()->saveAll(false);
 LevelData saved;assert(ExternalFileLevelStorage::readLevelData(argv[1],saved));
 assert(!saved.weather.raining&&!saved.weather.thundering&&saved.weather.rain==0&&saved.weather.thunder==0);
 std::cout<<"SAVED_SLEEP "<<saved.weather.rainTicks<<" "<<saved.weather.thunderTicks<<"\n";
 std::cout<<"Actual two-player beds: one sleeper preserves storm; both sleep100ticks -> dawn/clear broadcast; gradual fade and native save PASS\n";
}
