#include "client/Minecraft.h"
#include "client/player/LocalPlayer.h"
#include "network/ClientSideNetworkHandler.h"
#include "network/RakNetInstance.h"
#include "network/packet/ContainerSetContentPacket.h"
#include "world/level/Level.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include "world/level/biome/Biome.h"
#include "util/Mth.h"
#include <cassert>
#include <iostream>
struct Game: Minecraft{Game():Minecraft(){}};
int main(){Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();auto*l=new Level(new MemoryLevelStorage(),"hotbar",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);l->setUpdateLights(false);auto*g=new Game();g->level=l;RakNetInstance wire;g->raknetInstance=&wire;auto*p=new LocalPlayer(g,l,NULL,0,false);g->player=p;ClientSideNetworkHandler handler(g,&wire);assert(!p->nativeInventoryReady);std::vector<ItemInstance> items(36);items[0]=ItemInstance(17,42,0);items[6]=ItemInstance(3,8,0);ContainerSetContentPacket restore(0,items);restore.hotbar.present=true;for(int i=0;i<9;++i)restore.hotbar.slots[i]=-1;restore.hotbar.slots[2]=15;restore.hotbar.slots[5]=9;restore.hotbar.selected=5;handler.handle(RakNet::UNASSIGNED_RAKNET_GUID,&restore);assert(p->nativeInventoryReady&&!p->nativeInventoryState.empty());assert(p->inventory->getLinked(5)->id==17&&p->inventory->getLinked(5)->count==42);assert(p->inventory->getLinked(2)->id==3);assert(p->inventory->getLinked(0)==NULL);assert(p->inventory->selected==5);auto* q=new LocalPlayer(g,l,NULL,0,false);g->player=q;handler.handle(RakNet::UNASSIGNED_RAKNET_GUID,&restore);assert(q->inventory->getLinked(5)->count==42&&q->inventory->selected==5);ContainerSetContentPacket legacy(0,items);auto*old=new LocalPlayer(g,l,NULL,0,false);g->player=old;handler.handle(RakNet::UNASSIGNED_RAKNET_GUID,&legacy);assert(old->inventory->getLinked(0)->id==17);std::cout<<"Actual client restore: exact non-contiguous hotbar, selected slot, inventory-ready gate, new-player rejoin, legacy fallback PASS\n";}
