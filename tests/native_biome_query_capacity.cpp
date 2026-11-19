#include "world/level/Level.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/level/biome/Biome.h"
#include "world/level/biome/BiomeSource.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include "util/Mth.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main(){
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
 MemoryLevelStorage storage;
 Level level(&storage,"noise",LevelSettings(771234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);
 BiomeSource queried(&level),reference(&level);
 queried.getBiome(-17,33); // Formerly allocates only one float for downfall/noise.
 auto* expected=reference.getBiomeBlock(-32,16,16,16);auto* actual=queried.getBiomeBlock(-32,16,16,16);
 for(int i=0;i<256;++i){assert(expected[i]==actual[i]);assert(reference.temperatures[i]==queried.temperatures[i]);assert(reference.downfalls[i]==queried.downfalls[i]);}
 for(int pass=0;pass<64;++pass){
  queried.getBiome(pass-32,pass+8);
  const int w=pass%2?32:8,h=pass%2?8:32;
  actual=queried.getBiomeBlock(-32,16,w,h);expected=reference.getBiomeBlock(-32,16,w,h);
  for(int i=0;i<w*h;++i){assert(actual[i]&&actual[i]==expected[i]);assert(queried.temperatures[i]==reference.temperatures[i]);}
  auto* temperatures=queried.getTemperatureBlock(-32,16,32,32);assert(temperatures&&queried.lenTemperatures>=1024&&queried.lenNoises>=1024);
 }
 std::cout<<"Single-biome to chunk/rectangular/weather noise queries preserve values and grow all scratch buffers PASS\n";
}
