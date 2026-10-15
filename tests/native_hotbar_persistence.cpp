#include "client/Minecraft.h"
#include "client/gamemode/SurvivalMode.h"
#include "server/ServerPlayer.h"
#include "world/entity/player/Inventory.h"
#include "world/level/Level.h"
#include "world/level/storage/ExternalFileLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include "world/level/biome/Biome.h"
#include "util/Mth.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include "network/ServerSideNetworkHandler.h"
#include "network/RakNetInstance.h"
#include "network/packet/PlayerEquipmentPacket.h"
#include "network/packet/SendInventoryPacket.h"
#include "network/packet/ContainerSetContentPacket.h"
struct Game: Minecraft { Game():Minecraft(){} };
int count(Player* p,int id){int n=0;for(int i=Inventory::MAX_SELECTION_SIZE;i<p->inventory->getContainerSize();++i){auto*x=p->inventory->getItem(i);if(x&&x->id==id)n+=x->count;}return n;}
int main(int argc,char**argv){assert(argc==3);Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();auto* store=new ExternalFileLevelStorage("persist",argv[2]);auto*l=new Level(store,"persist",LevelSettings(1792046386,GameType::Survival,LGV_INFINITE),LGV_INFINITE);l->setUpdateLights(false);auto*g=new Game();g->level=l;g->gameMode=new SurvivalMode(g);auto*p=new ServerPlayer(g,l);p->name="Persist_Test";p->dimension=0;
if(std::string(argv[1])=="write") {assert(!p->hasFakeInventory);assert(store->loadNetworkPlayer(p)==0);ItemInstance wood(17,37,0);assert(p->inventory->add(&wood));assert(count(p,17)==37);RakNetInstance wire;g->raknetInstance=&wire;ServerSideNetworkHandler handler(g,&wire);handler.levelGenerated(l);p->owner=RakNet::RakNetGUID(12345);l->addEntity(p);PlayerEquipmentPacket held(p->entityId,17,0);handler.handle(p->owner,&held);assert(count(p,17)==37);PlayerEquipmentPacket empty(p->entityId,0,0);handler.handle(p->owner,&empty);assert(count(p,17)==37);p->setPos(-321.25f,84.62f,654.5f);p->health=13;p->inventory->moveToSelectionSlot(5,9,false);p->inventory->selectSlot(5);
SendInventoryPacket outbound(p,false);RakNet::BitStream wireState;outbound.write(&wireState);unsigned char packetId;assert(wireState.Read(packetId));SendInventoryPacket inbound;inbound.read(&wireState);assert(inbound.hotbar.present);assert(inbound.hotbar.selected==5&&inbound.hotbar.slots[5]==9);
Player client(l,false);client.inventory->replace(inbound.items,inbound.numItems);inbound.hotbar.apply(client.inventory);assert(client.inventory->selected==5&&client.inventory->linkedSlots[5].inventorySlot==9&&count(&client,17)==37);
ContainerSetContentPacket restore(0,inbound.items);restore.hotbar.capture(p->inventory);RakNet::BitStream restoreWire;restore.write(&restoreWire);assert(restoreWire.Read(packetId));ContainerSetContentPacket received;received.read(&restoreWire);assert(received.hotbar.present&&received.hotbar.slots[5]==9);restore.hotbar.present=false;RakNet::BitStream legacyWire;restore.write(&legacyWire);assert(legacyWire.Read(packetId));ContainerSetContentPacket legacy;legacy.read(&legacyWire);assert(!legacy.hotbar.present);
assert(store->saveNetworkPlayer(p));ItemInstance more(17,5,0);p->inventory->add(&more);l->saveLevelData();handler.onDisconnect(p->owner);std::cout<<"Pickup + disconnect snapshot + autosave PASS\n";}
else {p->name="persist_test";assert(store->loadNetworkPlayer(p)==1);assert(count(p,17)==42);assert(p->inventory->selected==5&&p->inventory->linkedSlots[5].inventorySlot==9);assert(p->x==-321.25f&&p->y==84.62f&&p->z==654.5f&&p->health==13);Player q(l,false);q.name="OtherPlayer";assert(store->loadNetworkPlayer(&q)==0);ItemInstance dirt(3,9,0);q.inventory->add(&dirt);assert(store->saveNetworkPlayer(&q));assert(count(p,17)==42);std::string path=std::string(argv[2])+"/players/706572736973745f74657374.dat";std::ofstream broken(path,std::ios::binary|std::ios::trunc);broken<<"truncated";broken.close();Player corrupt(l,false);corrupt.name="persist_test";assert(store->loadNetworkPlayer(&corrupt)==-1);std::cout<<"Corrupt save rejected PASS\n";std::cout<<"Separate process restart: exact hotbar slot 5, selection 5, position, health, 42 wood, player isolation PASS\n";}
}
