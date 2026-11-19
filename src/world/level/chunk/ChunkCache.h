#ifndef NET_MINECRAFT_WORLD_LEVEL_CHUNK__ChunkCache_H__
#define NET_MINECRAFT_WORLD_LEVEL_CHUNK__ChunkCache_H__

//package net.minecraft.world.level.chunk;

#include "ChunkSource.h"
#include <set>
#include <deque>
#include <map>
#include "../../entity/player/Player.h"
#include "../storage/ExternalFileLevelStorage.h"
#include "../levelgen/RandomLevelSource.h"
#include "storage/ChunkStorage.h"
#include "EmptyLevelChunk.h"
#include "../Level.h"
#include "../LevelConstants.h"
#include "../dimension/Dimension.h"

class ChunkCache: public ChunkSource {
    //static const int CHUNK_CACHE_WIDTH = CHUNK_CACHE_WIDTH; // WAS 32;
    static const int MAX_SAVES = 2;
    static const int INFINITE_CACHE_WIDTH = 32;
public:
    ChunkCache(Level* level_, ChunkStorage* storage_, ChunkSource* source_)
	:	xLast(-999999999),
		zLast(-999999999),
		last(NULL),
		cacheWidth(level_ && level_->getLevelData() && level_->getLevelData()->getGeneratorVersion() == LGV_INFINITE ? INFINITE_CACHE_WIDTH : CHUNK_CACHE_WIDTH),
		chunks(NULL),
		level(level_),
		storage(storage_),
		source(source_)
	{
		isChunkCache = true;
        //emptyChunk = new EmptyLevelChunk(level_, emptyChunkBlocks, 0, 0);
		emptyChunk = new EmptyLevelChunk(level_, NULL, 0, 0);
		chunks = new LevelChunk*[cacheWidth * cacheWidth];
		memset(chunks, 0, sizeof(LevelChunk*) * cacheWidth * cacheWidth);
    }

    ~ChunkCache() {
        // The procedural source's map is non-owning. Retired chunks can still
        // be referenced by queued saves and terrain views while the world is
        // alive, so release them exactly once only at whole-world teardown.
        delete source;
        for (int i=0;i<cacheWidth*cacheWidth;++i)
            if (chunks[i] && chunks[i]!=emptyChunk) ownedChunks.insert(chunks[i]);
        for (LevelChunk* chunk:ownedChunks) {
            chunk->deleteBlockData();
            delete chunk;
        }
        delete emptyChunk;
        delete[] chunks;
    }

    // Call only at a quiescent main-frame boundary, never from a lookup or
    // eviction. Region readers and failed disk writes retain their snapshots.
    size_t collectRetiredChunks(size_t budget=128) {
        trimAuthoritativeResidents();
        size_t removed=0;
        const size_t attempts=ownedChunks.size()>size_t(cacheWidth*cacheWidth*2)
            ?retiredChunks.size():std::min(budget,retiredChunks.size());
        for(size_t attempt=0;attempt<attempts;++attempt) {
            LevelChunk* chunk=retiredChunks.front();retiredChunks.pop_front();
            const int slot=(chunk->x&(cacheWidth-1))+(chunk->z&(cacheWidth-1))*cacheWidth;
            if(chunk==last||isResident(chunk)) {queuedRetirement.erase(chunk);continue;}
            bool discardable=!chunk->hasViews();
            if(!isNativeClient()) {
#if !defined(DEMO_MODE) && !defined(APPLE_DEMO_PROMOTION)
                auto* persistent=dynamic_cast<ExternalFileLevelStorage*>(storage);
                discardable=discardable&&persistent&&!chunk->unsaved&&!persistent->hasPendingChunk(chunk);
#else
                discardable=false;
#endif
            }
            auto* generated=dynamic_cast<RandomLevelSource*>(source);
            if(source&&!generated)discardable=false;
            if(!discardable) {retiredChunks.push_back(chunk);continue;}
            if(generated&&!generated->releaseRetiredChunk(chunk)) {retiredChunks.push_back(chunk);continue;}
            queuedRetirement.erase(chunk);ownedChunks.erase(chunk);
            chunk->deleteBlockData();delete chunk;++removed;
        }
        return removed;
    }
    size_t ownedChunkCount() const { return ownedChunks.size(); }
    size_t generatedChunkCount() const {
        auto* generated=dynamic_cast<RandomLevelSource*>(source);
        return generated?generated->cachedChunkCount():0;
    }
    size_t residentCapacity() const { return authorityCoordinates()?size_t(INFINITE_CACHE_WIDTH*INFINITE_CACHE_WIDTH)+level->players.size()*81:size_t(cacheWidth)*cacheWidth; }
    size_t residentChunkCount() const { return authorityCoordinates()?authorityChunks.size():ownedChunks.size()-retiredChunks.size(); }

