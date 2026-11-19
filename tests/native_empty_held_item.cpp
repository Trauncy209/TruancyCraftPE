#include "client/Minecraft.h"
#include "client/User.h"
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
int main(){
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
 auto*l=new Level(new MemoryLevelStorage(),"empty-held",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE); l->setUpdateLights(false);
 auto*g=new Game();g->level=l;RakNetInstance wire;g->raknetInstance=&wire;auto*p=new LocalPlayer(g,l,NULL,0,false);g->player=p;ClientSideNetworkHandler handler(g,&wire);g->user=new User("InventoryProbe","");const RakNet::RakNetGUID authority(1234);handler.beginJoin();handler.onConnect(authority);
 std::vector<ItemInstance> items(36);items[0]=ItemInstance(17,4,0);items[5]=ItemInstance(58,1,0);
 ContainerSetContentPacket restore(0,items); restore.hotbar.present=true;
 for(int i=0;i<9;++i)restore.hotbar.slots[i]=-1;
 restore.hotbar.slots[0]=9;restore.hotbar.slots[3]=15;restore.hotbar.slots[5]=14;restore.hotbar.selected=3;
 handler.handle(RakNet::RakNetGUID(9876),&restore);assert(!p->nativeInventoryReady);handler.handle(authority,&restore);
 // The server restores all 36 slots, including empty backing slot 15.
 assert(!p->canUseCarriedItemWhileMoving());
 assert(p->inventory->getSelected()==NULL);
 ItemInstance empty;assert(!empty.useOn(p,l,0,64,0,1,.5f,.5f,.5f));
 ItemInstance unknown(Item::MAX_ITEMS-1,1,0);assert(unknown.getItem()==NULL);
 p->inventory->setItem(15,&unknown);assert(!p->canUseCarriedItemWhileMoving());
 assert(!unknown.useOn(p,l,0,64,0,1,.5f,.5f,.5f));
 p->inventory->selectSlot(5);assert(p->inventory->getSelected()->id==58);assert(!p->canUseCarriedItemWhileMoving());
 p->inventory->getSelected()->count=0;assert(p->inventory->getSelected()==NULL);
 ItemInstance bow(Item::bow,1);p->inventory->setItem(15,&bow);p->inventory->selectSlot(3);assert(p->canUseCarriedItemWhileMoving());
 ItemInstance apple(Item::apple,1);p->inventory->setItem(15,&apple);assert(p->canUseCarriedItemWhileMoving());
 std::cout<<"PASS: actual native inventory restore with selected empty slot, zero-count hotbar, unknown held item, valid crafting table\n";
}
