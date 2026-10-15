#ifndef NET_MINECRAFT_WORLD_LEVEL_BIOME__RainforestBiome_H__
#define NET_MINECRAFT_WORLD_LEVEL_BIOME__RainforestBiome_H__

//package net.minecraft.world.level.biome;

#include "Biome.h"
#include "../../../util/Random.h"
#include "../levelgen/feature/TreeFeature.h"
#include "../levelgen/feature/TreeVariants.h"

class RainforestBiome: public Biome
{
public:
    Feature* getTreeFeature(Random* random) {
        return createOakVariant(random, false, true);
    }
};

#endif /*NET_MINECRAFT_WORLD_LEVEL_BIOME__RainforestBiome_H__*/
