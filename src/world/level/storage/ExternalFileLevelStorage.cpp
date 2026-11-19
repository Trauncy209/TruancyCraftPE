#if !defined(DEMO_MODE) && !defined(APPLE_DEMO_PROMOTION)

#include "LevelData.h"
#include "../../entity/player/Player.h"
#include "../../entity/player/Inventory.h"
#include <cerrno>
#include <cctype>
#include <sys/stat.h>
#include <zlib.h>
#ifndef _WIN32
#include <fcntl.h>
#endif
#ifndef _WIN32
#include <unistd.h>
#endif
#include "RegionFile.h"
#include "ExternalFileLevelStorage.h"
#include "FolderMethods.h"
#include "../chunk/LevelChunk.h"
#include "../Level.h"
#include "../LevelConstants.h"
#include "../tile/TreeTile.h"
#include "../../entity/EntityFactory.h"
#include "../../entity/SavedEntityTransform.h"
#include "../../../nbt/NbtIo.h"
#include "../../../nbt/NativeNbtValidation.h"
#include "../../../util/RakDataIO.h"
#include "../../../raknet/GetTime.h"
#include "../tile/entity/TileEntity.h"

namespace {
const size_t MaxNativeNbtBytes = 64u * 1024u * 1024u;
bool durableNativeFile(const std::string& path, const unsigned char* bytes, size_t size) {
    FILE* fp = fopen(path.c_str(), "wb");
    if (!fp) return false;
    bool ok = fwrite(bytes, 1, size, fp) == size;
    if (fflush(fp)) ok = false;
#ifndef _WIN32
    if (ok && fsync(fileno(fp))) ok = false;
#endif
    if (fclose(fp)) ok = false;
    return ok;
}
void syncNativeDirectory(const std::string& path) {
#ifndef _WIN32
    int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd >= 0) { fsync(fd); close(fd); }
#endif
}
bool readNativeEntityBytes(const std::string& path, std::vector<unsigned char>& bytes) {
    bytes.clear();
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return false;
    unsigned char header[12] = {};
    int version = 0, size = 0;
    bool ok = fread(header, 1, sizeof(header), fp) == sizeof(header);
    memcpy(&version, header + 4, 4); memcpy(&size, header + 8, 4);
    ok = ok && memcmp(header, "ENT\0", 4) == 0 && version == 1 && size > 0 &&
        size_t(size) <= MaxNativeNbtBytes && size <= getRemainingFileSize(fp);
    if (ok) {
        bytes.assign(header, header + sizeof(header)); bytes.resize(12 + size_t(size));
        ok = fread(bytes.data() + 12, 1, size, fp) == size_t(size) &&
            NativeNbtValidation::compound(bytes.data() + 12, size_t(size));
    }
    fclose(fp);
    if (!ok) bytes.clear();
    return ok;
}
bool writeNativeEntityBytes(const std::string& directory, const std::vector<unsigned char>& bytes) {
    const std::string path = directory + "/entities.dat";
    if (!durableNativeFile(path + "_new", bytes.data(), bytes.size())) return false;
    std::vector<unsigned char> previous;
    if (readNativeEntityBytes(path, previous)) {
        if (!durableNativeFile(path + "_old_new", previous.data(), previous.size()) ||
            rename((path + "_old_new").c_str(), (path + "_old").c_str())) return false;
    } else if (exists(path.c_str())) {
        // Preserve unreadable input for recovery instead of overwriting it.
        std::string corrupt = path + ".corrupt-" + std::to_string(RakNet::GetTimeMS());
        while (exists(corrupt.c_str())) corrupt += "-1";
        if (rename(path.c_str(), corrupt.c_str())) return false;
        LOGE("Preserved unreadable entity snapshot: %s\n", corrupt.c_str());
    }
#ifdef _WIN32
    if (exists(path.c_str()) && remove(path.c_str())) return false;
#endif
    if (rename((path + "_new").c_str(), path.c_str())) return false;
    syncNativeDirectory(directory);
    return true;
}
}

static const int ChunkVersion_Light = 1;
static const int ChunkVersion_Entity = 2;

const char* const fnLevelDatOld = "level.dat_old";
const char* const fnLevelDatNew = "level.dat_new";
const char* const fnLevelDat    = "level.dat";
const char* const fnPlayerDat   = "player.dat";

//
// Helpers for converting old levels to newer
//
class LevelConverters
{
public:
	// Replacing old Cloth (id based) with new Cloth (data based)
	static bool v1_ClothIdToClothData(LevelChunk* c) {
		bool changed = false;
		unsigned char* blocks = c->getBlockData();
		unsigned char newTile = Tile::cloth->id;

		for (int i = 0; i < 16*16*128; ++i) {
			unsigned char oldTile = blocks[i];
			//Tile::cloth_00 to Tile::cloth_61
			if (oldTile >= 101 && oldTile <= 115) {
				int color = 0xf - (oldTile - 101);
				blocks[i] = newTile;
				c->data.set(i, color);
				changed = true;
			}
		}
		return changed;
	}

