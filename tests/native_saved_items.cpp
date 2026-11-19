#include "world/level/tile/entity/ChestTileEntity.h"
#include "world/level/tile/entity/FurnaceTileEntity.h"
#include "world/inventory/FillingContainer.h"
#include "world/entity/player/Inventory.h"
#include "world/entity/player/Player.h"
#include "world/level/Level.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/entity/item/ItemEntity.h"
#include "world/item/Item.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/entity/MobCategory.h"
#include "world/level/biome/Biome.h"
#include "client/renderer/entity/ItemRenderer.h"
#include "nbt/CompoundTag.h"
#include "nbt/ListTag.h"
#include "util/Mth.h"
#include <cassert>
#include <iostream>
#include <string>
static CompoundTag* entry(int slot,int id,int count,int aux=0) {
    auto* tag=new CompoundTag();tag->putByte("Slot",slot);tag->putShort("id",id);
    tag->putByte("Count",count);tag->putShort("Damage",aux);return tag;
}
struct SavedPlayer : Player { using Player::readAdditionalSaveData; explicit SavedPlayer(Level* level) : Player(level,false) {} };
int main(int argc,char** argv) {
    std::cout.setf(std::ios::unitbuf);
    Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
    if(argc>1 && std::string(argv[1])=="render") {
        ItemRenderer renderer;
        for(int id:{-1,0,Item::MAX_ITEMS,Item::MAX_ITEMS-1}) {
            ItemEntity entity(NULL,0,0,0,ItemInstance(id,1,0));
            renderer.render(&entity,0,0,0,0,0);
            ItemRenderer::renderGuiItemCorrect(NULL,NULL,&entity.item,0,0);
            ItemRenderer::renderGuiItem(NULL,NULL,&entity.item,0,0,false);
        }
        assert(ItemRenderer::getAtlasPos(NULL)==-1);
        // Known creative/recipe icons remain valid even at count zero.
        for(int id:{1,5,17,302,345,344,347,330}) {
            ItemInstance preview(id,0,0);assert(preview.getItem());
            assert(ItemRenderer::getAtlasPos(&preview)>=-1);assert(preview.getIcon()>=0);
        }
        std::cout<<"PASS: invalid world/direct-GUI item renders return before GL; creative zero-count known-icon lookup retained\n";return 0;
    }
    CompoundTag chestTag;auto* list=new ListTag();
    list->add(entry(0,-1,1));list->add(entry(1,Item::MAX_ITEMS-1,1));list->add(entry(2,1,255));
    list->add(entry(3,1,0));list->add(entry(4,5,7));list->add(entry(5,1,1,-1));
    chestTag.put("Items",list);ChestTileEntity chest;chest.load(&chestTag);
    for(int slot:{0,1,2,3,5})assert(!chest.getItem(slot)||chest.getItem(slot)->isNull());
    assert(chest.getItem(4)&&chest.getItem(4)->id==5&&chest.getItem(4)->count==7);
    CompoundTag empty;chest.load(&empty);assert(!chest.getItem(4)||chest.getItem(4)->isNull());
    CompoundTag furnaceTag;furnaceTag.putShort("BurnTime",-1);furnaceTag.putShort("CookTime",32767);auto* furnaceItems=new ListTag();furnaceItems->add(entry(0,1,255));
    furnaceItems->add(entry(1,-1,1));furnaceItems->add(entry(2,Item::ironIngot->id,3));furnaceTag.put("Items",furnaceItems);
    FurnaceTileEntity furnace;furnace.load(&furnaceTag);assert(furnace.items[0].isNull()&&furnace.items[1].isNull());
    assert(furnace.items[2].id==Item::ironIngot->id&&furnace.items[2].count==3);
    assert(furnace.litTime==0&&furnace.tickCount==0);
    auto* inventoryItems=new ListTag();inventoryItems->add(entry(0,255,255,14));
    inventoryItems->add(entry(14,5,7));inventoryItems->add(entry(15,1,255));
    inventoryItems->add(entry(16,Item::MAX_ITEMS-1,1));inventoryItems->add(entry(17,1,0));
    inventoryItems->add(entry(18,Item::pickAxe_wood->id,1,-1));
    FillingContainer inventory(45,9,ContainerType::INVENTORY,false);inventory.load(inventoryItems);
    assert(inventory.getLinked(0)&&inventory.getLinked(0)->id==5&&inventory.getLinked(0)->count==7);
    for(int slot:{15,16,17,18})assert(!inventory.getItem(slot)||inventory.getItem(slot)->isNull());
    Inventory creative(NULL,true);const auto palette=ItemInstance::cloneSafe(creative.getItem(9));creative.load(inventoryItems);
    assert(ItemInstance::cloneSafe(creative.getItem(9))==palette);
    CompoundTag marker;marker.putShort("id",255);marker.putByte("Count",static_cast<char>(255));marker.putShort("Damage",14);
    ItemInstance raw;raw.load(&marker);assert(raw.id==255&&raw.count==255&&raw.getAuxValue()==14);
    assert(ItemInstance::fromTag(&marker)==NULL);
    auto* oldLarge=entry(9,5,254);ItemInstance* preserved=ItemInstance::fromTag(oldLarge);
    assert(preserved&&preserved->count==254);delete preserved;delete oldLarge;
    auto* brokenTool=entry(9,Item::pickAxe_wood->id,1,Item::pickAxe_wood->getMaxDamage()+1);
    assert(ItemInstance::fromTag(brokenTool)==NULL);delete brokenTool;
    auto* level=new Level(new MemoryLevelStorage(),"saved-armor",LevelSettings(1234,GameType::Survival,LGV_ORIGINAL),LGV_ORIGINAL);
    SavedPlayer player(level);CompoundTag armorTag;auto* armorList=new ListTag();
    armorList->add(entry(0,Item::helmet_iron->id,1,4));armorList->add(entry(1,Item::chestplate_iron->id,255));
    armorList->add(entry(2,5,1));armorList->add(entry(3,Item::boots_iron->id,1,32767));armorTag.put("Armor",armorList);
    player.readAdditionalSaveData(&armorTag);
    assert(player.getArmor(0)&&player.getArmor(0)->id==Item::helmet_iron->id&&player.getArmor(0)->getAuxValue()==4);
    for(int slot:{1,2,3})assert(!player.getArmor(slot));
    delete inventoryItems;
    std::cout<<"PASS: malformed saved chest/furnace/inventory items filtered; valid contents, legacy hotbar markers and creative palette preserved\n";
}
