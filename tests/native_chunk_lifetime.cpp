#include "world/level/Level.h"
#include "world/level/chunk/ChunkCache.h"
#include "world/level/storage/ExternalFileLevelStorage.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/levelgen/RandomLevelSource.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/entity/ChestTileEntity.h"
#include "world/level/tile/entity/FurnaceTileEntity.h"
#include "world/entity/MobCategory.h"
#include "world/entity/animal/Cow.h"
#include "world/entity/item/ItemEntity.h"
#include "world/entity/player/Inventory.h"
#include "world/item/Item.h"
#include "world/level/biome/Biome.h"
#include "network/RakNetInstance.h"
#include "util/Mth.h"
#include <cassert>
#include <iostream>
#include <set>
#include <map>
#include <unistd.h>

static bool contains(const EntityList& entities,Entity* value) {return std::find(entities.begin(),entities.end(),value)!=entities.end();}
static int count(Player* player,int id) {int total=0;for(int slot=9;slot<45;++slot){const auto* item=player->inventory->getItem(slot);if(item&&item->id==id)total+=item->count;}return total;}
static void evictOrigin(Level* level,ChunkCache* cache,Player* player) {
    // Players pin their simulation halo. Depart before testing genuine cold
    // chunk eviction; rejoin without adding a duplicate entity afterward.
    level->removePlayer(player);
    for(int x=1;x<=int(cache->residentCapacity())+1;++x)cache->getChunk(x,0);
    level->collectRetiredChunks();assert(!cache->hasChunkNow(0,0));
    level->players.push_back(player);
}
static void membership() {
    char path[]="/tmp/tcpe-chunk-membership-XXXXXX";assert(mkdtemp(path));
    auto* storage=new ExternalFileLevelStorage("membership",path);
    auto* level=new Level(storage,"membership",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);level->setUpdateLights(false);
    RakNetInstance wire;level->raknetInstance=&wire;
    LevelChunk* first=level->getChunk(0,0);
    for(int x=0;x<16;++x)for(int z=0;z<16;++z){first->setTileRaw(x,63,z,Tile::rock->id);for(int y=64;y<90;++y)first->setTileRaw(x,y,z,0);}
    auto* player=new Player(level,false);player->hasFakeInventory=false;player->inventory->clearInventory();player->moveTo(4.5f,64.f,4.5f,0,0);level->addEntity(player);
    auto* cow=new Cow(level);cow->moveTo(5.5f,64.f,4.5f,0,0);level->addEntity(cow);
    auto* item=new ItemEntity(level,4.5f,64.f,4.5f,ItemInstance(Item::leather,2));item->throwTime=0;level->addEntity(item);
    AABB query=player->bb.grow(3,3,3);
    assert(contains(level->getEntities(player,query),cow)&&contains(level->getEntities(player,query),item));
    auto* cache=dynamic_cast<ChunkCache*>(level->getChunkSource());assert(cache);
    { ChunkViewPin oldSnapshot(first);evictOrigin(level,cache,player);
      LevelChunk* returned=level->getChunk(0,0);assert(returned!=first); }
    level->tick(player,false);level->tick(cow,false);level->tick(item,false);
    auto nearby=level->getEntities(player,query);
    assert(contains(nearby,cow)&&contains(nearby,item));
    assert(std::count(nearby.begin(),nearby.end(),cow)==1&&std::count(nearby.begin(),nearby.end(),item)==1);
    ItemInstance sword(Item::sword_iron,1);player->inventory->setItem(9,&sword);player->inventory->linkSlot(0,9,false);
    const int health=cow->health;player->attack(cow);assert(cow->health<health);
    player->aiStep();assert(item->removed&&count(player,Item::leather->id)==2);
    // Re-entering the same chunk must retain storage identity and contents,
    // rather than making a second empty chest/furnace at those coordinates.
    level->setTile(1,64,1,Tile::chest->id);level->setTile(2,64,1,Tile::furnace->id);
    auto* chest=dynamic_cast<ChestTileEntity*>(level->getTileEntity(1,64,1));
    auto* furnace=dynamic_cast<FurnaceTileEntity*>(level->getTileEntity(2,64,1));assert(chest&&furnace);
    ItemInstance wood(Tile::wood,23);chest->setItem(0,&wood);furnace->setItem(0,&wood);
    evictOrigin(level,cache,player);level->getChunk(0,0);
    assert(level->getTileEntity(1,64,1)==chest&&level->getTileEntity(2,64,1)==furnace);
    assert(chest->getItem(0)->count==23&&furnace->getItem(0)->count==23);
    chest->setRemoved(); // Old invalidated object is still awaiting level cleanup.
    level->setTile(1,64,1,0);level->setTile(1,64,1,Tile::chest->id);
    auto* replacement=dynamic_cast<ChestTileEntity*>(level->getTileEntity(1,64,1));assert(replacement&&replacement!=chest);
    ItemInstance newerWood(Tile::wood,7);replacement->setItem(0,&newerWood);
    level->tickEntities();
    assert(level->getTileEntity(1,64,1)==replacement&&replacement->getItem(0)->count==7);
    for(int cycle=0;cycle<3;++cycle){evictOrigin(level,cache,player);level->getChunk(0,0);level->tick(cow,false);}
    assert(level->getTileEntity(1,64,1)==replacement&&replacement->getItem(0)->count==7);
    nearby=level->getEntities(player,query);assert(std::count(nearby.begin(),nearby.end(),cow)==1);
    std::cout<<"Actual disk-backed authority LRU eviction/re-entry: mob hitbox membership, attack, item pickup, no duplicates and filled chest/furnace identity PASS\n";
    delete level;delete storage;
}
static int created=0,destroyed=0;
struct CountedChunk:LevelChunk {
    CountedChunk(Level* level,int x,int z):LevelChunk(level,new unsigned char[16*16*128](),x,z){++created;terrainPopulated=true;}
    ~CountedChunk() override {++destroyed;}
};
struct MixedStorage:ChunkStorage {
    LevelChunk* load(Level* level,int x,int z) override {return x%2==0?new CountedChunk(level,x,z):nullptr;}
};
struct ReusedSource:ChunkSource {
    Level* level;std::map<std::pair<int,int>,LevelChunk*> generated;
    explicit ReusedSource(Level* l):level(l){}
    bool hasChunk(int,int) override{return true;}
    LevelChunk* getChunk(int x,int z) override {auto& chunk=generated[{x,z}];if(!chunk)chunk=new CountedChunk(level,x,z);return chunk;}
    LevelChunk* create(int x,int z) override{return getChunk(x,z);}
    void postProcess(ChunkSource*,int,int) override{}
    bool tick() override{return false;}bool shouldSave() override{return true;}
    Biome::MobList getMobsAt(const MobCategory&,int,int,int) override{return {};}
    std::string gatherStats() override{return "reused generated pointers";}
};
struct TestLevel:Level {
    using Level::Level;
    void replaceSource(ChunkSource* source){delete _chunkSource;_chunkSource=source;}
};
static void destruction() {
    const int beforeCreated=created,beforeDestroyed=destroyed;
    auto* storage=new MemoryLevelStorage();auto* level=new TestLevel(storage,"ownership",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);level->setUpdateLights(false);
    MixedStorage io;auto* source=new ReusedSource(level);auto* cache=new ChunkCache(level,&io,source);level->replaceSource(cache);
    const int visits=int(cache->residentCapacity())+96;
    for(int x=0;x<visits;++x){cache->getChunk(x,0);cache->collectRetiredChunks();}
    // Generated pointers may recur; loaded pointers are fresh on every visit.
    for(int cycle=0;cycle<3;++cycle){cache->getChunk(1,0);cache->getChunk(33,0);cache->getChunk(0,0);cache->getChunk(32,0);}
    const int allocated=created-beforeCreated;assert(allocated>visits);
    delete level;delete storage;
    assert(destroyed-beforeDestroyed==allocated);
    std::cout<<"Whole-world teardown frees every unique generated/loaded/retired chunk exactly once, including reused generated pointers PASS\n";
}
static void fullCoordinates() {
    std::set<ChunkPos> coordinates={ChunkPos(32767,0),ChunkPos(65535,0),ChunkPos(-1,0),ChunkPos(-32769,0)};assert(coordinates.size()==4);
    auto* storage=new MemoryLevelStorage();auto* level=new Level(storage,"coordinate-source",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);level->setUpdateLights(false);
    auto* source=new RandomLevelSource(level,1234,LGV_INFINITE,false);
    LevelChunk* a=source->getChunk(32767,0);LevelChunk* b=source->getChunk(65535,0);
    assert(a!=b&&a->isAt(32767,0)&&b->isAt(65535,0));
    LevelChunk* negative=source->getChunk(-1,0);LevelChunk* farNegative=source->getChunk(-32769,0);
    assert(negative!=farNegative&&negative->isAt(-1,0)&&farNegative->isAt(-32769,0));
    delete source;for(auto* chunk:{a,b,negative,farNegative}){chunk->deleteBlockData();delete chunk;}
    delete level;delete storage;
    std::cout<<"Procedural chunk source keeps full signed coordinate identity beyond the old 15-bit hash range PASS\n";
}
int main(){Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();TileEntity::initTileEntities();membership();destruction();fullCoordinates();}
