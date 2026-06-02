#ifndef NET_MINECRAFT_WORLD_LEVEL_STORAGE__LevelData_H__
#define NET_MINECRAFT_WORLD_LEVEL_STORAGE__LevelData_H__

//package net.minecraft.world.level.storage;

#include <string>
#include "PlayerData.h"
#include "../LevelSettings.h"
#include "../dimension/Dimension.h"

// sorry for RakNet dependency, but I really like using BitStream
#include "../../../raknet/BitStream.h"
#include "../../../platform/time.h"
#include "../../../nbt/CompoundTag.h"

class LevelData
{
public:
	LevelData();
    LevelData(const LevelSettings& settings, const std::string& levelName, int generatorVersion = -1);
	LevelData(CompoundTag* tag);
	LevelData(const LevelData& rhs);
	LevelData& operator=(const LevelData& rhs);
	~LevelData();

	void		v1_write(RakNet::BitStream& bitStream);
	void		v1_read(RakNet::BitStream& bitStream, int storageVersion);

	// Caller's responsibility to destroy this Tag
    CompoundTag* createTag();
    CompoundTag* createTag(const std::vector<Player*>& players);

	void		getTagData(const CompoundTag* tag);
    void		setTagData(CompoundTag* tag, CompoundTag* playerTag);

    long		getSeed() const;
    int			getXSpawn() const;
    int			getYSpawn() const;
    int			getZSpawn() const;
    long		getTime() const;
    long		getSizeOnDisk() const;

	void		 setPlayerTag(CompoundTag* tag);
	CompoundTag* getLoadedPlayerTag();
	void		 setLoadedPlayerTo(Player* p);

    int			getDimension();

    void		setSeed(long seed);
    void		setXSpawn(int xSpawn);
    void		setYSpawn(int ySpawn);
    void		setZSpawn(int zSpawn);
	void		setSpawn(int xSpawn, int ySpawn, int zSpawn);

    void		setTime(long time);
    void		setSizeOnDisk(long sizeOnDisk);
    void		setLoadedPlayerTag(CompoundTag* playerTag);
    void		setDimension(int dimension);

    std::string getLevelName();
    void		setLevelName(const std::string& levelName);

    int			getGeneratorVersion() const;
    void		setGeneratorVersion(int version);

    long		getLastPlayed() const;

	int			getStorageVersion() const;
	void		setStorageVersion(int version);
	int			getGameType() const;
	void		setGameType(int type);
	bool		getSpawnMobs() const;
	void		setSpawnMobs(bool doSpawn);
	bool		getCaves() const;
	void		setCaves(bool enabled);
	bool		getRavines() const;
	void		setRavines(bool enabled);
	bool		getWaterLakes() const;
	void		setWaterLakes(bool enabled);
	bool		getLavaLakes() const;
	void		setLavaLakes(bool enabled);
	bool		getWaterSprings() const;
	void		setWaterSprings(bool enabled);
	bool		getLavaSprings() const;
	void		setLavaSprings(bool enabled);
	bool		getBiomeGrassTint() const;
	void		setBiomeGrassTint(bool enabled);
	bool		getTallGrassEnabled() const;
	void		setTallGrassEnabled(bool enabled);
	bool		getBetaWorldGeneration() const;
	void		setBetaWorldGeneration(bool enabled);
	bool		getModernTerrainGeneration() const;
	void		setModernTerrainGeneration(bool enabled);
	bool		getExperimentalGameplayFeatures() const;
	void		setExperimentalGameplayFeatures(bool enabled);
	bool		getKeepInventory() const;
	void		setKeepInventory(bool enabled);

public:
	PlayerData playerData;
	int playerDataVersion;
	std::string levelName;
private:
	long seed;
	int xSpawn;
	int ySpawn;
	int zSpawn;
	long time;
	int lastPlayed;
	long sizeOnDisk;
	CompoundTag* loadedPlayerTag;
	int dimension;
	int gameType;
	int storageVersion;
	bool spawnMobs;
	//@note: This version is never written or loaded to disk. The only purpose
	//       is to use it in the level generator on server and clients.
	int generatorVersion;
	bool caves;
	bool ravines;
	bool waterLakes;
	bool lavaLakes;
	bool	waterSprings;
	bool	lavaSprings;
	bool	biomeGrassTint;
	bool	tallGrassEnabled;
	bool	betaWorldGeneration;
	bool	modernTerrainGeneration;
	bool	experimentalGameplayFeatures;
	bool	keepInventory;
};

#endif /*NET_MINECRAFT_WORLD_LEVEL_STORAGE__LevelData_H__*/