    bool fits(int x, int z) {
        if (level->getLevelData()->getGeneratorVersion() == LGV_INFINITE)
            return true;
        return x >= 0 && z >= 0 && x < CHUNK_CACHE_WIDTH && z < CHUNK_CACHE_WIDTH;
    }

    bool hasChunk(int x, int z) {
        return hasChunkNow(x, z);
    }

    bool hasChunkNow(int x, int z) {
        if (!fits(x, z))
            return false;
        if(authorityCoordinates()) {
            auto found=authorityChunks.find(std::make_pair(x,z));
            return found!=authorityChunks.end()&&found->second&&found->second!=emptyChunk;
        }
        if (x == xLast && z == zLast && last != NULL) {
            return last != emptyChunk && last->isAt(x, z);
        }
		int xs = level->getLevelData()->getGeneratorVersion() == LGV_INFINITE ? (x & (cacheWidth - 1)) : x;
		int zs = level->getLevelData()->getGeneratorVersion() == LGV_INFINITE ? (z & (cacheWidth - 1)) : z;
		int slot = xs + zs * cacheWidth;
        return chunks[slot] != NULL && chunks[slot] != emptyChunk && chunks[slot]->isAt(x, z);
    }

    LevelChunk* create(int x, int z) {
        if (!isNativeClient() || hasChunkNow(x, z)) return getChunk(x, z);
        if (!fits(x, z)) return emptyChunk;
        const int slot = (x & (cacheWidth - 1)) + (z & (cacheWidth - 1)) * cacheWidth;
        if (chunks[slot]) { chunks[slot]->unload();retire(chunks[slot]); }
        LevelChunk* chunk = new LevelChunk(level,
            new unsigned char[LevelChunk::ChunkBlockCount](), x, z);
        chunk->terrainPopulated = true;
        memset(chunk->skyLight.data, level->dimension->hasCeiling ? 0 : 0xff,
               chunk->skyLight.length);
        ownedChunks.insert(chunk);
        chunks[slot] = chunk;
        chunk->load();
        xLast = x; zLast = z; last = chunk;
        return chunk;
    }

    LevelChunk* getChunk(int x, int z) {
		//static Stopwatch sw;
		//sw.start();

		if (!fits(x, z))
			return emptyChunk;
        // Rendering/collision reads must not allocate missing network chunks.
        // Only create(), called by an incoming server snapshot, can do that.
        if (isNativeClient() && !hasChunkNow(x, z)) return emptyChunk;
        if (authorityCoordinates()) authorityAccess[Coordinate(x,z)]=++accessClock;

		if (x == xLast && z == zLast && last != NULL) {
            return last;
        }
		int xs = level->getLevelData()->getGeneratorVersion() == LGV_INFINITE ? (x & (cacheWidth - 1)) : x;
		int zs = level->getLevelData()->getGeneratorVersion() == LGV_INFINITE ? (z & (cacheWidth - 1)) : z;
		int slot = xs + zs * cacheWidth;
        LevelChunk*& resident=authorityCoordinates()?authorityChunks[std::make_pair(x,z)]:chunks[slot];
        if (!resident || resident==emptyChunk || !resident->isAt(x,z)) {
            if (resident != NULL) {
                resident->unload();
                if (!level->isClientSide) {
                    save(resident);
                    saveEntities(resident);
                }
                retire(resident);
            }

            const bool remoteNative = isNativeClient();
            LevelChunk* newChunk = load(x, z);
			bool updateLights = false;
            if (newChunk == NULL) {
                if (source == NULL) {
                    newChunk = emptyChunk;
                } else {
                    newChunk = source->getChunk(x, z);
                }
            } else {
				//return emptyChunk;
				updateLights = !level->isClientSide;
            }
            if (newChunk != emptyChunk) ownedChunks.insert(newChunk);
            resident = newChunk;
            newChunk->lightLava();

			if (updateLights)
			{
				for (int cx = 0; cx < 16; cx++)
				{
					for (int cz = 0; cz < 16; cz++)
					{
						int height = level->getHeightmap(cx + x * 16, cz + z * 16);
#ifdef STANDALONE_SERVER
                        // Queue the same cells as two vertical ranges. Per-voxel
                        // block-light boxes cannot merge vertically under the
                        // legacy volume limit and overwhelm the lighting queue.
                        if (height >= 0) {
                            level->updateLight(LightLayer::Sky, cx + x * 16, 0, cz + z * 16, cx + x * 16, height, cz + z * 16);
                            level->updateLight(LightLayer::Block, cx + x * 16 - 1, 0, cz + z * 16 - 1, cx + x * 16 + 1, height, cz + z * 16 + 1);
                        }
#else
						for (int cy = height; cy >= 0; cy--)
						{
							level->updateLight(LightLayer::Sky, cx + x * 16, cy, cz + z * 16, cx + x * 16, cy, cz + z * 16);
							level->updateLight(LightLayer::Block, cx + x * 16 - 1, cy, cz + z * 16 - 1, cx + x * 16 + 1, cy, cz + z * 16 + 1);
						}
#endif
					}
				}
				//level->updateLight(LightLayer::Sky, x * 16, 0, z * 16, x * 16 + 15, 128, z * 16 + 15);
				//level->updateLight(LightLayer::Block, x * 16, 0, z * 16, x * 16 + 15, 128, z * 16 + 15);
			}

            if (resident != NULL) {
                resident->load();
            }

            if (!remoteNative && !resident->terrainPopulated && hasChunk(x + 1, z + 1) && hasChunk(x, z + 1) && hasChunk(x + 1, z)) postProcess(this, x, z);
            if (!remoteNative && hasChunk(x - 1, z) && !getChunk(x - 1, z)->terrainPopulated && hasChunk(x - 1, z + 1) && hasChunk(x, z + 1) && hasChunk(x - 1, z)) postProcess(this, x - 1, z);
            if (!remoteNative && hasChunk(x, z - 1) && !getChunk(x, z - 1)->terrainPopulated && hasChunk(x + 1, z - 1) && hasChunk(x, z - 1) && hasChunk(x + 1, z)) postProcess(this, x, z - 1);
            if (!remoteNative && hasChunk(x - 1, z - 1) && !getChunk(x - 1, z - 1)->terrainPopulated && hasChunk(x - 1, z - 1) && hasChunk(x, z - 1) && hasChunk(x - 1, z)) postProcess(this, x - 1, z - 1);
        }
        xLast = x;
        zLast = z;
        last = resident;

		//sw.stop();
		//sw.printEvery(500000, "ChunkCache::load: ");

        return resident;
    }

