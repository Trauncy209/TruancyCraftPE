#include "client/Minecraft.h"
#include "server/ServerPlayer.h"
#include "network/ServerSideNetworkHandler.h"
#include "network/RakNetInstance.h"
#include "network/packet/SendInventoryPacket.h"
#include "network/packet/ContainerSetSlotPacket.h"
#include "world/level/Level.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/entity/FurnaceTileEntity.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include "world/level/biome/Biome.h"
#include "world/inventory/FurnaceMenu.h"
#include "util/Mth.h"
#include <cassert>
#include <iostream>

struct Game : Minecraft {};
int main() {
    Mth::initMth(); Material::initMaterials(); MobCategory::initMobCategories();
    Tile::initTiles(); Item::initItems(); Biome::initBiomes();
    auto* level = new Level(new MemoryLevelStorage(), "packet-validation",
        LevelSettings(1234, GameType::Survival, LGV_INFINITE), LGV_INFINITE);
    level->setUpdateLights(false);
    auto* game = new Game(); RakNetInstance wire;
    game->level = level; game->raknetInstance = &wire; level->raknetInstance = &wire;
    ServerSideNetworkHandler server(game, &wire); server.levelGenerated(level);
    auto* player = new ServerPlayer(game, level);
    player->owner = RakNet::RakNetGUID(12345); level->addEntity(player);
    player->inventory->clearInventory();
    for (int i=0; i<Inventory::MAX_SELECTION_SIZE; ++i)
        assert(player->inventory->linkedSlots[i].inventorySlot == -1);
    ItemInstance leather(Item::leather, 5); player->inventory->setItem(9, &leather);
    player->inventory->linkSlot(0, 9, false);
    SendInventoryPacket good(player, false); good.items[0].count = 6;
    server.handle(player->owner, &good);
    assert(player->inventory->getItem(9)->count == 6);
    auto unchanged = [&]() { assert(player->inventory->getItem(9)->count == 6); };
    auto bad = good; bad.numItems = -1; server.handle(player->owner, &bad); unchanged();
    bad = good; bad.numItems = 0; server.handle(player->owner, &bad); unchanged();
    bad = good; bad.items.resize(2); server.handle(player->owner, &bad); unchanged();
    bad = good; bad.items[0] = ItemInstance(Item::MAX_ITEMS - 1, 1, 0);
    server.handle(player->owner, &bad); unchanged();
    bad = good; bad.items[0].count = 255; server.handle(player->owner, &bad); unchanged();
    bad = good; bad.items[36] = ItemInstance(1, 1, 0);
    server.handle(player->owner, &bad); unchanged();
    bad = good; bad.hotbar.slots[0] = -2; server.handle(player->owner, &bad); unchanged();
    bad = good; bad.hotbar.selected = 9; server.handle(player->owner, &bad); unchanged();
    bad = good; bad.extra = 128; server.handle(player->owner, &bad); unchanged();
    bad = good; bad.items[0].count = 7;
    server.handle(RakNet::RakNetGUID(9999), &bad); unchanged();

    RakNet::BitStream truncated;
    truncated.Write(player->entityId); truncated.Write((unsigned char)0); truncated.Write((short)36);
    SendInventoryPacket decoded; decoded.read(&truncated);
    assert(!decoded.valid); server.handle(player->owner, &decoded); unchanged();
    RakNet::BitStream encoded; good.write(&encoded);
    RakNet::MessageID id; encoded.Read(id); decoded.read(&encoded);
    assert(decoded.valid && decoded.numItems == 36 && decoded.hotbar.present);
    auto legacy = good; legacy.hotbar.present = false; legacy.items[0].count = 7;
    server.handle(player->owner, &legacy);
    assert(player->inventory->getSelected() && player->inventory->getSelected()->count == 7);
    server.handle(player->owner, &good); unchanged();
    auto* furnace = new FurnaceTileEntity();
    player->openFurnace(furnace);
    player->containerMenu->containerId = 7;
    ContainerSetSlotPacket validSlot(7, 0, ItemInstance(1, 3, 0));
    server.handle(player->owner, &validSlot);
    assert(furnace->getItem(0)->id == 1 && furnace->getItem(0)->count == 3);
    for (int index : {-1, 3, 32767}) {
        ContainerSetSlotPacket invalidSlot(7, index, ItemInstance(1, 8, 0));
        server.handle(player->owner, &invalidSlot);
        assert(furnace->getItem(0)->count == 3);
    }
    ContainerSetSlotPacket invalidItem(7, 0, ItemInstance(Item::MAX_ITEMS - 1, 1, 0));
    server.handle(player->owner, &invalidItem); assert(furnace->getItem(0)->count == 3);
    RakNet::BitStream shortSlot;
    shortSlot.Write((unsigned char)7); shortSlot.Write((short)0);
    ContainerSetSlotPacket decodedSlot; decodedSlot.read(&shortSlot);
    assert(!decodedSlot.valid); server.handle(player->owner, &decodedSlot);
    assert(furnace->getItem(0)->count == 3);
    std::cout << "Malformed inventory/slot packets rejected before state mutation; valid round trip PASS\n";
}