	// Replacing unavailable blocks with "Update!" blocks
	static bool ReplaceUnavailableBlocks(LevelChunk* c) {
		//int st = getTimeMs();
		
		bool changed = false;
		unsigned char* blocks = c->getBlockData();

		for (int i = 0; i < 16*16*128; ++i) {
			unsigned char oldTile = blocks[i];
			unsigned char newTile = Tile::transformToValidBlockId(oldTile);
			if (oldTile != newTile) {
				blocks[i] = newTile;
				changed = true;
			}
		}

		//int et = getTimeMs();
		//LOGI("time: %d\n", et - st);
		return changed;
	}

	//static bool ConvertPlayerDatToLevelDat() {
	//	return false;
	//}
};


ExternalFileLevelStorage::ExternalFileLevelStorage(const std::string& levelId, const std::string& fullPath)
:	levelId(levelId),
	levelPath(fullPath),
	loadedLevelData(NULL),
	regionFile(NULL),
	entitiesFile(NULL),
	tickCount(0),
	lastSavedEntitiesTick(-999999),
	level(NULL),
	loadedStorageVersion(SharedConstants::StorageVersion)
{
	createFolderIfNotExists(levelPath.c_str());

	std::string datFileName   = levelPath + "/" + fnLevelDat;
	std::string levelFileName = levelPath + "/" + fnPlayerDat;
	loadedLevelData = new LevelData();
	if (readLevelData(levelPath, *loadedLevelData))
	{
		loadedStorageVersion = loadedLevelData->getStorageVersion();
		readPlayerData(levelFileName, *loadedLevelData);
	} else {
		delete loadedLevelData;
		loadedLevelData = NULL;
	}
}

ExternalFileLevelStorage::~ExternalFileLevelStorage()
{
	delete regionFile;
	delete loadedLevelData;
}


// Username-only native sessions have no authenticated UUID. Use a case-insensitive,
// hex-encoded name as the stable key; never use a connection GUID or raw path.
static std::string nativePlayerKey(const std::string& name) {
    static const char hex[] = "0123456789abcdef";
    std::string key;
    if (name.empty() || name.size() > 64) return key;
    for (size_t i = 0; i < name.size(); ++i) {
        unsigned char c = (unsigned char)name[i];
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        key += hex[c >> 4]; key += hex[c & 15];
    }
    return key;
}

bool ExternalFileLevelStorage::saveNetworkPlayer(Player* player) {
    if (!player) return false;
    const std::string key = nativePlayerKey(player->name);
    if (key.empty()) return false;
    const std::string dir = levelPath + "/players";
#ifdef _WIN32
    _mkdir(dir.c_str());
#else
    if (mkdir(dir.c_str(), 0700) && errno != EEXIST) return false;
#endif
    const std::string path = dir + "/" + key + ".dat";
    CompoundTag tag;
    player->saveWithoutId(&tag);
    tag.putInt("NativePlayerVersion", 1);
    tag.putInt("NativeSelectedSlot", player->inventory->selected);
    for (int i=0;i<Inventory::MAX_SELECTION_SIZE;++i)
        tag.putInt("NativeHotbarSlot" + std::to_string(i), player->inventory->linkedSlots[i].inventorySlot);
    tag.putString("NativePlayerName", key);
    RakNet::BitStream data;
    RakDataOutput output(data);
    NbtIo::write(&tag, &output);
    tag.deleteChildren();
    FILE* file = fopen((path + ".new").c_str(), "wb");
    if (!file) return false;
    const unsigned int header[3] = {0x31504354, data.GetNumberOfBytesUsed(),
        (unsigned int)crc32(0, data.GetData(), data.GetNumberOfBytesUsed())};
    bool ok = fwrite(header, sizeof(header), 1, file) == 1;
    ok = (fwrite(data.GetData(), 1, data.GetNumberOfBytesUsed(), file) == data.GetNumberOfBytesUsed()) && ok;
    if (fflush(file)) ok = false;
#ifndef _WIN32
    if (ok && fsync(fileno(file))) ok = false;
#endif
    if (fclose(file)) ok = false;
    if (!ok) { remove((path + ".new").c_str()); return false; }
    // Keep a recoverable previous snapshot before atomically replacing the current one.
    FILE* previous = fopen(path.c_str(), "rb");
    if (previous) {
        FILE* backup = fopen((path + ".old.new").c_str(), "wb");
        bool copied = backup != NULL;
        char buffer[4096]; size_t size;
        while ((size = fread(buffer, 1, sizeof(buffer), previous)) != 0)
            if (backup && fwrite(buffer, 1, size, backup) != size) copied = false;
        if (ferror(previous)) copied = false;
        fclose(previous);
        if (backup && fclose(backup)) copied = false;
        if (!copied || rename((path + ".old.new").c_str(), (path + ".old").c_str())) return false;
    }
    if (rename((path + ".new").c_str(), path.c_str())) return false;
#ifndef _WIN32
    int directory = open(dir.c_str(), O_RDONLY | O_DIRECTORY);
    if (directory < 0) return false;
    bool durable = fsync(directory) == 0;
    close(directory);
    if (!durable) return false;
#endif
    return true;
}

