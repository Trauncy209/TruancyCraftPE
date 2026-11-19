#include <sstream>
#include <iostream>
#define private public
#include "client/Minecraft.h"
#undef private
#include "client/gui/screens/DisconnectionScreen.h"
#include "client/player/LocalPlayer.h"
#include "client/User.h"
#include "network/RakNetInstance.h"
#define private public
#include "network/ClientSideNetworkHandler.h"
#undef private
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
int main(){Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();auto*l=new Level(new MemoryLevelStorage(),"disconnect",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);l->isClientSide=true;l->setUpdateLights(false);auto*g=new Game();g->level=l;RakNetInstance wire;g->raknetInstance=&wire;g->player=new LocalPlayer(g,l,NULL,0,false);g->player->nativeInventoryReady=true;g->screenMutex=true;g->user=new User("DisconnectProbe","");ClientSideNetworkHandler handler(g,&wire);handler.level=l;handler.beginJoin();handler.onConnect(RakNet::RakNetGUID(1234));handler.onDisconnect(RakNet::RakNetGUID(1234));assert(l->isClientSide);assert(!g->player->nativeInventoryReady);assert(g->hasScheduledScreen&&dynamic_cast<DisconnectionScreen*>(g->scheduledScreen));std::cout<<"Disconnect retains remote authority, disables inventory uploads, and requests disconnect UI PASS\n";}
