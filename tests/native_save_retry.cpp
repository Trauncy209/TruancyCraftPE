#include <vector>
#include <list>
#include <map>
#include <string>
#include <sstream>
#include "world/level/Level.h"
#define private public
#include "world/level/storage/ExternalFileLevelStorage.h"
#undef private
#include "world/level/storage/RegionFile.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/entity/player/Player.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include "world/level/biome/Biome.h"
#include "util/Mth.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <sys/resource.h>
#include <csignal>
#include <unistd.h>

struct RetryLevel : Level {
    using Level::Level;
    void fixtureSource(ChunkSource* source) { _chunkSource=source; }
};
struct SparseFixtureSource : ChunkSource {
    std::map<std::pair<int,int>,LevelChunk*> chunks;
    bool hasChunk(int x,int z) override { return chunks.count({x,z})!=0; }
    LevelChunk* getChunk(int x,int z) override {auto found=chunks.find({x,z});return found==chunks.end()?nullptr:found->second;}
    LevelChunk* create(int x,int z) override {return getChunk(x,z);}
    void postProcess(ChunkSource*,int,int) override {}
    bool tick() override {return false;}
    bool shouldSave() override {return false;}
    Biome::MobList getMobsAt(const MobCategory&,int,int,int) override {return {};}
    std::string gatherStats() override {return "sparse test chunks";}
};
static void coordinateIdentity() {
    char directory[]="/tmp/tcpe-save-coordinates-XXXXXX";assert(mkdtemp(directory));
    auto* store=new ExternalFileLevelStorage("coordinates",directory);
    auto* level=new RetryLevel(store,"coordinates",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);level->setUpdateLights(false);
    auto* firstPlayer=new Player(level,false);auto* secondPlayer=new Player(level,false);
    firstPlayer->x=-16;firstPlayer->z=0;secondPlayer->x=65535.f*16;secondPlayer->z=0;
    level->players={firstPlayer,secondPlayer};
    auto* source=new SparseFixtureSource();level->fixtureSource(source);
    auto* one=new LevelChunk(level,new unsigned char[16*16*128](),-1,0);
    auto* two=new LevelChunk(level,new unsigned char[16*16*128](),65535,0);
    one->getBlockData()[70]=Tile::wood->id;two->getBlockData()[70]=Tile::dirt->id;
    one->unsaved=two->unsaved=true;source->chunks[{-1,0}]=one;source->chunks[{65535,0}]=two;
    struct rlimit previous;assert(getrlimit(RLIMIT_FSIZE,&previous)==0);auto oldSignal=signal(SIGXFSZ,SIG_IGN);
    struct rlimit limited=previous;limited.rlim_cur=0;assert(setrlimit(RLIMIT_FSIZE,&limited)==0);
    store->tickCount=49;store->lastSavedEntitiesTick=50;store->tick();
    assert(store->unsavedChunkList.size()==2); // -1 and 65535 used to collide.
    auto* newest=new LevelChunk(level,new unsigned char[16*16*128](),-1,0);newest->getBlockData()[70]=Tile::stoneBrick->id;newest->unsaved=true;
    source->chunks[{-1,0}]=newest;two->unsaved=false;
    store->tickCount=99;store->lastSavedEntitiesTick=100;store->tick();
    assert(store->unsavedChunkList.size()==2);bool sawNew=false,sawFar=false;
    for(const auto& pending:store->unsavedChunkList) {assert(pending.chunk!=one);sawNew|=pending.chunk==newest;sawFar|=pending.chunk==two;}
    assert(sawNew&&sawFar);
    assert(setrlimit(RLIMIT_FSIZE,&previous)==0);signal(SIGXFSZ,oldSignal);
    assert(store->savePendingUnsavedChunks(-1)==2&&store->unsavedChunkList.empty());
    auto* reader=new ExternalFileLevelStorage("coordinates",directory);
    assert(reader->load(level,-1,0)->getTile(0,70,0)==Tile::stoneBrick->id);
    assert(reader->load(level,65535,0)->getTile(0,70,0)==Tile::dirt->id);
    std::cout<<"Actual tick enqueue: negative/far coordinate key collisions remain distinct and reloaded coordinate replaces stale queued pointer PASS\n";
}
int main() {
    Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
    char directory[]="/tmp/tcpe-save-retry-XXXXXX";assert(mkdtemp(directory));
    auto* store=new ExternalFileLevelStorage("retry",directory);
    auto* level=new Level(store,"retry",LevelSettings(1234,GameType::Survival,LGV_ORIGINAL),LGV_ORIGINAL);level->setUpdateLights(false);
    auto* blocks=new unsigned char[16*16*128]();blocks[70]=Tile::wood->id;
    LevelChunk chunk(level,blocks,0,0);chunk.data.set(0,70,0,2);
    memset(chunk.skyLight.data,0xff,chunk.skyLight.length);memset(chunk.blockLight.data,0,chunk.blockLight.length);
    chunk.updateMap[0]=0x10;chunk.unsaved=false;
    UnsavedLevelChunk pending;pending.pos=0;pending.addedToList=0;pending.chunk=&chunk;store->unsavedChunkList.push_back(pending);
    struct rlimit previous;assert(getrlimit(RLIMIT_FSIZE,&previous)==0);
    auto oldSignal=signal(SIGXFSZ,SIG_IGN);struct rlimit limit=previous;limit.rlim_cur=4096;
    assert(setrlimit(RLIMIT_FSIZE,&limit)==0);
    const int failed=store->savePendingUnsavedChunks(-1);
    assert(setrlimit(RLIMIT_FSIZE,&previous)==0);signal(SIGXFSZ,oldSignal);
    assert(failed==0&&chunk.unsaved&&store->unsavedChunkList.size()==1&&store->unsavedChunkList.front().chunk==&chunk);
    assert(store->savePendingUnsavedChunks(-1)==1&&!chunk.unsaved&&store->unsavedChunkList.empty());
    assert(store->savePendingUnsavedChunks(-1)==0);
    auto* reader=new ExternalFileLevelStorage("retry",directory);auto* loaded=reader->load(level,0,0);
    assert(loaded&&loaded->getTile(0,70,0)==Tile::wood->id&&loaded->getData(0,70,0)==2);
    assert(memcmp(loaded->getBlockData(),chunk.getBlockData(),16*16*128)==0);
    assert(memcmp(loaded->data.data,chunk.data.data,16*16*128/2)==0);
    assert(memcmp(loaded->skyLight.data,chunk.skyLight.data,16*16*128/2)==0);
    assert(memcmp(loaded->blockLight.data,chunk.blockLight.data,16*16*128/2)==0);
    assert(memcmp(loaded->updateMap,chunk.updateMap,256)==0);
    // A newer successful direct save must retire the old coordinate's queue,
    // otherwise a later flush can roll the disk back to stale modifications.
    store->unsavedChunkList.push_back(pending);
    auto* newerBlocks=new unsigned char[16*16*128]();newerBlocks[70]=Tile::dirt->id;
    LevelChunk newer(level,newerBlocks,0,0);store->save(level,&newer);
    assert(!newer.unsaved&&store->unsavedChunkList.empty()&&store->savePendingUnsavedChunks(-1)==0);
    delete reader;reader=new ExternalFileLevelStorage("retry",directory);
    assert(reader->load(level,0,0)->getTile(0,70,0)==Tile::dirt->id);
    // A direct/cache save with no queue entry must retain its raw pointer too,
    // and a later failed write at the same coordinates supersedes that pointer.
    auto* directBlocks=new unsigned char[16*16*128]();directBlocks[70]=Tile::wood->id;
    LevelChunk direct(level,directBlocks,1,0);
    assert(getrlimit(RLIMIT_FSIZE,&previous)==0);oldSignal=signal(SIGXFSZ,SIG_IGN);limit=previous;limit.rlim_cur=0;
    assert(setrlimit(RLIMIT_FSIZE,&limit)==0);store->save(level,&direct);
    assert(direct.unsaved&&store->unsavedChunkList.size()==1&&store->unsavedChunkList.front().chunk==&direct);
    auto* latestBlocks=new unsigned char[16*16*128]();latestBlocks[70]=Tile::dirt->id;
    LevelChunk latest(level,latestBlocks,1,0);store->save(level,&latest);
    assert(latest.unsaved&&store->unsavedChunkList.size()==1&&store->unsavedChunkList.front().chunk==&latest);
    assert(setrlimit(RLIMIT_FSIZE,&previous)==0);signal(SIGXFSZ,oldSignal);
    assert(store->savePendingUnsavedChunks(1)==1&&!latest.unsaved&&store->unsavedChunkList.empty());
    delete reader;reader=new ExternalFileLevelStorage("retry",directory);
    assert(reader->load(level,1,0)->getTile(0,70,0)==Tile::dirt->id);
    // An inaccessible region directory is also a failure, not a clean chunk.
    const std::string blocked=std::string(directory)+"/regular-file";
    {std::ofstream file(blocked);file<<"original";}
    ExternalFileLevelStorage unavailable("unavailable",blocked);
    newer.unsaved=false;unavailable.save(level,&newer);assert(newer.unsaved);
    assert(unavailable.unsavedChunkList.size()==1&&unavailable.unsavedChunkList.front().chunk==&newer);
    std::ifstream input(blocked);std::string original;input>>original;assert(original=="original");
    coordinateIdentity();
    std::cout<<"Actual queued chunk save: write/open failure retains dirty data and queue, bounded retry commits once, byte-exact reopen, correct saved count and stale-queue supersession PASS\n";
}