int ExternalFileLevelStorage::loadNetworkPlayer(Player* player) {
    const std::string key = nativePlayerKey(player->name);
    if (key.empty()) return -1;
    const std::string path = levelPath + "/players/" + key + ".dat";
    FILE* file = fopen(path.c_str(), "rb");
    if (!file) return errno == ENOENT ? 0 : -1;
    fseek(file, 0, SEEK_END); long size = ftell(file); rewind(file);
    if (size <= 0 || size > 1024 * 1024) { fclose(file); return -1; }
    std::vector<unsigned char> bytes(size);
    bool ok = fread(&bytes[0], 1, size, file) == (size_t)size;
    fclose(file);
    if (!ok) return -1;
    if (bytes.size() < 12) return -1;
    unsigned int header[3]; memcpy(header, &bytes[0], sizeof(header));
    if (header[0] != 0x31504354 || header[1] != bytes.size() - 12
        || header[2] != (unsigned int)crc32(0, &bytes[12], bytes.size() - 12)) return -1;
    if (!NativeNbtValidation::compound(&bytes[12], bytes.size() - 12)) return -1;
    RakNet::BitStream data(&bytes[12], bytes.size() - 12, false);
    RakDataInput input(data);
    CompoundTag* tag = NbtIo::read(&input);
    if (!tag) return -1;
    ok = tag->getInt("NativePlayerVersion") == 1 && tag->getString("NativePlayerName") == key
        && tag->contains("Pos", Tag::TAG_List) && tag->contains("Inventory", Tag::TAG_List);
    if (ok) {
        ok = player->load(tag);
        // Legacy inventory loading compacts empty hotbar slots. Restore the exact
        // native layout afterwards, including deliberate gaps.
        for (int i=0;i<Inventory::MAX_SELECTION_SIZE;++i) {
            std::string field="NativeHotbarSlot"+std::to_string(i);
            if (tag->contains(field)) {
                int slot=tag->getInt(field);
                if (slot==-1 || (slot>=Inventory::MAX_SELECTION_SIZE && slot<player->inventory->getContainerSize()))
                    player->inventory->linkedSlots[i].inventorySlot=slot;
            }
        }
        player->inventory->selectSlot(tag->getInt("NativeSelectedSlot"));
    }
    tag->deleteChildren(); delete tag;
    return ok ? 1 : -1;
}

void ExternalFileLevelStorage::saveLevelData(LevelData& levelData, std::vector<Player*>* players) {
	if (players) for (size_t i = 0; i < players->size(); ++i)
        if (!saveNetworkPlayer((*players)[i])) LOGE("Native player save failed: %s\n", (*players)[i]->name.c_str());
	ExternalFileLevelStorage::saveLevelData(levelPath, levelData, players);
}

void ExternalFileLevelStorage::saveLevelData( const std::string& levelPath, LevelData& levelData, std::vector<Player*>* players )
{
	std::string directory = levelPath + "/";
    std::string tmpFile = directory + fnLevelDatNew;
    std::string datFile = directory + fnLevelDat;
    std::string oldFile = directory + fnLevelDatOld;

	levelData.setStorageVersion(SharedConstants::StorageVersion);
	if (!writeLevelData(tmpFile, levelData, players))
        return;

    // Remove old backup
    remove(oldFile.c_str());

    // If it exists, move the previous save to backup (and possibly delete it)
    if (exists(datFile.c_str())) {
        if (rename(datFile.c_str(), oldFile.c_str())) {
            LOGE("Error@saveLevelData: Couldn't move savefile to level.dat_old\n");
            return;
        }
        remove(datFile.c_str());
    }
    // Move the new save to level.dat
    if (rename(tmpFile.c_str(), datFile.c_str())) {
        LOGE("Error@saveLevelData: Couldn't move new file to level.dat\n");
        return;
    }

    // Remove the temporary save, if the rename didn't do it
    remove(tmpFile.c_str());
}

