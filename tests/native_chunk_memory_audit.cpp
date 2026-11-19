// Diagnostic probe, not a release gate: reports resident native chunk memory
// across ring-cache replacements without opening a game window or user world.
#include "world/level/Level.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include "world/level/biome/Biome.h"
#include "util/Mth.h"
#include <malloc.h>
#include <iostream>
#include <cassert>
static size_t heapBytes() {const auto info=mallinfo2();return info.uordblks+info.hblkhd;}
int main() {
    Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
    auto* storage=new MemoryLevelStorage();auto* level=new Level(storage,"memory-probe",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);
    level->setUpdateLights(false);level->isClientSide=true;auto* source=level->getChunkSource();
    for(int x=0;x<32;++x)source->create(x,0);
    const size_t warm=heapBytes();
    for(int x=32;x<288;++x)source->create(x,0);
    const size_t traveled=heapBytes();int resident=0;
    for(int x=0;x<288;++x)if(source->hasChunkNow(x,0))++resident;
    assert(resident<=32);
    delete level;delete storage;const size_t afterClose=heapBytes();
    std::cout<<"native_chunk_memory_audit resident="<<resident<<" additionalSnapshots=256 heapWarm="<<warm<<" heapAfterTravel="<<traveled<<" growth="<<(traveled-warm)<<" heapAfterLevelClose="<<afterClose<<"\n";
}
