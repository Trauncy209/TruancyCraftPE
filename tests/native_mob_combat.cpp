#include "client/Minecraft.h"
#include "client/gamemode/SurvivalMode.h"
#include "server/ServerPlayer.h"
#include "network/ServerSideNetworkHandler.h"
#include "network/RakNetInstance.h"
#include "network/packet/InteractPacket.h"
#include "network/packet/MovePlayerPacket.h"
#include "network/packet/ContainerSetContentPacket.h"
#include "world/level/Level.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/entity/MobCategory.h"
#include "world/entity/MobFactory.h"
#include "world/entity/item/ItemEntity.h"
#include "world/item/Item.h"
#include "world/level/biome/Biome.h"
#include "util/Mth.h"
#include <cassert>
#include <iostream>
#include <limits>
struct TestLevel: Level {using Level::Level;using Level::updateSkyDarken;};
struct Game: Minecraft{Game():Minecraft(){}};
struct Wire: RakNetInstance {int snapshots=0;void send(const RakNet::RakNetGUID& owner,Packet& packet) override {if(auto* content=dynamic_cast<ContainerSetContentPacket*>(&packet)){assert(owner==RakNet::RakNetGUID(12345));assert(content->containerId==0&&content->items.size()==36&&content->hotbar.present);++snapshots;}RakNetInstance::send(owner,packet);}};
int main(){Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
 auto*l=new TestLevel(new MemoryLevelStorage(),"combat",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);l->setUpdateLights(false);l->difficulty=2;l->setTime(18000);l->updateSkyDarken();
 auto*g=new Game();g->level=l;g->gameMode=new SurvivalMode(g);Wire wire;g->raknetInstance=&wire;l->raknetInstance=&wire;ServerSideNetworkHandler handler(g,&wire);handler.levelGenerated(l);
 for(int x=0;x<16;++x)for(int z=0;z<16;++z){for(int y=64;y<96;++y)l->setTileNoUpdate(x,y,z,0);l->setTileNoUpdate(x,63,z,1);}
 auto*p=new ServerPlayer(g,l);p->name="CombatQA";p->owner=RakNet::RakNetGUID(12345);p->setPos(0,65.62f,0);l->addEntity(p);
 ItemInstance sword(Item::sword_iron,1);p->inventory->add(&sword);p->inventory->selectSlot(0);
 int types[]={MobTypes::Chicken,MobTypes::Cow,MobTypes::Pig,MobTypes::Sheep,MobTypes::Creeper,MobTypes::Zombie,MobTypes::Skeleton,MobTypes::Spider,MobTypes::PigZombie};
 for(int type:types){auto*m=MobFactory::CreateMob(type,l);m->moveTo(8,64,8,0,0);l->addEntity(m);int hits=0;
  while(m->health>0&&hits<50){int before=m->health;InteractPacket attack(InteractPacket::Attack,p->entityId,m->entityId);handler.handle(p->owner,&attack);if(!(m->health<before))std::cerr<<"FAILED type="<<type<<" hits="<<hits<<" hp="<<m->health<<" removed="<<m->removed<<" cooldown="<<m->invulnerableTime<<" playerhp="<<p->health<<" pos="<<m->x<<","<<m->y<<","<<m->z<<std::endl;assert(m->health<before);++hits;for(int tick=0;tick<21;++tick)l->tick(m,true);if(m->invulnerableTime!=0)std::cerr<<"COOLDOWN type="<<type<<" time="<<m->invulnerableTime<<" hp="<<m->health<<" removed="<<m->removed<<" pos="<<m->x<<","<<m->y<<","<<m->z<<std::endl;assert(m->invulnerableTime==0);}
  assert(m->health<=0);std::cout<<"mob "<<type<<" damage/death PASS hits="<<hits<<std::endl;
 }
 int beforeLoot=0;for(int i=9;i<45;++i){auto*stack=p->inventory->getItem(i);if(stack)beforeLoot+=stack->count;}
 auto entities=l->entities;int drops=0,pickedCount=0;for(auto*e:entities)if(e->isItemEntity()&&!e->removed){auto*i=(ItemEntity*)e;i->throwTime=0;pickedCount+=i->item.count;i->playerTouch(p);assert(i->removed);++drops;}
 auto* rejected=MobFactory::CreateMob(MobTypes::Cow,l);rejected->moveTo(1,64,1,0,0);l->addEntity(rejected);p->attack(rejected);int wear=p->inventory->getSelected()->getAuxValue();p->attack(rejected);assert(p->inventory->getSelected()->getAuxValue()==wear);
 std::cout<<"neighborhood="<<l->hasChunksAt(-32,0,-32,32,127,32)<<" invulnerable="<<rejected->invulnerableTime<<std::endl;
 for(int i=0;i<25;++i)l->tick(rejected,true);
 std::cout<<"after level ticks invulnerable="<<rejected->invulnerableTime<<std::endl;assert(rejected->invulnerableTime==0);
 auto* delayed=new ItemEntity(l,8,75,8,ItemInstance(Item::leather,1));delayed->throwTime=10;l->addEntity(delayed);for(int i=0;i<10;++i)l->tick(delayed,true);assert(delayed->throwTime==0);delayed->playerTouch(p);assert(delayed->removed);
 std::cout<<"Sparse infinite chunks: mob cooldown and item pickup timers PASS"<<std::endl;
 l->isClientSide=true;p->attack(rejected);assert(p->inventory->getSelected()->getAuxValue()==wear);l->isClientSide=false;
 int afterLoot=0;for(int i=9;i<45;++i){auto*stack=p->inventory->getItem(i);if(stack)afterLoot+=stack->count;}assert(afterLoot==beforeLoot+pickedCount+1);
 assert(wire.snapshots>=drops+9);
 auto* unloaded=MobFactory::CreateMob(MobTypes::Cow,l);unloaded->setPos(100000,70,100000);unloaded->invulnerableTime=20;l->tick(unloaded,true);assert(unloaded->invulnerableTime==20&&!l->hasChunkNow(6250,6250));delete unloaded;
 p->inChunk=false;MovePlayerPacket moved(p->entityId,-240.f,64.f,714.f,10.f,20.f);handler.handle(p->owner,&moved);assert(p->x==-240.f&&p->z==714.f&&std::abs(p->y-(64.f+p->heightOffset))<.001f&&p->inChunk&&p->xChunk==-15&&p->zChunk==44);
 MovePlayerPacket spoofed(p->entityId,0.f,64.f,0.f,0.f,0.f);handler.handle(RakNet::RakNetGUID(9876),&spoofed);assert(p->x==-240.f);
 MovePlayerPacket overflow(p->entityId,std::numeric_limits<float>::max(),64.f,0.f,0.f,0.f);handler.handle(p->owner,&overflow);assert(p->x==-240.f);
 MovePlayerPacket nan(p->entityId,std::numeric_limits<float>::quiet_NaN(),64.f,0.f,0.f,0.f);handler.handle(p->owner,&nan);assert(p->x==-240.f);
 auto* travelDrop=new ItemEntity(l,p->x,p->y-p->heightOffset,p->z,ItemInstance(Item::leather,1));l->addEntity(travelDrop);p->aiStep();assert(travelDrop->removed);
 std::cout<<"Remote movement out-of-chunk recovery, owner validation, pickup at traveled position PASS"<<std::endl;
 assert(drops>0);std::cout<<"loot pickup PASS drops="<<drops<<std::endl;
}