LevelData* ExternalFileLevelStorage::prepareLevel(Level* _level)
{
	level = _level;
	return loadedLevelData;
}

bool ExternalFileLevelStorage::readLevelData(const std::string& directory, LevelData& levelData)
{
    const char* candidates[] = { fnLevelDat, fnLevelDatOld };

    for (int attempt = 0; attempt < 2; ++attempt) {
        std::string datFilename = directory + "/" + candidates[attempt];
        FILE* file = fopen(datFilename.c_str(), "rb");
        if (!file)
            continue;

        int version = 0;
        int size = 0;
        unsigned char* data = NULL;
        bool loaded = false;

        do {
            if (fread(&version, sizeof(version), 1, file) != 1)
                break;
            if (fread(&size, sizeof(size), 1, file) != 1)
                break;

            int left = getRemainingFileSize(file);
            if (size > left || size <= 0 || size_t(size) > MaxNativeNbtBytes)
                break;

            data = new unsigned char[size];
            if (fread(data, 1, size, file) != size)
                break;

            if (version == 1) {
                RakNet::BitStream bitStream(data, size, false);
                levelData.v1_read(bitStream, version);
                loaded = true;
            } else if (version >= 2) {
                if (!NativeNbtValidation::compound(data, size_t(size))) break;
                RakNet::BitStream bitStream(data, size, false);
                RakDataInput stream(bitStream);
                CompoundTag* tag = NbtIo::read(&stream);
                if (tag) {
                    if (tag->contains("Player") &&
                        (!tag->contains("Player", Tag::TAG_Compound) ||
                         !tag->getCompound("Player")->contains("Inventory", Tag::TAG_List) ||
                         !SavedEntityTransform::valid(tag->getCompound("Player"), Level::MAX_LEVEL_SIZE))) {
                        tag->deleteChildren(); delete tag; break;
                    }
                    levelData.getTagData(tag);
                    tag->deleteChildren();
                    delete tag;
                    loaded = true;
                }
            }
        } while (false);

        fclose(file);
        delete [] data;

        if (loaded)
            return true;

        LOGE("Error@readLevelData: Failed to parse %s\n", datFilename.c_str());
    }

    return false;
}

bool ExternalFileLevelStorage::writeLevelData(const std::string& datFileName, LevelData& levelData, const std::vector<Player*>* players)
{
	LOGI("Writing down level seed as: %ld\n", levelData.getSeed());
	//return true;

	// Write level info
	FILE* file = fopen(datFileName.c_str(), "wb");
	if (!file)
        return false;

    //if (levelData.getStorageVersion() == 1) {
	RakNet::BitStream data;
	if (levelData.getStorageVersion() == 1)
		levelData.v1_write(data);
	else {
		RakDataOutput buf(data);
		//LOGI("---> Trying to write level with version %d\n", version);
		CompoundTag* tag = NULL;
		if (players && !players->empty())
			tag = levelData.createTag(*players);
		else
			tag = levelData.createTag();

		NbtIo::write(tag, &buf);
		tag->deleteChildren();
		delete tag;
		//LOGI("<--- Finished writing level data. Size: %d\n", fdout.bytesWritten);
	}

	int version = levelData.getStorageVersion(); // 1
    int size = data.GetNumberOfBytesUsed();
    bool ok = fwrite(&version, sizeof(version), 1, file) == 1;
    ok = (fwrite(&size, sizeof(size), 1, file) == 1) && ok;
    ok = (fwrite(data.GetData(), 1, size, file) == size_t(size)) && ok;
    if (fflush(file)) ok = false;
#ifndef _WIN32
    if (ok && fsync(fileno(file))) ok = false;
#endif
    if (fclose(file)) ok = false;
    return ok;
}

bool ExternalFileLevelStorage::readPlayerData(const std::string& filename, LevelData& dest)
{
	FILE* fp = fopen(filename.c_str(), "rb");
	if (!fp)
		return false;

	do {
		int version;
		if (fread(&version, 4, 1, fp) != 1)
			break;

		int size;
		if (fread(&size, 4, 1, fp) != 1)
			break;

		if (version == 1) {
            if (size != sizeof(dest.playerData)) break;
			if (fread(&dest.playerData, 1, sizeof(dest.playerData), fp) != size)
				break;

			// Fix coordinates
			Vec3& pos = dest.playerData.pos;
			if (dest.getGeneratorVersion() == LGV_INFINITE) {
				if (pos.x > (Level::MAX_LEVEL_SIZE - 0.5f)) pos.x = Level::MAX_LEVEL_SIZE - 0.5f;
				if (pos.x < (-Level::MAX_LEVEL_SIZE + 0.5f)) pos.x = -Level::MAX_LEVEL_SIZE + 0.5f;
				if (pos.z > (Level::MAX_LEVEL_SIZE - 0.5f)) pos.z = Level::MAX_LEVEL_SIZE - 0.5f;
				if (pos.z < (-Level::MAX_LEVEL_SIZE + 0.5f)) pos.z = -Level::MAX_LEVEL_SIZE + 0.5f;
			} else {
				if (pos.x > (LEVEL_WIDTH - 0.5f)) pos.x = LEVEL_WIDTH - 0.5f;
				if (pos.x < 0.5f) pos.x = 0.5f;
				if (pos.z > (LEVEL_DEPTH - 0.5f)) pos.z = LEVEL_DEPTH - 0.5f;
				if (pos.z < 0.5f) pos.z = 0.5f;
			}
			if (pos.y < 0) pos.y = 64;

			dest.playerDataVersion = version;
		}
	} while (false);

	fclose(fp);
	return true;
}