	Biome::MobList getMobsAt(const MobCategory& mobCategory, int x, int y, int z) {
		return source->getMobsAt(mobCategory, x, y, z);
	}

    void postProcess(ChunkSource* parent, int x, int z) {
		if (isNativeClient() || !fits(x, z)) return;
        LevelChunk* chunk = getChunk(x, z);
        if (!chunk->terrainPopulated) {
            chunk->terrainPopulated = true;
            if (source != NULL) {
                source->postProcess(parent, x, z);
				chunk->clearUpdateMap();
                // Population is persisted even when no player has edited it.
                // Clearing network deltas must not mark generated terrain saved.
                if (!level->isClientSide) chunk->unsaved = true;
            }
        }
    }

    //bool save(bool force, ProgressListener progressListener) {
    //    int saves = 0;
    //    int count = 0;
    //    if (progressListener != NULL) {
    //        for (int i = 0; i < chunks.length; i++) {
    //            if (chunks[i] != NULL && chunks[i].shouldSave(force)) {
    //                count++;
    //            }
    //        }
    //    }
    //    int cc = 0;
    //    for (int i = 0; i < chunks.length; i++) {
    //        if (chunks[i] != NULL) {
    //            if (force && !chunks[i].dontSave) saveEntities(chunks[i]);
    //            if (chunks[i].shouldSave(force)) {
    //                save(chunks[i]);
    //                chunks[i].unsaved = false;
    //                if (++saves == MAX_SAVES && !force) return false;
    //                if (progressListener != NULL) {
    //                    if (++cc % 10 == 0) {
    //                        progressListener.progressStagePercentage(cc * 100 / count);
    //                    }
    //                }
    //            }
    //        }
    //    }

    //    if (force) {
    //        if (storage == NULL) return true;
    //        storage.flush();
    //    }
    //    return true;
    //}

    bool tick() {
        if (isNativeClient()) return false;
        if (storage != NULL) storage->tick();
        return source->tick();
    }

    bool shouldSave() {
        return true;
    }

    std::string gatherStats() {
        return "ChunkCache: 1024";
    }

