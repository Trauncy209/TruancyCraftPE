#include "client/Minecraft.h"
#include "network/java/JavaGameMode.h"
#include "client/gamemode/SurvivalMode.h"
#include "world/level/Level.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/entity/Entity.h"
#include "server/ServerPlayer.h"
#include "world/level/material/Material.h"
#include "world/entity/MobCategory.h"
#include "world/level/tile/Tile.h"
#include "world/item/Item.h"
#include "world/level/biome/Biome.h"
#include "client/player/LocalPlayer.h"
#include "network/ClientSideNetworkHandler.h"
#include "network/packet/MoveEntityPacket.h"
#include <cassert>
#include <iostream>
struct GameProbe:Minecraft{GameProbe():Minecraft(){}};
struct RemoteProbe:ServerPlayer{using ServerPlayer::ServerPlayer;bool genericEcho()const{return autoSendPosRot;}};
struct Probe:Entity{
 Level* testLevel;
 int calls=0;
 Probe(Level* l):Entity(l),testLevel(l){}
 ~Probe() override {
  // Recovery attaches the probe to a chunk. Remove it before stack destruction.
  if(inChunk && testLevel->hasChunk(xChunk,zChunk))
   testLevel->getChunk(xChunk,zChunk)->removeEntity(this,yChunk);
  inChunk=false;
 }
 void tick()override{++calls;}int getEntityTypeId()const override{return 0;}void readAdditionalSaveData(CompoundTag*)override{}void addAdditonalSaveData(CompoundTag*)override{}};
int main(){
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
 auto* game=new GameProbe();game->setIsCreativeMode(false);delete game->gameMode;game->gameMode=new JavaGameMode(game);game->setIsCreativeMode(false);assert(dynamic_cast<JavaGameMode*>(game->gameMode)==nullptr&&dynamic_cast<SurvivalMode*>(game->gameMode));
 std::cout<<"Java-to-native Survival controller replacement PASS\n";
 auto* store=new MemoryLevelStorage();auto* level=new Level(store,"",LevelSettings(123456,GameType::Survival,LGV_ORIGINAL),LGV_ORIGINAL);game->level=level;
 for(int z=0;z<16;++z)for(int x=0;x<16;++x)level->getChunk(x,z);
 for(auto pos:{std::pair<float,float>{.5f,.5f},{31.92f,146.38f},{255.5f,.5f},{.5f,255.5f},{255.5f,255.5f},{128.f,128.f}}){Probe probe(level);probe.setPos(pos.first,80,pos.second);probe.inChunk=true;probe.xChunk=(int)pos.first>>4;probe.zChunk=(int)pos.second>>4;probe.yChunk=5;level->tick(&probe,true);assert(probe.calls==1);}
 std::cout<<"Finite physics ticks at every corner, frozen x31.92 position and center PASS\n";
 for(auto pos:{std::pair<float,float>{-.2f,128.f},{256.17f,172.f},{128.f,-.2f},{172.f,256.17f}}){Probe probe(level);probe.setPos(pos.first,80,pos.second);probe.inChunk=false;level->tick(&probe,true);assert(probe.calls==1&&probe.inChunk&&probe.x>0&&probe.x<256&&probe.z>0&&probe.z<256);}
 for(auto delta:{std::pair<float,float>{-4,0},{4,0},{0,-4},{0,4}}){Probe probe(level);probe.setPos(delta.first<0?.5f:delta.first>0?255.5f:128.f,100,delta.second<0?.5f:delta.second>0?255.5f:128.f);probe.move(delta.first,0,delta.second);assert(probe.bb.x0>=0&&probe.bb.x1<=256&&probe.bb.z0>=0&&probe.bb.z1<=256);}
 std::cout<<"Finite border collision and stranded-player recovery PASS\n";
 RemoteProbe remote(game,level);assert(!remote.genericEcho());std::cout<<"Native remote player never broadcasts generic owner movement echo PASS\n";
 std::cout<<"Gameplay regressions PASS\n";
}