void ExternalFileLevelStorage::tick()
{
	tickCount++;
	if ((tickCount % 50) == 0 && level)
	{
		auto enqueueUnsavedChunk = [this](LevelChunk* chunk) {
			if (!chunk || !chunk->unsaved)
				return;

			// Compare coordinates, not the old lossy 16-bit packed key.
			UnsavedChunkList::iterator prev = unsavedChunkList.begin();
			for ( ; prev != unsavedChunkList.end(); ++prev)
			{
				if (prev->chunk && prev->chunk->x == chunk->x && prev->chunk->z == chunk->z)
				{
					// the chunk has been modified again, so update its time
					(*prev).addedToList = RakNet::GetTimeMS();
                    prev->chunk = chunk;
					break;
				}
			}
			if (prev == unsavedChunkList.end())
			{
				UnsavedLevelChunk unsaved;
				unsaved.pos = 0; // Kept for layout compatibility; no longer used as identity.
				unsaved.addedToList = RakNet::GetTimeMS();
				unsaved.chunk = chunk;
				unsavedChunkList.push_back(unsaved);
			}
			chunk->unsaved = false; // not actually saved, but in our working list at least
		};

		const bool infiniteWorld = level->getLevelData()->getGeneratorVersion() == LGV_INFINITE;
		if (infiniteWorld)
		{
			std::vector<LevelChunk*> visitedChunks;
			for (size_t i = 0; i < level->players.size(); ++i)
			{
				Player* player = level->players[i];
				if (!player)
					continue;

				const int px = Mth::floor(player->x / (float)CHUNK_WIDTH);
				const int pz = Mth::floor(player->z / (float)CHUNK_DEPTH);
				for (int dz = -8; dz <= 8; ++dz)
				{
					for (int dx = -8; dx <= 8; ++dx)
					{
						const int cx = px + dx;
						const int cz = pz + dz;
						if (!level->hasChunk(cx, cz))
							continue;

						LevelChunk* chunk = level->getChunk(cx, cz);
						if (!chunk)
							continue;

						bool alreadyVisited = false;
						for (size_t vi = 0; vi < visitedChunks.size(); ++vi)
						{
							if (visitedChunks[vi] == chunk)
							{
								alreadyVisited = true;
								break;
							}
						}
						if (alreadyVisited)
							continue;

						visitedChunks.push_back(chunk);
						enqueueUnsavedChunk(chunk);
					}
				}
			}
		}
		else
		{
			// look for chunks that needs to be saved
			for (int z = 0; z < CHUNK_CACHE_WIDTH; z++)
			{
				for (int x = 0; x < CHUNK_CACHE_WIDTH; x++)
				{
					LevelChunk* chunk = level->getChunk(x, z);
					enqueueUnsavedChunk(chunk);
				}
			}
		}

		const int saveBudget = infiniteWorld ? 1 : 2;
        savePendingUnsavedChunks(saveBudget);
	}
	if (tickCount - lastSavedEntitiesTick > (60 * SharedConstants::TicksPerSecond)) {
		saveEntities(level, NULL);
	}
}

