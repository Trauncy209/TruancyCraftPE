#define main lifetimeRegressionMain
#include "native_chunk_lifetime.cpp"
#undef main
#include "world/level/Region.h"
#include "world/level/chunk/ChunkCollectionGate.h"
#include "network/packet/ChunkDataPacket.h"
#include "client/Minecraft.h"
#include "client/User.h"
#include "client/player/LocalPlayer.h"
#define private public
#include "network/ClientSideNetworkHandler.h"
#undef private
#include <sys/resource.h>
#include <csignal>
#include <type_traits>
#include <thread>
#include <future>
#include <memory>

struct ProbeGame:Minecraft {};

static void snapshotPlateau() {
    auto* storage=new MemoryLevelStorage();
    auto* level=new Level(storage,"snapshot-plateau",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);
    level->setUpdateLights(false);level->isClientSide=true;
    auto* cache=dynamic_cast<ChunkCache*>(level->getChunkSource());assert(cache);
    auto* game=new ProbeGame();RakNetInstance wire;game->raknetInstance=&wire;game->user=new User("LongWalkProbe","");game->level=level;
    auto* player=new LocalPlayer(game,level,game->user,0,false);game->player=player;player->x=4;player->z=4;
    ClientSideNetworkHandler handler(game,&wire);const RakNet::RakNetGUID server(4321);handler.beginJoin();handler.onConnect(server);handler.levelGenerated(level);
    auto receive=[&](int x,int z){ChunkDataPacket packet;packet.x=x;packet.z=z;for(int col=0;col<256;++col)packet.chunkData.Write((unsigned char)0);handler.handle(server,&packet);};
    for(int x=0;x<32;++x)for(int z=0;z<32;++z)receive(x,z);
    for(int x=32;x<160;++x) {
        for(int z=0;z<32;++z)receive(x,z);
        assert(cache->ownedChunkCount()<=cache->residentCapacity()+32);
        handler.pruneChunkReceipts();level->collectRetiredChunks();
        assert(cache->ownedChunkCount()==cache->residentCapacity());
        assert(handler.receivedChunks.size()<=cache->residentCapacity());
        assert(cache->generatedChunkCount()==0);
    }
    std::cout<<"Snapshot long walk: 5120 actual receive-handler snapshots, 1024 live slots; chunk ownership and receipt bookkeeping plateau after every frame PASS\n";
    game->level=nullptr;game->player=nullptr;delete player;delete level;delete storage;
}
static void proceduralPlateau() {
    char path[]="/tmp/tcpe-procedural-plateau-XXXXXX";assert(mkdtemp(path));
    auto* storage=new ExternalFileLevelStorage("walk",path);
    auto* level=new Level(storage,"walk",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);level->setUpdateLights(false);
    auto* cache=dynamic_cast<ChunkCache*>(level->getChunkSource());assert(cache);
    for(int x=0;x<int(cache->residentCapacity())+128;++x) {
        LevelChunk* chunk=cache->getChunk(x,0);
        chunk->setTileRaw(8,90,8,Tile::wood->id);chunk->data.set(8,90,8,x&3);chunk->unsaved=true;
        level->collectRetiredChunks();
        assert(cache->ownedChunkCount()<=cache->residentCapacity()&&cache->generatedChunkCount()<=cache->residentCapacity());
    }
    LevelChunk* restored=cache->getChunk(0,0);
    assert(restored->getTile(8,90,8)==Tile::wood->id&&restored->getData(8,90,8)==0);
    level->collectRetiredChunks();assert(cache->ownedChunkCount()<=cache->residentCapacity());
    std::cout<<"Procedural long walk: generated/saved chunks exceed authority LRU budget; source map and owned snapshots plateau at resident capacity, edited data returns from disk PASS\n";
    delete level;delete storage;
}
static void viewAndPacketPins() {
    static_assert(!std::is_copy_constructible<Region>::value,"views cannot shallow-copy pins");
    static_assert(!std::is_copy_assignable<Region>::value,"views cannot shallow-assign pins");
    static_assert(std::is_nothrow_move_constructible<Region>::value,"view moves transfer pins");
    static_assert(std::is_nothrow_move_assignable<Region>::value,"view moves transfer pins");
    auto* storage=new MemoryLevelStorage();auto* level=new Level(storage,"pinned-views",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);
    level->setUpdateLights(false);level->isClientSide=true;auto* cache=dynamic_cast<ChunkCache*>(level->getChunkSource());assert(cache);
    LevelChunk* original=cache->create(0,0);original->setTileRaw(1,70,1,Tile::wood->id);
    {
        Region first(level,0,0,0,15,127,15);Region moved(std::move(first));
        cache->create(32,0);level->collectRetiredChunks();
        assert(cache->ownedChunkCount()==2&&moved.getTile(1,70,1)==Tile::wood->id);
        Region another(level,512,0,0,527,127,15);
        moved=std::move(another);level->collectRetiredChunks();assert(cache->ownedChunkCount()==1);
    }
    LevelChunk* serialized=cache->create(64,0);serialized->setTileRaw(1,70,1,Tile::wood->id);
    {
        ChunkDataPacket outgoing(64,0,serialized,true);
        ChunkViewPin held(serialized);ChunkViewPin copied=held;
        ChunkViewPin moved(std::move(copied));
        std::unique_ptr<ChunkDataPacket> queued(new ChunkDataPacket(64,0,serialized,true));
        cache->create(96,0);level->collectRetiredChunks();
        assert(cache->ownedChunkCount()==2);
        RakNet::BitStream bytes;queued->write(&bytes);assert(bytes.GetNumberOfBytesUsed()>1000);
    }
    level->collectRetiredChunks();assert(cache->ownedChunkCount()==1);
    std::cout<<"Region Rule-of-Five moves and copied leases/queued outgoing serializers retain snapshots across collection, then release them PASS\n";
    delete level;delete storage;
}
static void pendingWritePin() {
    char path[]="/tmp/tcpe-pinned-write-XXXXXX";assert(mkdtemp(path));
    auto* storage=new ExternalFileLevelStorage("pinned-write",path);
    auto* level=new Level(storage,"pinned-write",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);level->setUpdateLights(false);
    auto* cache=dynamic_cast<ChunkCache*>(level->getChunkSource());assert(cache);
    LevelChunk* modified=cache->getChunk(0,0);
    modified->setTileRaw(2,90,2,0);modified->setTileRaw(3,90,3,0);storage->save(level,modified); // Older disk state lacks A and B.
    modified->setTileRaw(2,90,2,Tile::wood->id);modified->data.set(2,90,2,1);
    modified->skyLight.set(2,90,2,4);modified->blockLight.set(2,90,2,12);modified->unsaved=true;
    level->setTile(1,90,1,Tile::chest->id);auto* chest=dynamic_cast<ChestTileEntity*>(level->getTileEntity(1,90,1));assert(chest);
    ItemInstance contents(Tile::wood,23);chest->setItem(0,&contents);
    auto* cow=new Cow(level);cow->moveTo(4.5f,90.f,4.5f,0,0);level->addEntity(cow);
    const uint64_t before=modified->residencyIdentity();
    // Fill the coordinate LRU, leaving the edited origin as its oldest entry.
    for(int x=1;x<int(cache->residentCapacity());++x)cache->getChunk(x,0);
    struct rlimit previous;assert(getrlimit(RLIMIT_FSIZE,&previous)==0);auto signalBefore=signal(SIGXFSZ,SIG_IGN);
    struct rlimit limit=previous;limit.rlim_cur=0;assert(setrlimit(RLIMIT_FSIZE,&limit)==0);
    cache->getChunk(int(cache->residentCapacity()),0);level->collectRetiredChunks();
    assert(modified->unsaved&&storage->hasPendingChunk(modified)&&modified->getTile(2,90,2)==Tile::wood->id);
    LevelChunk* reentered=cache->getChunk(0,0);
    assert(reentered->residencyIdentity()!=before);
    // Re-entry must read the newest queued bytes, not the older disk snapshot.
    assert(reentered->getTile(2,90,2)==Tile::wood->id&&reentered->getData(2,90,2)==1);
    assert(reentered->skyLight.get(2,90,2)==4&&reentered->blockLight.get(2,90,2)==12);
    assert(level->getTileEntity(1,90,1)==chest&&chest->getItem(0)->count==23);
    AABB bounds(0,85,0,10,100,10);auto entities=level->getEntities(nullptr,bounds);
    assert(std::count(entities.begin(),entities.end(),cow)==1);
    reentered->setTileRaw(3,90,3,Tile::stoneBrick->id);reentered->data.set(3,90,3,2);reentered->unsaved=true;
    level->collectRetiredChunks(); // The old pointer may now be reclaimed; queued fresh snapshot stays pinned.
    assert(storage->hasPendingChunk(reentered));
    assert(setrlimit(RLIMIT_FSIZE,&previous)==0);signal(SIGXFSZ,signalBefore);
    assert(storage->savePendingUnsavedChunks(-1)==2); // Both evicted coordinate snapshots were retained.
    level->collectRetiredChunks();assert(cache->ownedChunkCount()<=cache->residentCapacity());
    // Make origin oldest again and cross the real capacity, rather than
    // depending on the former modulo collision between chunks0 and32.
    for(int x=1;x<=int(cache->residentCapacity())+1;++x)cache->getChunk(x,0);
    level->collectRetiredChunks();assert(!cache->hasChunkNow(0,0));
    LevelChunk* fromDisk=cache->getChunk(0,0);
    assert(fromDisk->getTile(2,90,2)==Tile::wood->id&&fromDisk->getData(2,90,2)==1);
    assert(fromDisk->getTile(3,90,3)==Tile::stoneBrick->id&&fromDisk->getData(3,90,3)==2);
    assert(level->getTileEntity(1,90,1)==chest&&chest->getItem(0)->count==23);
    entities=level->getEntities(nullptr,bounds);assert(std::count(entities.begin(),entities.end(),cow)==1);
    std::cout<<"Failed write -> same-coordinate re-entry -> A+B edits -> retry: queued blocks/metadata/lights, mob membership and filled storage survive without stale entity copies PASS\n";
    delete level;delete storage;
}

