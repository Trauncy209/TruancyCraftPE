#include <sstream>
#include <cassert>
#include <iostream>
#include "client/Minecraft.h"
#include "client/gamemode/SurvivalMode.h"
#include "server/ServerPlayer.h"
#define private public
#include "network/ServerSideNetworkHandler.h"
#undef private
#include "network/RakNetInstance.h"
#include "network/packet/LoginPacket.h"
#include "network/packet/ReadyPacket.h"
#include "world/level/Level.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include "world/level/biome/Biome.h"
#include "util/Mth.h"

struct Game : Minecraft {};
struct Storage : MemoryLevelStorage {
    int loads = 0, saves = 0;
    int loadNetworkPlayer(Player* player) override {
        ++loads;
        player->setPos(8, 160, 8); // Air above the world: no terrain fixture needed.
        return 1;
    }
    bool saveNetworkPlayer(Player*) override { ++saves; return true; }
};
struct Pending : ServerPlayer {
    int* destroyed;
    Pending(Minecraft* game, Level* level, int* destroyed)
        : ServerPlayer(game, level), destroyed(destroyed) {}
    ~Pending() { ++*destroyed; }
};
int main() {
    Mth::initMth(); Material::initMaterials(); MobCategory::initMobCategories();
    Tile::initTiles(); Item::initItems(); Biome::initBiomes();
    auto* storage = new Storage();
    auto* level = new Level(storage, "pending-disconnect",
        LevelSettings(1234, GameType::Survival, LGV_INFINITE), LGV_INFINITE);
    level->setUpdateLights(false);
    auto* game = new Game(); RakNetInstance wire;
    game->level = level; game->raknetInstance = &wire; level->raknetInstance = &wire;
    game->gameMode = new SurvivalMode(game);
    ServerSideNetworkHandler server(game, &wire); server.levelGenerated(level);
    server._allowIncoming = true;
    const RakNet::RakNetGUID first(1234), retry(5678), foreign(9999);
    LoginPacket login("ReconnectQA", SharedConstants::NetworkProtocolVersion);
    server.handle(first, &login);
    assert(server._pendingPlayers.size() == 1 && level->players.empty());
    assert(storage->loads == 1 && storage->saves == 0);
    server.handle(retry, &login); // Active pending sessions still reserve the name.
    assert(server._pendingPlayers.size() == 1 && storage->loads == 1);
    server.onDisconnect(first);
    assert(server._pendingPlayers.empty() && storage->saves == 0);
    server.handle(retry, &login); // Same name, different transport, before any Ready.
    assert(server._pendingPlayers.size() == 1 && storage->loads == 2);
    assert(server._pendingPlayers[0]->owner == retry);
    server.onReady_ClientGeneration(first); // A late Ready cannot resurrect the old session.
    assert(server._pendingPlayers.size() == 1 && level->players.empty());
    server.onDisconnect(foreign);
    assert(server._pendingPlayers.size() == 1);
    server.onDisconnect(retry);
    assert(server._pendingPlayers.empty() && storage->saves == 0);
    for (int i = 0; i < 10; ++i) {
        const RakNet::RakNetGUID owner(10000 + i);
        server.handle(owner, &login);
        assert(server._pendingPlayers.size() == 1);
        server.onDisconnect(owner);
        assert(server._pendingPlayers.empty());
    }
    assert(storage->loads == 12 && storage->saves == 0);
    int destroyed = 0;
    auto* pending = new Pending(game, level, &destroyed);
    pending->owner = first; server._pendingPlayers.push_back(pending);
    server.onDisconnect(first);
    assert(server._pendingPlayers.empty() && destroyed == 1 && storage->saves == 0);
    std::cout << "Disconnect before Ready releases pending player/name without saving; repeated same-name reconnect PASS\n";
}