void ExternalFileLevelStorage::save(Level* level, LevelChunk* levelChunk)
{
    if (!levelChunk) return;
    levelChunk->unsaved = true;
    auto retainForRetry = [&]() {
        for (auto& pending : unsavedChunkList) {
            if (pending.chunk && pending.chunk->x == levelChunk->x && pending.chunk->z == levelChunk->z) {
                // A cache/direct write can carry newer edits than the queued
                // instance. Keep the latest complete in-memory copy.
                if(pending.chunk!=levelChunk)pending.chunk->unsaved=false;
                pending.chunk = levelChunk;
                return;
            }
        }
        UnsavedLevelChunk pending;
        pending.pos = static_cast<int>((static_cast<unsigned int>(levelChunk->x) << 16) ^
            (static_cast<unsigned int>(levelChunk->z) & 0xffffu));
        pending.addedToList = RakNet::GetTimeMS();
        pending.chunk = levelChunk;
        unsavedChunkList.push_back(pending);
    };
	if (!regionFile)
	{
		regionFile = new RegionFile(levelPath);
		if (!regionFile->open())
		{
			delete regionFile;
			regionFile = NULL;
            retainForRetry();
			return;
		}
	}

	// Write chunk
	RakNet::BitStream chunkData;
	chunkData.Write((const char*)levelChunk->getBlockData(), CHUNK_BLOCK_COUNT);
	chunkData.Write((const char*)levelChunk->data.data, CHUNK_BLOCK_COUNT / 2);

	chunkData.Write((const char*)levelChunk->skyLight.data, CHUNK_BLOCK_COUNT / 2);
	chunkData.Write((const char*)levelChunk->blockLight.data, CHUNK_BLOCK_COUNT / 2);

	chunkData.Write((const char*)levelChunk->updateMap, CHUNK_COLUMNS);

    if (!regionFile->writeChunk(levelChunk->x, levelChunk->z, chunkData)) {
        retainForRetry();
        LOGE("Failed to save chunk %d,%d; keeping it dirty for retry\n", levelChunk->x, levelChunk->z);
        return;
    }
    levelChunk->unsaved = false;
    // A direct/cache save may supersede an older queued copy at these same
    // coordinates. Drop it only after the new bytes are durably committed.
    for (auto pending = unsavedChunkList.begin(); pending != unsavedChunkList.end();) {
        LevelChunk* queued = pending->chunk;
        if (queued && queued->x == levelChunk->x && queued->z == levelChunk->z) {
            if(queued!=levelChunk)queued->unsaved=false;
            pending = unsavedChunkList.erase(pending);
        }
        else ++pending;
    }

	// Write entities

	//LOGI("Saved chunk (%d, %d)\n", levelChunk->x, levelChunk->z);

}

