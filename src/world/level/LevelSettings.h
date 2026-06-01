#ifndef NET_MINECRAFT_WORLD_LEVEL__LevelSettings_H__
#define NET_MINECRAFT_WORLD_LEVEL__LevelSettings_H__

//package net.minecraft.world.level;

#include "../../SharedConstants.h"

namespace GameType {
	const int Undefined = -1;
	const int Survival = 0;
	const int Creative = 1;

	const int Default = Creative;
}

class LevelSettings
{
public:
    LevelSettings(long seed, int gameType, int generatorVersion = (int)LGV_ORIGINAL,
        bool caves = false,
        bool ravines = false,
        bool waterLakes = false,
        bool lavaLakes = false,
        bool waterSprings = false,
        bool lavaSprings = false,
        bool biomeGrassTint = true,
        bool tallGrassEnabled = true,
        bool betaWorldGeneration = false,
        bool modernTerrainGeneration = false,
        bool experimentalGameplayFeatures = false)
    :   seed(seed),
        gameType(validateGameType(gameType)),
        generatorVersion(validateGeneratorVersion(generatorVersion)),
        caves(caves),
        ravines(ravines),
        waterLakes(waterLakes),
        lavaLakes(lavaLakes),
        waterSprings(waterSprings),
        lavaSprings(lavaSprings),
        biomeGrassTint(biomeGrassTint),
        tallGrassEnabled(tallGrassEnabled),
        betaWorldGeneration(betaWorldGeneration && !modernTerrainGeneration),
        modernTerrainGeneration(modernTerrainGeneration),
        experimentalGameplayFeatures(experimentalGameplayFeatures)
    {
    }
	static LevelSettings None() {
		return LevelSettings(-1, -1, -1);
	}

    long getSeed() const { return seed; }
    int getGameType() const { return gameType; }
    int getGeneratorVersion() const { return generatorVersion; }
    bool getCaves() const { return caves; }
    bool getRavines() const { return ravines; }
    bool getWaterLakes() const { return waterLakes; }
    bool getLavaLakes() const { return lavaLakes; }
    bool getWaterSprings() const { return waterSprings; }
    bool getLavaSprings() const { return lavaSprings; }
    bool getBiomeGrassTint() const { return biomeGrassTint; }
    bool getTallGrassEnabled() const { return tallGrassEnabled; }
    bool getBetaWorldGeneration() const { return betaWorldGeneration; }
    bool getModernTerrainGeneration() const { return modernTerrainGeneration; }
    bool getExperimentalGameplayFeatures() const { return experimentalGameplayFeatures; }

	//
	// Those two should actually not be here
	// @todo: Move out when we add LevelSettings.cpp :p
	//
	static int validateGameType(int gameType) {
        switch (gameType) {
			case GameType::Creative:
			case GameType::Survival:
            return gameType;
        }
        return GameType::Default;
    }

	static std::string gameTypeToString(int gameType) {
		if (gameType == GameType::Survival) return "Survival";
		if (gameType == GameType::Creative) return "Creative";
		return "Undefined";
	}

	static int validateGeneratorVersion(int generatorVersion) {
		switch (generatorVersion) {
		case LGV_ORIGINAL:
		case LGV_INFINITE:
			return generatorVersion;
		}
		return LGV_ORIGINAL;
	}

	static std::string generatorVersionToString(int generatorVersion) {
		if (generatorVersion == LGV_INFINITE) return "Infinite";
		if (generatorVersion == LGV_ORIGINAL) return "Classic";
		return "Unknown";
	}

	static bool defaultCavesForGenerator(int generatorVersion) { return generatorVersion == LGV_INFINITE; }
	static bool defaultRavinesForGenerator(int generatorVersion) { return generatorVersion == LGV_INFINITE; }
	static bool defaultWaterLakesForGenerator(int generatorVersion) { return generatorVersion == LGV_INFINITE; }
	static bool defaultLavaLakesForGenerator(int) { return false; }
	static bool defaultWaterSpringsForGenerator(int generatorVersion) { return generatorVersion == LGV_INFINITE; }
	static bool defaultLavaSpringsForGenerator(int generatorVersion) { return generatorVersion == LGV_INFINITE; }

private:
    const long seed;
    const int gameType;
    const int generatorVersion;
    const bool caves;
    const bool ravines;
    const bool waterLakes;
    const bool lavaLakes;
    const bool waterSprings;
    const bool lavaSprings;
    const bool biomeGrassTint;
    const bool tallGrassEnabled;
    const bool betaWorldGeneration;
    const bool modernTerrainGeneration;
    const bool experimentalGameplayFeatures;
};

#endif /*NET_MINECRAFT_WORLD_LEVEL__LevelSettings_H__*/
