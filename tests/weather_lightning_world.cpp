#include "world/level/WeatherLightningRuntime.h"
#include "world/level/Level.h"
#include "world/level/LevelListener.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/biome/Biome.h"
#include "world/level/biome/BiomeSource.h"
#include "world/level/dimension/Dimension.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/FireTile.h"
#include "world/entity/MobCategory.h"
#include "world/entity/animal/Pig.h"
#include "world/entity/monster/PigZombie.h"
#include "world/entity/monster/Creeper.h"
#include "world/entity/player/Player.h"
#include "world/item/Item.h"
#include "network/RakNetInstance.h"
#include "network/packet/WeatherLightningPacket.h"
#include "nbt/CompoundTag.h"
#include <cassert>
#include <iostream>
#include <limits>
struct Climate:BiomeSource {
 Biome* selected=Biome::plains;
 Biome* getBiome(int,int) override{return selected;}
};
struct Victim:Entity {
 int damage=0;
 Victim(Level* l):Entity(l){}
 void immune(){fireImmune=true;}
 bool hurt(Entity*,int n) override {damage+=n;return true;}
 int getEntityTypeId() const override{return 999;}
 void readAdditionalSaveData(CompoundTag*) override{}
 void addAdditonalSaveData(CompoundTag*) override{}
};
struct Wire:IRakNetInstance {
 int starts=0,flashes=0,ends=0;
 void send(Packet& p) override {
  auto* bolt=dynamic_cast<WeatherLightningPacket*>(&p);if(!bolt)return;
  assert(bolt->valid);if(bolt->phase==0)++starts;else if(bolt->phase==1)++flashes;else ++ends;
 }
};
struct Sounds:LevelListener {
 int thunder=0,impact=0;
 void playSound(const std::string& name,float,float,float,float,float) override {
  if(name=="ambient.weather.thunder")++thunder;if(name=="random.explode")++impact;
 }
};
int main() {
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
 auto* level=new Level(new MemoryLevelStorage(),"lightning",LevelSettings(987,GameType::Survival,LGV_ORIGINAL),LGV_ORIGINAL);
 for(int x=1;x<=3;++x)for(int z=1;z<=3;++z)level->getChunk(x,z);
 for(int y=98;y<=105;++y)for(int x=36;x<=44;++x)for(int z=36;z<=44;++z)level->setTileNoUpdate(x,y,z,y==99?Tile::rock->id:0);
 auto* player=new Player(level,false);player->moveTo(44,100,44,0,0);level->addEntity(player);
 auto* victim=new Victim(level);victim->moveTo(40,100,40,0,0);victim->onFire=-1;level->addEntity(victim);
 auto* outside=new Victim(level);outside->moveTo(46,100,40,0,0);level->addEntity(outside);
 auto* immune=new Victim(level);immune->moveTo(41,100,40,0,0);immune->immune();level->addEntity(immune);
 auto* pig=new Pig(level);pig->moveTo(40,100,41,35,10);level->addEntity(pig);
 auto* creeper=new Creeper(level);creeper->moveTo(39,100,40,0,0);level->addEntity(creeper);
 Wire wire;Sounds sounds;level->raknetInstance=&wire;level->addListener(&sounds);level->difficulty=2;
 assert(!WeatherLightningRuntime::spawn(*level,std::numeric_limits<double>::quiet_NaN(),100,40));
 assert(WeatherLightningRuntime::spawn(*level,40,100,40));assert(wire.starts==1);
 assert(level->getTile(40,100,40)==Tile::fire->id);
 WeatherLightningRuntime::tick(*level);
 assert(victim->damage==5&&victim->onFire==160&&outside->damage==0&&immune->damage==0);
 assert(pig->removed&&creeper->isPowered());
 int pigs=0;for(Entity* e:level->entities)if(auto* transformed=dynamic_cast<PigZombie*>(e)) {++pigs;assert(transformed->x==40&&transformed->z==41&&transformed->yRot==35);}
 assert(pigs==1&&level->lightningBoltTime==2);
 CompoundTag saved;creeper->addAdditonalSaveData(&saved);Creeper reloaded(level);reloaded.readAdditionalSaveData(&saved);assert(reloaded.isPowered());
 for(int i=0;i<20;++i)WeatherLightningRuntime::tick(*level);
 assert(level->weatherLightning.empty());assert(wire.flashes==1&&wire.ends==1&&sounds.thunder==1&&sounds.impact==1);
 assert(!level->hasChunkNow(0,2)&&!level->hasChunkNow(4,2));
 level->isClientSide=true;WeatherLightning event;event.id=500;event.x=40;event.y=100;event.z=40;event.seed=123;
 assert(WeatherLightningRuntime::receive(*level,event,0));assert(!WeatherLightningRuntime::receive(*level,event,0));
 const int damage=victim->damage;WeatherLightningRuntime::tick(*level);assert(victim->damage==damage);
 event.life=1;event.flashes=0;event.seed=456;event.x=41;assert(!WeatherLightningRuntime::receive(*level,event,1));event.x=40;assert(WeatherLightningRuntime::receive(*level,event,1));assert(!WeatherLightningRuntime::receive(*level,event,1));
 assert(WeatherLightningRuntime::receive(*level,event,2));assert(level->weatherLightning.empty());
 event.life=2;event.flashes=1;assert(!WeatherLightningRuntime::receive(*level,event,0));assert(!WeatherLightningRuntime::spawn(*level,40,100,40));
 level->isClientSide=false;
 auto* climate=new Climate();level->dimension->biomeSource=climate;
 auto& weather=level->getLevelData()->weather;weather.rain=weather.previousRain=1;weather.thunder=weather.previousThunder=1;
 level->random.setSeed(7754);
 for(int i=0;i<500000;++i)WeatherLightningRuntime::tickChunk(*level,2,2);
 assert(!level->weatherLightning.empty());
 level->weatherLightning.clear();climate->selected=Biome::desert;level->random.setSeed(7754);
 for(int i=0;i<500000;++i)WeatherLightningRuntime::tickChunk(*level,2,2);
 assert(level->weatherLightning.empty());
 level->dimension->hasCeiling=true;assert(!WeatherLightningRuntime::spawn(*level,40,100,40));
 std::cout<<"Lightning actual world fire, damage/immunity, pig transformation, powered persistence, one flash/audio, client authority/dedup and loaded-only checks PASS\n";
 // Isolated fixture owns no running server/world and intentionally exits here.
}