	void saveAll(bool onlyUnsaved) {
        if (isNativeClient()) return;
		if (storage != NULL) {
			std::vector<LevelChunk*> loadedChunks;
			for (int i = 0; i < cacheWidth * cacheWidth; ++i) {
				LevelChunk* chunk = chunks[i];
				if (chunk == NULL || chunk == emptyChunk)
					continue;
				if (!onlyUnsaved || chunk->shouldSave(false))
					loadedChunks.push_back(chunk);
			}
            if(authorityCoordinates()) for(const auto& entry:authorityChunks) {
                LevelChunk* chunk=entry.second;
                if(chunk&&chunk!=emptyChunk&&(!onlyUnsaved||chunk->shouldSave(false))) loadedChunks.push_back(chunk);
            }
			storage->saveAll(level, loadedChunks);
		}
	}
private:
    // Authoritative worlds may serve players whose coordinates alias the
    // client's modulo ring. Keep those chunks distinct, and trim only at the
    // established quiescent frame boundary. The per-player nine-by-nine halo
    // covers the existing tile/weather simulation radius; the extra 1024 LRU
    // slots absorb streaming and generation without unbounded resident growth.
    // Failed saves and outstanding readers retain retired chunks separately.
    typedef std::pair<int,int> Coordinate;
    std::map<Coordinate,LevelChunk*> authorityChunks;
    std::map<Coordinate,uint64_t> authorityAccess;
    uint64_t accessClock=0;
    bool authorityCoordinates() const { return !level->isClientSide&&level->getLevelData()->getGeneratorVersion()==LGV_INFINITE; }
    bool isResident(LevelChunk* chunk) const {
        if(authorityCoordinates()) {auto found=authorityChunks.find(Coordinate(chunk->x,chunk->z));return found!=authorityChunks.end()&&found->second==chunk;}
        return chunks[(chunk->x&(cacheWidth-1))+(chunk->z&(cacheWidth-1))*cacheWidth]==chunk;
    }
    bool playerPinned(const Coordinate& coordinate) const {
        for(auto* player:level->players) if(player&&!player->removed) {
            const int x=Mth::floor(player->x/16.f),z=Mth::floor(player->z/16.f);
            if(std::abs((int64_t)coordinate.first-x)<=4&&std::abs((int64_t)coordinate.second-z)<=4)return true;
        }
        return false;
    }
    void trimAuthoritativeResidents() {
        if(!authorityCoordinates()||authorityChunks.size()<=residentCapacity())return;
        std::vector<std::pair<uint64_t,Coordinate> > candidates;
        for(const auto& entry:authorityChunks) if(!playerPinned(entry.first)) candidates.push_back(std::make_pair(authorityAccess[entry.first],entry.first));
        std::sort(candidates.begin(),candidates.end());
        for(const auto& candidate:candidates) {
            if(authorityChunks.size()<=residentCapacity())break;
            auto found=authorityChunks.find(candidate.second);if(found==authorityChunks.end())continue;
            LevelChunk* chunk=found->second;
            if(chunk&&chunk!=emptyChunk) {
                chunk->unload();save(chunk);saveEntities(chunk);retire(chunk);
                if(last==chunk){last=NULL;xLast=zLast=-999999999;}
            }
            authorityChunks.erase(found);authorityAccess.erase(candidate.second);
        }
    }
    bool isNativeClient() const { return level->isClientSide && !level->isJavaServerLevel(); }

    LevelChunk* load(int x, int z) {
        if (storage == NULL) return emptyChunk;
        //try {
            LevelChunk* levelChunk = storage->load(level, x, z);
            if (levelChunk != NULL) {
                levelChunk->lastSaveTime = level->getTime();
            }
            return levelChunk;
        //} catch (Exception e) {
        //    e.printStackTrace();
        //    return emptyChunk;
        //}
    }

    void saveEntities(LevelChunk* levelChunk) {
        if (storage == NULL) return;
        //try {
            storage->saveEntities(level, levelChunk);
        //} catch (Error e) {
        //    e.printStackTrace();
        //}
    }

    void save(LevelChunk* levelChunk) {
        if (storage == NULL) return;
        //try {
            levelChunk->lastSaveTime = level->getTime();
            storage->save(level, levelChunk);
        //} catch (IOException e) {
        //    e.printStackTrace();
        //}
    }

public:
	int xLast;
    int zLast;
private:
	//unsigned char emptyChunkBlocks[LevelChunk::ChunkBlockCount];
    LevelChunk* emptyChunk;
    ChunkSource* source;
    ChunkStorage* storage;
	int cacheWidth;
	LevelChunk** chunks;
    Level* level;

    LevelChunk* last;
    std::set<LevelChunk*> ownedChunks;
    std::deque<LevelChunk*> retiredChunks;
    std::set<LevelChunk*> queuedRetirement;
    void retire(LevelChunk* chunk) {
        if(chunk&&chunk!=emptyChunk&&queuedRetirement.insert(chunk).second)retiredChunks.push_back(chunk);
    }

};

#endif /*NET_MINECRAFT_WORLD_LEVEL_CHUNK__ChunkCache_H__*/
