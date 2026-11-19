#include <sstream>
#define private public
#include "client/Minecraft.h"
#undef private
#include "client/User.h"
#include "network/ClientSideNetworkHandler.h"
#include "network/RakNetInstance.h"
#include "network/packet/ReadyPacket.h"
#include "network/packet/RequestChunkPacket.h"
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
struct Game : Minecraft {
    using Minecraft::isGeneratingLevel;
    using Minecraft::_hasSignaledGeneratingLevelFinished;
    using Minecraft::_nativeLeavePending;
    Game() : Minecraft() { user = new User("CancelProbe", ""); screenMutex = true; }
};
struct Wire : IRakNetInstance {
    RakNetInstance peer; int ready = 0, chunks = 0;
    RakNet::RakPeerInterface* getPeer() override { return peer.getPeer(); }
    void send(Packet& packet) override {
        if (dynamic_cast<ReadyPacket*>(&packet)) ++ready;
        if (dynamic_cast<RequestChunkPacket*>(&packet)) ++chunks;
    }
};
struct Storage : MemoryLevelStorage {
    int* saves; explicit Storage(int* saves) : saves(saves) {}
    void saveGame(Level*) override { ++*saves; }
};
struct JavaLevel : Level {
    JavaLevel() : Level(new MemoryLevelStorage(), "java-guard", LevelSettings(1234, GameType::Survival, LGV_INFINITE), LGV_INFINITE) { isClientSide = true; }
    bool isJavaServerLevel() const override { return true; }
};
int main() {
    Mth::initMth(); Material::initMaterials(); MobCategory::initMobCategories();
    Tile::initTiles(); Item::initItems(); Biome::initBiomes();
    auto* game = new Game(); Wire wire; game->raknetInstance = &wire;
    int saves = 0;
    auto* level = new Level(new Storage(&saves), "cancel", LevelSettings(1234, GameType::Survival, LGV_INFINITE), LGV_INFINITE);
    level->isClientSide = true; game->level = level;
    auto* handler = new ClientSideNetworkHandler(game, &wire); game->netCallback = handler;
    handler->beginJoin(); handler->onConnect(RakNet::RakNetGUID(1234)); handler->cancelJoin();
    game->isGeneratingLevel = true; game->_hasSignaledGeneratingLevelFinished = false;
    // OK while the detached worker is alive must preserve its snapshot and
    // retain the request, without emitting Ready or requesting any chunks.
    game->leaveGame(); assert(game->level == level && game->_nativeLeavePending);
    handler->levelGenerated(level); assert(wire.ready == 0 && wire.chunks == 0);
    game->isGeneratingLevel = false;
    game->tick(0, 0);
    assert(game->level == NULL && game->netCallback == NULL);
    assert(!game->_nativeLeavePending && game->_hasSignaledGeneratingLevelFinished);
    assert(wire.ready == 0 && wire.chunks == 0 && saves == 0);
    // Local generation retains its existing save/lifetime guard.
    auto* local = new Level(new MemoryLevelStorage(), "local", LevelSettings(1234, GameType::Survival, LGV_INFINITE), LGV_INFINITE);
    game->level = local; game->isGeneratingLevel = true; game->_hasSignaledGeneratingLevelFinished = false;
    game->leaveGame(); assert(game->level == local && !game->_nativeLeavePending);
    auto* java = new JavaLevel(); game->level = java;
    game->leaveGame(); assert(game->level == java && !game->_nativeLeavePending);
    std::cout << "PASS: native cancel/OK during bootstrap defers cleanup until worker completion; no late Ready, chunk requests, or remote save; local/Java guards preserved\n";
}