LevelChunk* ExternalFileLevelStorage::load(Level* level, int x, int z)
{
    // A failed eviction write leaves the newest terrain in the pending queue.
    // Re-enter from that snapshot, not older disk bytes. Copy terrain only:
    // LevelChunk::load reattaches the live entity/storage registries separately.
    for (auto& pending:unsavedChunkList) {
        LevelChunk* previous=pending.chunk;
        if(!previous||previous->x!=x||previous->z!=z)continue;
        unsigned char* blocks=new unsigned char[CHUNK_BLOCK_COUNT];
        memcpy(blocks,previous->getBlockData(),CHUNK_BLOCK_COUNT);
        LevelChunk* restored=new LevelChunk(level,blocks,x,z);
        memcpy(restored->data.data,previous->data.data,CHUNK_BLOCK_COUNT/2);
        memcpy(restored->skyLight.data,previous->skyLight.data,CHUNK_BLOCK_COUNT/2);
        memcpy(restored->blockLight.data,previous->blockLight.data,CHUNK_BLOCK_COUNT/2);
        memcpy(restored->updateMap,previous->updateMap,CHUNK_COLUMNS);
        restored->recalcHeightmapOnly();
        restored->terrainPopulated=previous->terrainPopulated;
        restored->createdFromSave=previous->createdFromSave;
        restored->unsaved=true;
        previous->unsaved=false; // The fresh complete copy now owns the pending edits.
        pending.chunk=restored;
        return restored;
    }

	if (!regionFile)
	{
		regionFile = new RegionFile(levelPath);
		if (!regionFile->open())
		{
			delete regionFile;
			regionFile = NULL;
			return NULL;
		}
	}

	RakNet::BitStream* chunkData = NULL;
	if (!regionFile->readChunk(x, z, &chunkData))
	{
		//LOGI("Failed to read data for %d, %d\n", x, z);
		return NULL;
	}

    chunkData->ResetReadPointer();
    const unsigned int required = CHUNK_BLOCK_COUNT + CHUNK_BLOCK_COUNT / 2 + CHUNK_COLUMNS +
        (loadedStorageVersion >= ChunkVersion_Light ? CHUNK_BLOCK_COUNT : 0);
    if (chunkData->GetNumberOfUnreadBits() < required * 8u) {
        // Retain the original region before any regenerated chunk can replace
        // this truncated save. Never construct a chunk from unread memory.
        regionFile->markChunkCorrupt(x, z);
        delete[] chunkData->GetData(); delete chunkData;
        LOGE("Rejected truncated saved chunk at %d,%d\n", x, z);
        return NULL;
    }
    unsigned char* blockIds = new unsigned char[CHUNK_BLOCK_COUNT];
    bool complete = chunkData->Read((char*)blockIds, CHUNK_BLOCK_COUNT);
    LevelChunk* levelChunk = new LevelChunk(level, blockIds, x, z);
    complete = chunkData->Read((char*)levelChunk->data.data, CHUNK_BLOCK_COUNT / 2) && complete;
    if (loadedStorageVersion >= ChunkVersion_Light) {
        complete = chunkData->Read((char*)levelChunk->skyLight.data, CHUNK_BLOCK_COUNT / 2) && complete;
        complete = chunkData->Read((char*)levelChunk->blockLight.data, CHUNK_BLOCK_COUNT / 2) && complete;
    }
    complete = chunkData->Read((char*)levelChunk->updateMap, CHUNK_COLUMNS) && complete;
    if (!complete) {
        regionFile->markChunkCorrupt(x, z);
        delete[] chunkData->GetData(); delete chunkData;
        delete levelChunk; delete[] blockIds;
        return NULL;
    }
	// This will be difficult to maintain.. Storage version could be per chunk
	// too (but probably better to just read all -> write all, so that all
	// chunks got same version anyway)
	//if (loadedStorageVersion >= ChunkVersion_Entity) {
	//	int dictSize;
	//	chunkData->Read(dictSize);

	//	RakDataInput dis(*chunkData);
	//	Tag* tmp = Tag::readNamedTag(&dis);
	//	if (tmp && tmp->getId() == Tag::TAG_Compound) {
	//		CompoundTag* tag = (CompoundTag*) tmp;

	//		delete tmp;
	//	}
	//}

	delete [] chunkData->GetData();
	delete chunkData;

	//bool dbg = (x == 7 && z == 9);

	//int t = 0;
	//for (int i = 0; i < CHUNK_COLUMNS; ++i) {
	//	char bits = levelChunk->updateMap[i];
	//	t += (bits != 0);
	//	int xx = x * 16 + i%16;
	//	int zz = z * 16 + i/16;
	//	if (dbg && xx == 125 && zz == 152) {
	//		LOGI("xz: %d, %d: %d\n", xx, zz, bits);
	//		for (int j = 0; j < 8; ++j) {
	//			if (bits & (1 << j)) {
	//				LOGI("%d - %d\n", j << 4, ((j+1) << 4) - 1);
	//			}
	//		}
	//	}
	//}

	//
	// Convert LevelChunks here if necessary
	//
	//LOGI("level version: %d: upd: %d - (%d, %d)\n", loadedStorageVersion, t, x, z);
	bool changed = false;

	// Loaded level has old Cloth types (one Tile* per color)
	if (loadedStorageVersion == 1)
		changed |= LevelConverters::v1_ClothIdToClothData(levelChunk);

	// Loaded level is newer than our level - replace all unavailable block types
	//if (loadedStorageVersion > SharedConstants::StorageVersion)
		changed |= LevelConverters::ReplaceUnavailableBlocks(levelChunk);

	levelChunk->recalcHeightmap();
	levelChunk->unsaved = changed;
	levelChunk->terrainPopulated = true;
	levelChunk->createdFromSave = true;

	return levelChunk;
}

void ExternalFileLevelStorage::saveEntities( Level* level, LevelChunk* levelChunk )
{
	lastSavedEntitiesTick = tickCount;
	int count = 0;
	float st = getTimeS();

	// Version 1: Save ALL Entities for all chunks in one structure
	EntityList& entities = level->entities;

	ListTag* entityTags = new ListTag();
	for (unsigned int i = 0; i < entities.size(); ++i) {
		Entity* e = entities[i];

		CompoundTag* tag = new CompoundTag();
		if (e->save(tag)) {
			count++;
			entityTags->add(tag);
		} else
			delete tag;
	}

	// Version 1: Save ALL TileEntities for all chunks in one structure
	TileEntityList& tileEntities = level->tileEntities;
	//TileEntityList keep, dontKeep;
	//partitionTileEntities

	ListTag* tileEntityTags = new ListTag();
	for (unsigned int i = 0; i < tileEntities.size(); ++i) {
		TileEntity* e = tileEntities[i];
		if (!e->shouldSave()) continue;

		CompoundTag* tag = new CompoundTag();
		if (e->save(tag)) {
			count++;
			tileEntityTags->add(tag);
		} else
			delete tag;
	}

	CompoundTag base;
	base.put("Entities", entityTags);
	base.put("TileEntities", tileEntityTags);

	RakNet::BitStream stream;
	RakDataOutput dos(stream);
	NbtIo::write(&base, &dos);
	int numBytes = stream.GetNumberOfBytesUsed();

    std::vector<unsigned char> snapshot(12 + numBytes);
    const int version = 1;
    memcpy(snapshot.data(), "ENT\0", 4);
    memcpy(snapshot.data() + 4, &version, 4);
    memcpy(snapshot.data() + 8, &numBytes, 4);
    memcpy(snapshot.data() + 12, stream.GetData(), numBytes);
    if (!writeNativeEntityBytes(levelPath, snapshot))
        LOGE("Failed to commit entity snapshot; previous save preserved: %s\n", levelPath.c_str());

	base.deleteChildren();

	float tt = getTimeS() - st;
	LOGI("Time to save %d entities: %f s. Size: %d bytes\n", count, tt, numBytes);
}

