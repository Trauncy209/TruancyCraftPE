#include <cassert>
#include <iostream>
#include <sstream>
#define private public
#include "client/Minecraft.h"
#include "network/ClientSideNetworkHandler.h"
#undef private
#include "client/player/LocalPlayer.h"
#include "client/multiplayer/MultiPlayerLevel.h"
#include "server/ServerPlayer.h"
#include "network/RakNetInstance.h"
#include "network/packet/AddItemEntityPacket.h"
#include "network/packet/TakeItemEntityPacket.h"
#include "network/packet/ContainerSetContentPacket.h"
#include "network/packet/DropItemPacket.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include "world/level/biome/Biome.h"
#include "util/Mth.h"

struct Game : Minecraft {};
struct Wire : RakNetInstance {
    int snapshots = 0, updates = 0, takes = 0, drops = 0;
    void send(const RakNet::RakNetGUID&, Packet& packet) override {
        if (dynamic_cast<ContainerSetContentPacket*>(&packet)) ++snapshots;
    }
    void send(Packet& packet) override {
        if (dynamic_cast<AddItemEntityPacket*>(&packet)) ++updates;
        if (dynamic_cast<TakeItemEntityPacket*>(&packet)) ++takes;
        if (dynamic_cast<DropItemPacket*>(&packet)) ++drops;
    }
};

int main() {
    Mth::initMth(); Material::initMaterials(); MobCategory::initMobCategories();
    Tile::initTiles(); Item::initItems(); Biome::initBiomes();
    auto* level = new MultiPlayerLevel(new MemoryLevelStorage(), "pickup",
        LevelSettings(1234, GameType::Survival, LGV_INFINITE), LGV_INFINITE);
    level->setUpdateLights(false);
    auto* game = new Game();
    Wire wire;
    game->level = level; game->raknetInstance = &wire;
    level->raknetInstance = &wire;
    auto* player = new ServerPlayer(game, level);
    player->inventory->clearInventory();
    for (int slot = 9; slot < 45; ++slot) {
        ItemInstance full(1, 64, 0);
        player->inventory->setItem(slot, &full);
    }
    ItemInstance nearlyFull(Item::leather, 63);
    player->inventory->setItem(9, &nearlyFull);
    ItemEntity stack(level, 0, 70, 0, ItemInstance(Item::leather, 5));
    stack.playerTouch(player);
    assert(!stack.removed && stack.item.count == 4);
    assert(player->inventory->getItem(9)->count == 64);
    assert(wire.snapshots == 1 && wire.updates == 1 && wire.takes == 0);
    stack.playerTouch(player); // Nothing fits now: do not emit another pickup.
    assert(stack.item.count == 4 && wire.snapshots == 1);
    player->inventory->clearSlot(10);
    stack.playerTouch(player);
    assert(stack.removed && player->inventory->getItem(10)->count == 4);
    assert(wire.snapshots == 2 && wire.takes == 1);
    stack.playerTouch(player); // Removed entities cannot be collected twice.
    assert(player->inventory->getItem(10)->count == 4 && wire.snapshots == 2);
    std::cout << "Partial/full inventory pickup preserves exact quantities and broadcasts PASS\n";

    game->player = new LocalPlayer(game, level, NULL, 0, false);
    ClientSideNetworkHandler client(game, &wire);
    client.level = level;
    client.serverGuid = RakNet::RakNetGUID(1234);
    auto* remote = new ItemEntity(level, 0, 70, 0, ItemInstance(Item::leather, 5));
    level->putEntity(98765, remote);
    AddItemEntityPacket update(remote);
    update.itemCount = 4;
    client.handle(RakNet::RakNetGUID(9999), &update);
    assert(remote->item.count == 5);
    client.handle(RakNet::RakNetGUID(1234), &update);
    assert(level->getEntity(98765) == remote && remote->item.count == 4);
    TakeItemEntityPacket legacyTake(98765, game->player->entityId);
    client.handle(RakNet::RakNetGUID(9999), &legacyTake);
    assert(!game->player->inventory->getItem(9) && wire.drops == 0);
    client.handle(RakNet::RakNetGUID(1234), &legacyTake);
    assert(game->player->inventory->getItem(9)->count == 4);
    game->player->inventory->clearSlot(9);
    std::vector<ItemInstance> items(36, ItemInstance(1, 64, 0));
    ContainerSetContentPacket snapshot(0, items);
    snapshot.hotbar.capture(game->player->inventory);
    client.handle(RakNet::RakNetGUID(9999), &snapshot);
    assert(!client.authoritativeInventory && !game->player->inventory->getItem(9));
    client.handle(RakNet::RakNetGUID(1234), &snapshot);
    assert(client.authoritativeInventory);
    // No registered recipient avoids rendering a particle in this headless test.
    TakeItemEntityPacket take(98765, -123);
    client.handle(RakNet::RakNetGUID(1234), &take);
    assert(wire.drops == 0 && game->player->inventory->getItem(9)->count == 64);
    // Even a matching recipient must not add a speculative item to an empty slot.
    game->player->inventory->clearSlot(9);
    take.playerId = game->player->entityId;
    client.handle(RakNet::RakNetGUID(1234), &take);
    assert(!game->player->inventory->getItem(9) && wire.drops == 0);
    std::cout << "World stack refresh and authoritative pickup without client re-drop PASS\n";
}