static void generationAndWorkerBoundary() {
    using NativeChunkLifetime::mayCollectAtFrameEnd;
    assert(!mayCollectAtFrameEnd(true,true,false));
    assert(!mayCollectAtFrameEnd(false,true,true)); // Flag cleared, worker not joined yet.
    assert(!mayCollectAtFrameEnd(false,false,false));
    assert(mayCollectAtFrameEnd(false,false,true));
    auto* storage=new MemoryLevelStorage();auto* level=new Level(storage,"worker-pin",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);
    level->setUpdateLights(false);level->isClientSide=true;auto* cache=dynamic_cast<ChunkCache*>(level->getChunkSource());cache->create(0,0)->setTileRaw(1,70,1,Tile::wood->id);
    std::promise<void> ready,release;auto released=release.get_future();
    std::thread worker([&]{Region view(level,0,0,0,15,127,15);ready.set_value();released.wait();assert(view.getTile(1,70,1)==Tile::wood->id);});
    ready.get_future().wait();level->isGeneratingTerrain=true;
    cache->create(32,0);assert(level->collectRetiredChunks()==0&&cache->ownedChunkCount()==2);
    release.set_value();worker.join();level->isGeneratingTerrain=false;
    level->collectRetiredChunks();assert(cache->ownedChunkCount()==1);
    std::cout<<"Generation/worker-join gate and held worker Region prevent reclamation until actual reader completion PASS\n";
    delete level;delete storage;
}
static void receiptIdentity() {
    auto* game=new ProbeGame();RakNetInstance wire;game->raknetInstance=&wire;game->user=new User("ReceiptProbe","");
    auto* storage=new MemoryLevelStorage();auto* level=new Level(storage,"receipt",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);
    level->setUpdateLights(false);level->isClientSide=true;game->level=level;
    auto* player=new LocalPlayer(game,level,game->user,0,false);game->player=player;player->x=4;player->z=4;
    ClientSideNetworkHandler handler(game,&wire);const RakNet::RakNetGUID source(1234);handler.beginJoin();handler.onConnect(source);handler.levelGenerated(level);
    auto fillPacket=[](ChunkDataPacket& packet){packet.x=0;packet.z=0;for(int col=0;col<256;++col)packet.chunkData.Write((unsigned char)0);};
    ChunkDataPacket packet;fillPacket(packet);handler.handle(source,&packet);assert(handler.isChunkLoaded(0,0));
    const uint64_t old=handler.receivedChunks[std::make_pair(0,0)];auto* cache=dynamic_cast<ChunkCache*>(level->getChunkSource());
    cache->create(32,0);level->collectRetiredChunks();LevelChunk* blank=cache->create(0,0);
    assert(blank->residencyIdentity()!=old&&!handler.isChunkLoaded(0,0));
    handler.pruneChunkReceipts();assert(!handler.receivedChunks.count({0,0}));
    ChunkDataPacket replacement;fillPacket(replacement);handler.handle(source,&replacement);assert(handler.isChunkLoaded(0,0));
    assert(handler.receivedChunks[std::make_pair(0,0)]==blank->residencyIdentity());
    std::cout<<"Actual authoritative receipt identity survives deletion/address-reuse without treating a replacement blank as received terrain PASS\n";
    game->level=nullptr;game->player=nullptr;delete player;delete level;delete storage;
}
int main(){
    Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();TileEntity::initTileEntities();
    membership();destruction();fullCoordinates();viewAndPacketPins();pendingWritePin();generationAndWorkerBoundary();receiptIdentity();snapshotPlateau();proceduralPlateau();
    std::cout<<"BOUNDED NATIVE CHUNK LIFETIME ASAN REGRESSIONS PASS\n";
}