void ExternalFileLevelStorage::loadEntities(Level* level, LevelChunk* chunk) {
    lastSavedEntitiesTick = tickCount;
    CompoundTag* tag = NULL;
    const char* names[] = { "entities.dat", "entities.dat_old", "entities.dat_new" };
    for (const char* name : names) {
        std::vector<unsigned char> bytes;
        const std::string path = levelPath + "/" + name;
        if (!readNativeEntityBytes(path, bytes)) continue;
        RakNet::BitStream stream(bytes.data() + 12, bytes.size() - 12, false);
        RakDataInput input(stream);
        tag = NbtIo::read(&input);
        if (tag) {
            if (std::string(name) != "entities.dat") LOGE("Recovered entity snapshot from %s\n", path.c_str());
            break;
        }
    }
    if (!tag) return;
    if (tag->contains("Entities", Tag::TAG_List)) {
        ListTag* entityTags = tag->getList("Entities");
        for (int i = 0; i < entityTags->size(); ++i) {
            Tag* entry = entityTags->get(i);
            if (!entry || entry->getId() != Tag::TAG_Compound) continue;
            if (Entity* entity = EntityFactory::loadEntity(static_cast<CompoundTag*>(entry), level))
                level->addEntity(entity);
        }
    }
    if (tag->contains("TileEntities", Tag::TAG_List)) {
        ListTag* tileEntityTags = tag->getList("TileEntities");
        for (int i = 0; i < tileEntityTags->size(); ++i) {
            Tag* entry = tileEntityTags->get(i);
            if (!entry || entry->getId() != Tag::TAG_Compound) continue;
            TileEntity* entity = TileEntity::loadStatic(static_cast<CompoundTag*>(entry));
            if (!entity) continue;
            const bool infinite = level->getLevelData()->getGeneratorVersion() == LGV_INFINITE;
            const int low = infinite ? -Level::MAX_LEVEL_SIZE : 0;
            const int highX = infinite ? Level::MAX_LEVEL_SIZE : LEVEL_WIDTH;
            const int highZ = infinite ? Level::MAX_LEVEL_SIZE : LEVEL_DEPTH;
            if (entity->x < low || entity->x >= highX || entity->z < low || entity->z >= highZ ||
                entity->y < 0 || entity->y >= Level::DEPTH) {
                LOGE("Ignoring out-of-world saved tile entity at %d,%d,%d\n", entity->x, entity->y, entity->z);
                delete entity; continue;
            }
            LevelChunk* target = level->getChunkAt(entity->x, entity->z);
            if (target && !target->hasTileEntityAt(entity)) target->addTileEntity(entity);
            else delete entity;
        }
    }
    tag->deleteChildren();
    delete tag;
}

void ExternalFileLevelStorage::saveGame(Level* level) {
	saveEntities(level, NULL);
}

bool ExternalFileLevelStorage::hasPendingChunk(const LevelChunk* chunk) const {
    for(const auto& pending:unsavedChunkList)if(pending.chunk==chunk)return true;
    return false;
}

int ExternalFileLevelStorage::savePendingUnsavedChunks(int maxCount) {
    if (maxCount < 0) maxCount = static_cast<int>(unsavedChunkList.size());
    int saved = 0;
    while (saved < maxCount && !unsavedChunkList.empty()) {
        auto oldest = unsavedChunkList.begin();
        for (auto candidate = unsavedChunkList.begin(); candidate != unsavedChunkList.end(); ++candidate)
            if (candidate->addedToList < oldest->addedToList) oldest = candidate;
        LevelChunk* chunk = oldest->chunk;
        if (!chunk) break;
        // save() removes the queue entry only after a confirmed commit. Stop
        // after one failure rather than spinning or discarding the sole copy.
        save(level, chunk);
        if (chunk->unsaved) break;
        ++saved;
    }
    return saved;
}

void ExternalFileLevelStorage::saveAll( Level* level, std::vector<LevelChunk*>& levelChunks ) {
    ChunkStorage::saveAll(level, levelChunks);
	int numChunks = savePendingUnsavedChunks(-1);
    LOGI("Saving %d additional chunks.\n", numChunks);
}

#endif /*DEMO_MODE*/
