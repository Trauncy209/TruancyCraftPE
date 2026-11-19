#include "client/Minecraft.h"
#include "client/player/LocalPlayer.h"
#include "network/ClientSideNetworkHandler.h"
#include "network/RakNetInstance.h"
#include "network/packet/ContainerSetContentPacket.h"
#include "network/packet/ContainerSetSlotPacket.h"
#include "world/level/Level.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/entity/FurnaceTileEntity.h"
#include "world/inventory/FurnaceMenu.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include "world/level/biome/Biome.h"
#include "util/Mth.h"
#include <cassert>
#include <iostream>
struct Game : Minecraft { Game() : Minecraft() {} };
int main() {
    Mth::initMth(); Material::initMaterials(); MobCategory::initMobCategories();
    Tile::initTiles(); Item::initItems(); Biome::initBiomes();
    auto* level = new Level(new MemoryLevelStorage(), "container-update", LevelSettings(1234, GameType::Survival, LGV_INFINITE), LGV_INFINITE);
    auto* game = new Game(); game->level = level;
    RakNetInstance wire; game->raknetInstance = &wire;
    auto* player = new LocalPlayer(game, level, NULL, 0, false); game->player = player;
    ClientSideNetworkHandler handler(game, &wire);
    FurnaceTileEntity furnace; FurnaceMenu menu(&furnace); menu.containerId = 7; player->containerMenu = &menu;
    const RakNet::RakNetGUID server = RakNet::UNASSIGNED_RAKNET_GUID, foreign(12345);
    ItemInstance stone(1, 4, 0), wood(17, 2, 0);
    ContainerSetContentPacket content(7, std::vector<ItemInstance>(3, stone));
    handler.handle(server, &content); assert(furnace.items[0] == stone && furnace.items[2] == stone);
    content.items.assign(4, wood); handler.handle(server, &content);
    assert(furnace.items[0] == stone && furnace.items[2] == stone);
    content.items.assign(3, wood); handler.handle(foreign, &content); assert(furnace.items[0] == stone);
    content.valid = false; handler.handle(server, &content); assert(furnace.items[0] == stone);
    content.valid = true; handler.handle(server, &content); assert(furnace.items[0] == wood);
    ContainerSetSlotPacket slot(7, 2, stone); handler.handle(server, &slot); assert(furnace.items[2] == stone);
    slot.item = wood; handler.handle(foreign, &slot); assert(furnace.items[2] == stone);
    slot.valid = false; handler.handle(server, &slot); assert(furnace.items[2] == stone);
    slot.valid = true; slot.slot = -1; handler.handle(server, &slot);
    slot.slot = 3; handler.handle(server, &slot); assert(furnace.items[0] == wood && furnace.items[2] == stone);
    slot.slot = 32767; handler.handle(server, &slot);
    ContainerSetContentPacket inventory(0, std::vector<ItemInstance>(130, stone));
    player->inventory->setItem(9, &wood);
    handler.handle(server, &inventory); assert(*player->inventory->getItem(9) == wood);
    RakNet::BitStream encoded; ContainerSetContentPacket good(7, std::vector<ItemInstance>(3, stone)); good.write(&encoded);
    // Every truncated header/item prefix must fail without partial mutation.
    for (unsigned int length = 1; length < encoded.GetNumberOfBytesUsed(); ++length) {
        RakNet::BitStream prefix(encoded.GetData() + 1, length - 1, false);
        ContainerSetContentPacket parsed; parsed.items.push_back(wood); parsed.read(&prefix);
        assert(!parsed.valid && parsed.items.empty());
    }
    RakNet::BitStream full(encoded.GetData() + 1, encoded.GetNumberOfBytesUsed() - 1, false);
    ContainerSetContentPacket parsed; parsed.read(&full); assert(parsed.valid && parsed.items.size() == 3 && parsed.items[2] == stone);
    RakNet::BitStream huge; huge.Write((unsigned char)7); huge.Write((short)32767);
    parsed.read(&huge); assert(!parsed.valid && parsed.items.empty());
    RakNet::BitStream negative; negative.Write((unsigned char)7); negative.Write((short)-1);
    parsed.read(&negative); assert(!parsed.valid && parsed.items.empty());
    std::cout << "PASS: bounded content parsing, actual furnace updates, rejected oversized/negative/truncated/foreign/invalid container updates\n";
}
