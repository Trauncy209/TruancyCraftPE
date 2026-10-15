#ifndef NET_MINECRAFT_WORLD_LEVEL_CHUNK__ChunkCache_H__
#define NET_MINECRAFT_WORLD_LEVEL_CHUNK__ChunkCache_H__

//package net.minecraft.world.level.chunk;

#include "ChunkSource.h"
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
		delete source;
		delete emptyChunk;

		for (int i = 0; i < cacheWidth * cacheWidth; i++)
		{
			if (chunks[i])
			{
				chunks[i]->deleteBlockData();
				delete chunks[i];
			}
		}
		delete[] chunks;
	}

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
        if (chunks[slot]) chunks[slot]->unload();
        LevelChunk* chunk = new LevelChunk(level,
            new unsigned char[LevelChunk::ChunkBlockCount](), x, z);
        chunk->terrainPopulated = true;
        memset(chunk->skyLight.data, level->dimension->hasCeiling ? 0 : 0xff,
               chunk->skyLight.length);
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

		if (x == xLast && z == zLast && last != NULL) {
            return last;
        }
		int xs = level->getLevelData()->getGeneratorVersion() == LGV_INFINITE ? (x & (cacheWidth - 1)) : x;
		int zs = level->getLevelData()->getGeneratorVersion() == LGV_INFINITE ? (z & (cacheWidth - 1)) : z;
		int slot = xs + zs * cacheWidth;
        if (!hasChunk(x, z)) {
            if (chunks[slot] != NULL) {
                chunks[slot]->unload();
                if (!level->isClientSide) {
                    save(chunks[slot]);
                    saveEntities(chunks[slot]);
                }
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
            chunks[slot] = newChunk;
            newChunk->lightLava();

			if (updateLights)
			{
				for (int cx = 0; cx < 16; cx++)
				{
					for (int cz = 0; cz < 16; cz++)
					{
						int height = level->getHeightmap(cx + x * 16, cz + z * 16);
						for (int cy = height; cy >= 0; cy--)
						{
							level->updateLight(LightLayer::Sky, cx + x * 16, cy, cz + z * 16, cx + x * 16, cy, cz + z * 16);
							level->updateLight(LightLayer::Block, cx + x * 16 - 1, cy, cz + z * 16 - 1, cx + x * 16 + 1, cy, cz + z * 16 + 1);
						}
					}
				}
				//level->updateLight(LightLayer::Sky, x * 16, 0, z * 16, x * 16 + 15, 128, z * 16 + 15);
				//level->updateLight(LightLayer::Block, x * 16, 0, z * 16, x * 16 + 15, 128, z * 16 + 15);
			}

            if (chunks[slot] != NULL) {
                chunks[slot]->load();
            }

            if (!remoteNative && !chunks[slot]->terrainPopulated && hasChunk(x + 1, z + 1) && hasChunk(x, z + 1) && hasChunk(x + 1, z)) postProcess(this, x, z);
            if (!remoteNative && hasChunk(x - 1, z) && !getChunk(x - 1, z)->terrainPopulated && hasChunk(x - 1, z + 1) && hasChunk(x, z + 1) && hasChunk(x - 1, z)) postProcess(this, x - 1, z);
            if (!remoteNative && hasChunk(x, z - 1) && !getChunk(x, z - 1)->terrainPopulated && hasChunk(x + 1, z - 1) && hasChunk(x, z - 1) && hasChunk(x + 1, z)) postProcess(this, x, z - 1);
            if (!remoteNative && hasChunk(x - 1, z - 1) && !getChunk(x - 1, z - 1)->terrainPopulated && hasChunk(x - 1, z - 1) && hasChunk(x, z - 1) && hasChunk(x - 1, z)) postProcess(this, x - 1, z - 1);
        }
        xLast = x;
        zLast = z;
        last = chunks[slot];

		//sw.stop();
		//sw.printEvery(500000, "ChunkCache::load: ");

        return chunks[slot];
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
			storage->saveAll(level, loadedChunks);
		}
	}
private:
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

};

#endif /*NET_MINECRAFT_WORLD_LEVEL_CHUNK__ChunkCache_H__*/
