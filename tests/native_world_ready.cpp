// Native worlds must wait for the main-thread renderer/camera handoff, just as
// Java worlds do. A worker finishing between ticks must not expose a null level
// in LevelRenderer::renderSky.
#include <sstream>
#define private public
#include "client/Minecraft.h"
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
struct Game : Minecraft { using Minecraft::isGeneratingLevel; using Minecraft::_hasSignaledGeneratingLevelFinished; };
int main() {
 Mth::initMth(); Material::initMaterials(); MobCategory::initMobCategories();
 Tile::initTiles(); Item::initItems(); Biome::initBiomes();
 auto* game=new Game();
 auto* level=new Level(new MemoryLevelStorage(),"ready",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);
 game->level=level;
 game->isGeneratingLevel=true;game->_hasSignaledGeneratingLevelFinished=false;
 assert(!game->isLevelGenerated());
 game->isGeneratingLevel=false;
 assert(!game->isLevelGenerated()); // Worker finished; renderer not attached yet.
 game->_hasSignaledGeneratingLevelFinished=true;
 assert(game->isLevelGenerated());
 game->level=NULL;assert(!game->isLevelGenerated());
 std::cout<<"Native world render readiness waits for worker and main-thread handoff PASS\n";
}
