#include "network/java/JavaEntityAdapter.h"
#include "network/java/JavaGameSession.h"
#include "network/java/JavaDisplayRenderer.h"
#include "network/java/JavaLevel.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/tile/Tile.h"
#include "world/item/Item.h"
#include "world/entity/item/ItemEntity.h"
#include "world/entity/monster/PigZombie.h"
#include "world/entity/player/Player.h"
#include "world/level/material/Material.h"
#include "world/entity/MobCategory.h"
#include "world/level/biome/Biome.h"
#include "util/Mth.h"
#include "client/renderer/entity/EntityRenderDispatcher.h"
#include "client/player/RemotePlayer.h"
#include <cassert>
#include <iostream>
class OfflineGateProbe:public JavaLevel{public:OfflineGateProbe():JavaLevel(new MemoryLevelStorage(),JavaChunkStore()){}bool isJavaServerLevel()const override{return false;}};
class TickProbePlayer:public RemotePlayer{public:explicit TickProbePlayer(Level* level):RemotePlayer(level,false){}void tick()override{++tickCount;}};
int main() {
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();
 Tile::initTiles();Item::initItems();Biome::initBiomes();
 JavaGameSession session;
 Level* level=new JavaLevel(new MemoryLevelStorage(),session.chunks);
 JavaGameSession::Entity snapshot{};snapshot.id=200;snapshot.kind="Player";snapshot.position={{2,64,3}};snapshot.yaw=90;snapshot.headYaw=100;snapshot.pitch=20;snapshot.sneaking=true;snapshot.hurtTicks=4;
 snapshot.equipment[0].originalId=267;snapshot.equipment[0].count=1;
 session.entities[200]=snapshot;
 synchronizeJavaEntities(level,session);
 auto* player=level->getEntity(200);assert(player&&player->isPlayer());assert(player->bb.y0==64);assert(player->isSneaking());assert(static_cast<Mob*>(player)->hurtTime==4);assert(static_cast<Mob*>(player)->getCarriedItem()->id==267);
 const int initialTicks=player->tickCount;level->tick(player);assert(player->tickCount==initialTicks+1);
 player->inChunk=false;level->tick(player);assert(player->tickCount==initialTicks+2);
 level->tick(player,false);assert(player->tickCount==initialTicks+2);
 snapshot.position={{8,-50,9}};session.entities[200]=snapshot;synchronizeJavaEntities(level,session);assert(player->x==8&&player->bb.y0==-50);
 // Query through Level*, as native picking does. No server chunks have arrived.
 assert(level->getEntities(nullptr,player->bb).size()==1);
 assert(level->getEntities(player,player->bb).empty());
 snapshot.position={{80,260,-90}};session.entities[200]=snapshot;synchronizeJavaEntities(level,session);
 assert(level->getEntities(nullptr,player->bb).size()==1);
 assert(level->getEntities(nullptr,AABB(1,63,2,3,67,4)).empty());
 snapshot.kind="Pig";snapshot.id=201;session.entities[201]=snapshot;synchronizeJavaEntities(level,session);assert(level->getEntity(201)&&!level->getEntity(201)->isPlayer());
 session.entities.erase(200);synchronizeJavaEntities(level,session);assert(player->removed);
 for(Entity* candidate:level->getEntities(nullptr,player->bb))assert(candidate!=player);
 // Removal goes through native Level, including delayed player deletion.
 level->tickEntities();assert(!level->getEntity(200));
 snapshot.kind="Player";snapshot.id=200;session.entities[200]=snapshot;synchronizeJavaEntities(level,session);assert(level->getEntity(200)&&level->getEntity(200)!=player);
 for(int i=0;i<20;++i)level->tickEntities();assert(level->getEntity(200));
 snapshot.kind="Marker";snapshot.id=202;session.entities[202]=snapshot;synchronizeJavaEntities(level,session);
 Entity* marker=level->getEntity(202);assert(marker&&!marker->isMob());
 assert(marker->bbWidth==0.f&&marker->bbHeight==0.f&&!marker->isPickable()&&!marker->isShootable());
 EntityRenderDispatcher dispatcher;assert(renderJavaPresentation(marker,dispatcher,0,0,0));
 snapshot.kind="Interaction";snapshot.id=203;snapshot.hitbox={{4,3}};
 session.entities[203]=snapshot;synchronizeJavaEntities(level,session);
 Entity* interaction=level->getEntity(203);assert(interaction&&!interaction->isMob()&&interaction->isPickable());
 assert(interaction->bbWidth==4.f&&interaction->bbHeight==3.f);
 assert(renderJavaPresentation(interaction,dispatcher,0,0,0));
 snapshot.kind="ArmorStand";snapshot.id=204;snapshot.hitbox={{.5,1.975}};snapshot.invisible=true;
 session.entities[204]=snapshot;synchronizeJavaEntities(level,session);Entity* stand=level->getEntity(204);
 assert(stand&&stand->isPickable()&&stand->isShootable());assert(stand->bbWidth==.5f&&stand->bbHeight==1.975f);
 snapshot.hitbox={{.25,.9875}};session.entities[204]=snapshot;synchronizeJavaEntities(level,session);assert(stand->isPickable()&&stand->bbWidth==.25f&&stand->bbHeight==.9875f&&javaArmorStandNameHeight(snapshot)==.9875f);
 snapshot.armorStandMarker=true;session.entities[204]=snapshot;synchronizeJavaEntities(level,session);
 assert(!stand->isPickable()&&!stand->isShootable()&&stand->bbWidth==0.f&&stand->bbHeight==0.f);

 snapshot.kind="Item";snapshot.id=205;snapshot.position={{4,-60,5}};snapshot.droppedItem.originalId=267;snapshot.droppedItem.originalData=0;snapshot.droppedItem.count=16;
 session.entities[205]=snapshot;synchronizeJavaEntities(level,session);auto* dropped=dynamic_cast<ItemEntity*>(level->getEntity(205));assert(dropped&&!dropped->isMob()&&dropped->isItemEntity());assert(dropped->item.id==267&&dropped->item.count==16&&dropped->bb.y0==-60);const int itemAge=dropped->age;level->tick(dropped);assert(dropped->age==itemAge+1&&dropped->bb.y0==-60);dropped->playerTouch(Player::asPlayer(level->getEntity(200)));assert(dropped->item.count==16&&!dropped->removed);assert(!dropped->hurt(nullptr,100));
 snapshot.droppedItem.count=0;session.entities[205]=snapshot;synchronizeJavaEntities(level,session);assert(dropped->item.isNull());assert(renderJavaPresentation(dropped,dispatcher,0,0,0));
 snapshot.droppedItem.count=1;snapshot.droppedItem.originalId=-1;session.entities[205]=snapshot;synchronizeJavaEntities(level,session);assert(dropped->item.isNull());assert(renderJavaPresentation(dropped,dispatcher,0,0,0));
 snapshot.kind="ZombifiedPiglin";snapshot.id=206;snapshot.position={{9,-60,7}};snapshot.equipment[0].originalId=283;snapshot.equipment[0].originalData=0;snapshot.equipment[0].count=1;session.entities[206]=snapshot;synchronizeJavaEntities(level,session);auto* pigZombie=dynamic_cast<PigZombie*>(level->getEntity(206));assert(pigZombie&&pigZombie->getTexture()=="mob/pigzombie.png");assert(pigZombie->getCarriedItem()&&pigZombie->getCarriedItem()->id==283);const float px=pigZombie->x;level->tick(pigZombie);assert(pigZombie->x==px&&pigZombie->bb.y0==-60);
 JavaChunkStore delayed;std::vector<uint16_t> states(256*256);
 TcpeJavaEvent chunk{};chunk.kind=TCPE_JAVA_CHUNK;chunk.min_y=0;chunk.height=256;chunk.states=states.data();chunk.states_len=states.size();
 assert(delayed.apply(chunk));static_cast<JavaLevel*>(level)->synchronize(delayed);
 assert(level->minBuildHeight()==0&&level->maxBuildHeight()==256);
 delayed.unload(0,0);static_cast<JavaLevel*>(level)->synchronize(delayed);
 assert(level->minBuildHeight()==0&&level->maxBuildHeight()==256);
 states.resize(512*256);chunk.min_y=-128;chunk.height=512;chunk.states=states.data();chunk.states_len=states.size();
 assert(delayed.apply(chunk));static_cast<JavaLevel*>(level)->synchronize(delayed);
 assert(level->minBuildHeight()==-128&&level->maxBuildHeight()==384);
 states.resize(256*256);chunk.min_y=256;chunk.height=256;chunk.states=states.data();chunk.states_len=states.size();
 assert(delayed.apply(chunk));static_cast<JavaLevel*>(level)->synchronize(delayed);
 assert(level->minBuildHeight()==256&&level->maxBuildHeight()==512);
 Entity* currentPlayer=level->getEntity(200);const int highTicks=currentPlayer->tickCount;
 level->tick(currentPlayer);assert(currentPlayer->tickCount==highTicks+1);
 OfflineGateProbe* offline=new OfflineGateProbe();TickProbePlayer* stopped=new TickProbePlayer(offline);stopped->setPos(2,64,3);assert(offline->addEntity(stopped));const int oldTicks=stopped->tickCount;static_cast<Level*>(offline)->tick(stopped);assert(stopped->tickCount==oldTicks);delete offline;
 JavaGameSession::Entity mannequin{};mannequin.id=777;mannequin.kind="Mannequin";mannequin.position={{4,260,5}};mannequin.profileName="ActualNpc";session.entities[777]=mannequin;synchronizeJavaEntities(level,session);
 Entity* npc=level->getEntity(777);assert(dynamic_cast<RemotePlayer*>(npc));assert(Player::asPlayer(npc)->name=="ActualNpc");const double npcY=npc->y;for(int tick=0;tick<10;++tick)npc->tick();assert(npc->y==npcY);
 delete level;
 std::cout<<"Entity pose, gear, removal and ID reuse PASS\n";
}
