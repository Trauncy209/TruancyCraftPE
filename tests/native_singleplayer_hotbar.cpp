#include "world/level/Level.h"
#include "world/level/storage/ExternalFileLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/entity/MobCategory.h"
#include "world/entity/player/Player.h"
#include "world/entity/player/Inventory.h"
#include "world/item/Item.h"
#include "world/level/biome/Biome.h"
#include "util/Mth.h"
#include <cassert>
#include <iostream>
#include <unistd.h>
int main(){
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
 char dir[]="/tmp/tcpe-local-hotbar-XXXXXX";assert(mkdtemp(dir));
 auto* store=new ExternalFileLevelStorage("hotbar",dir);auto* level=new Level(store,"hotbar",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);level->setUpdateLights(false);
 auto* player=new Player(level,false);player->name="LocalHotbarProbe";level->addEntity(player);player->inventory->clearInventory();
 ItemInstance wood(17,37,0),dirt(3,8,0);player->inventory->setItem(9,&wood);player->inventory->setItem(15,&dirt);
 assert(player->inventory->moveToSelectionSlot(5,9,false));assert(player->inventory->moveToSelectionSlot(2,15,false));player->inventory->selectSlot(5);
 player->setPos(-321.25f,84.62f,654.5f);player->health=13;level->saveLevelData();
 auto* reopenStore=new ExternalFileLevelStorage("hotbar",dir);auto* reopened=new Level(reopenStore,"hotbar",LevelSettings(9,GameType::Creative,LGV_ORIGINAL),LGV_ORIGINAL);reopened->setUpdateLights(false);
 auto* restored=new Player(reopened,false);reopened->loadPlayer(restored,true);
 assert(restored->inventory->getLinked(0)==NULL);
 assert(restored->inventory->getLinked(2)&&restored->inventory->getLinked(2)->id==3&&restored->inventory->getLinked(2)->count==8);
 assert(restored->inventory->getLinked(5)&&restored->inventory->getLinked(5)->id==17&&restored->inventory->getLinked(5)->count==37);
 assert(restored->inventory->selected==5&&restored->health==13&&restored->x==-321.25f&&restored->z==654.5f);
 std::cout<<"Singleplayer metadata save/reopen preserves intentional empty hotbar gaps, exact links, selected slot, inventory, position and health PASS\n";
}
