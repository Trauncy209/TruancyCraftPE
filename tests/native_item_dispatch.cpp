// Headless regression: network inventory snapshots may contain empty or
// unsupported item IDs. They must never dispatch through a null definition.
#include "world/item/Item.h"
#include "world/item/ItemInstance.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/entity/MobCategory.h"
#include "world/level/biome/Biome.h"
#include "util/Mth.h"
#include <cassert>
#include <iostream>

static void checkUnavailable(ItemInstance item) {
    const ItemInstance before(item);
    assert(!item.useOn(NULL, NULL, 0, 0, 0, 0, 0, 0, 0));
    assert(item.use(NULL, NULL) == &item);
    assert(item.getDestroySpeed(NULL) == 1.0f);
    assert(item.getAttackDamage(NULL) == 1);
    assert(!item.canDestroySpecial(NULL));
    item.hurtEnemy(NULL);
    item.interactEnemy(NULL);
    item.mineBlock(0, 0, 0, 0);
    item.releaseUsing(NULL, NULL, 0);
    assert(item.getUseDuration() == 0);
    assert(item.getUseAnimation() == UseAnim::none);
    assert(item.useTimeDepleted(NULL, NULL) == before);
    assert(item == before);
    assert(item.getMaxStackSize() >= 1);
}

int main() {
    Mth::initMth(); Material::initMaterials(); MobCategory::initMobCategories();
    Tile::initTiles(); Item::initItems(); Biome::initBiomes();
    checkUnavailable(ItemInstance());
    checkUnavailable(ItemInstance(-1, 1, 0));
    checkUnavailable(ItemInstance(Item::MAX_ITEMS, 1, 0));
    checkUnavailable(ItemInstance(Item::MAX_ITEMS - 1, 1, 0));
    checkUnavailable(ItemInstance(Item::bow, 0));
    ItemInstance bow(Item::bow, 1);
    assert(bow.getUseAnimation() == UseAnim::bow);
    assert(bow.getUseDuration() > 0);
    assert(bow.getMaxStackSize() == 1);
    ItemInstance apple(Item::apple, 1);
    assert(apple.getUseAnimation() == UseAnim::eat);
    assert(apple.getMaxStackSize() == 64);
    std::cout << "PASS: empty, zero-count and unsupported item action dispatch; valid bow and food behavior\n";
}
