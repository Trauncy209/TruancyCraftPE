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
#include "../../../nbt/NbtIo.h"
#include "../../../util/RakDataIO.h"
#include "../../../raknet/GetTime.h"
#include "../tile/entity/TileEntity.h"

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
            if (size > left || size <= 0)
                break;

            data = new unsigned char[size];
            if (fread(data, 1, size, file) != size)
                break;

            if (version == 1) {
                RakNet::BitStream bitStream(data, size, false);
                levelData.v1_read(bitStream, version);
                loaded = true;
            } else if (version >= 2) {
                RakNet::BitStream bitStream(data, size, false);
                RakDataInput stream(bitStream);
                CompoundTag* tag = NbtIo::read(&stream);
                if (tag) {
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
	fwrite(&version, sizeof(version), 1, file);
	int size = data.GetNumberOfBytesUsed();
	fwrite(&size, sizeof(size), 1, file);

	fwrite(data.GetData(), 1, size, file);
	fclose(file);

	return true;
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

			const int pos = (chunk->x << 16) ^ (chunk->z & 0xffff);
			UnsavedChunkList::iterator prev = unsavedChunkList.begin();
			for ( ; prev != unsavedChunkList.end(); ++prev)
			{
				if ((*prev).pos == pos)
				{
					// the chunk has been modified again, so update its time
					(*prev).addedToList = RakNet::GetTimeMS();
					break;
				}
			}
			if (prev == unsavedChunkList.end())
			{
				UnsavedLevelChunk unsaved;
				unsaved.pos = pos;
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
	if (!regionFile)
	{
		regionFile = new RegionFile(levelPath);
		if (!regionFile->open())
		{
			delete regionFile;
			regionFile = NULL;
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

	regionFile->writeChunk(levelChunk->x, levelChunk->z, chunkData);

	// Write entities

	//LOGI("Saved chunk (%d, %d)\n", levelChunk->x, levelChunk->z);

}

LevelChunk* ExternalFileLevelStorage::load(Level* level, int x, int z)
{
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

	unsigned char* blockIds = new unsigned char[CHUNK_BLOCK_COUNT];
	chunkData->Read((char*)blockIds, CHUNK_BLOCK_COUNT);

	LevelChunk* levelChunk = new LevelChunk(level, blockIds, x, z);
	chunkData->Read((char*)levelChunk->data.data, CHUNK_BLOCK_COUNT / 2);
	if (loadedStorageVersion >= ChunkVersion_Light) {
		chunkData->Read((char*)levelChunk->skyLight.data, CHUNK_BLOCK_COUNT / 2);
		chunkData->Read((char*)levelChunk->blockLight.data, CHUNK_BLOCK_COUNT / 2);
	}
	chunkData->Read((char*)levelChunk->updateMap, CHUNK_COLUMNS);
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

	FILE* fp = fopen((levelPath + "/entities.dat").c_str(), "wb");
	if (fp) {
		int version = 1;
		fwrite("ENT\0", 1, 4, fp);
		fwrite(&version, sizeof(int), 1, fp);
		fwrite(&numBytes, sizeof(int), 1, fp);
		fwrite(stream.GetData(), 1, numBytes, fp);
		fclose(fp);
	}

	base.deleteChildren();

	float tt = getTimeS() - st;
	LOGI("Time to save %d entities: %f s. Size: %d bytes\n", count, tt, numBytes);
}

void ExternalFileLevelStorage::loadEntities(Level* level, LevelChunk* chunk) {
	lastSavedEntitiesTick = tickCount;
	FILE* fp = fopen((levelPath + "/entities.dat").c_str(), "rb");
	if (fp) {
		char header[5];
		int version, numBytes;
		fread(header, 1, 4, fp);
		fread(&version, sizeof(int), 1, fp);
		fread(&numBytes, sizeof(int), 1, fp);

		int left = getRemainingFileSize(fp);
		if (numBytes <= left && numBytes > 0) {
			unsigned char* buf = new unsigned char[numBytes];

			fread(buf, 1, numBytes, fp);

			RakNet::BitStream stream(buf, numBytes, false);
			RakDataInput dis(stream);

			CompoundTag* tag = NbtIo::read(&dis);
			//
			// Read Entity:es
			//
			if (tag->contains("Entities", Tag::TAG_List)) {
				ListTag* entityTags = tag->getList("Entities");
				for (int i = 0; i < entityTags->size(); ++i) {
					Tag* _et = entityTags->get(i);
					if (!_et || _et->getId() != Tag::TAG_Compound) {
						LOGE("Entity tag is either NULL or not a compoundTag: %p : %d!\n", _et, _et?_et->getId() : -1);
						continue;
					}
					CompoundTag* et = (CompoundTag*)_et;
					if (Entity* e = EntityFactory::loadEntity(et, level)) {
						level->addEntity(e);
					}
				}
			}
			//
			// Read TileEntity:s
			//
			if (tag->contains("TileEntities", Tag::TAG_List)) {
				ListTag* tileEntityTags = tag->getList("TileEntities");
				for (int i = 0; i < tileEntityTags->size(); ++i) {
					Tag* _et = tileEntityTags->get(i);
					if (!_et || _et->getId() != Tag::TAG_Compound) {
						LOGE("TileEntity tag is either NULL or not a compoundTag: %p : %d!\n", _et, _et?_et->getId() : -1);
						continue;
					}
					CompoundTag* et = (CompoundTag*)_et;
					if (TileEntity* e = TileEntity::loadStatic(et)) {
						
						LevelChunk* chunk = level->getChunkAt(e->x, e->z);
						if (chunk && !chunk->hasTileEntityAt(e)) {
							LOGI("Adding TileEntity %d to %d, %d, %d\n", e->type, e->x, e->y, e->z);
							chunk->addTileEntity(e);
						} else {
							if (!chunk)
								LOGE("Couldn't find chunk at %d, %d to add %d\n", e->x, e->z, e->type);
							else
								LOGE("Already have TileEntity at %d, %d to add %d\n", e->x, e->z, e->type);
							delete e;
						}
					}
				}
			}

			tag->deleteChildren();
			delete tag;

			delete[] buf;
		}
		LOGI("header: %s, version: %d, bytes: %d (remaining: %d)\n", header, version, numBytes, left);

		//fread(stream.GetData(), 1, numBytes, fp);
		fclose(fp);
	}
}

void ExternalFileLevelStorage::saveGame(Level* level) {
	saveEntities(level, NULL);
}

int ExternalFileLevelStorage::savePendingUnsavedChunks( int maxCount ) {
    if (maxCount < 0)
        maxCount = unsavedChunkList.size();

    int count = 0;
    while (++count <= maxCount && !unsavedChunkList.empty()) {

        UnsavedChunkList::iterator it = unsavedChunkList.begin();
        UnsavedChunkList::iterator remove = unsavedChunkList.begin();
        UnsavedLevelChunk* oldest = &(*it);

        for ( ; it != unsavedChunkList.end(); ++it) {
            if ((*it).addedToList < oldest->addedToList) {
                oldest = &(*it);
                remove = it;
            }
        }
        LevelChunk* chunk = oldest->chunk;
        unsavedChunkList.erase(remove);

        save(level, chunk);
    }
    return count;
}

void ExternalFileLevelStorage::saveAll( Level* level, std::vector<LevelChunk*>& levelChunks ) {
    ChunkStorage::saveAll(level, levelChunks);
	int numChunks = savePendingUnsavedChunks(-1);
    LOGI("Saving %d additional chunks.\n", numChunks);
}

#endif /*DEMO_MODE*/
