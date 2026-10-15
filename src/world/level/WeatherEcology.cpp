#include "WeatherEcology.h"
#include "Level.h"
#include "LevelConstants.h"
#include "LightLayer.h"
#include "biome/Biome.h"
#include "biome/BiomeSource.h"
#include "dimension/Dimension.h"
#include "material/Material.h"
#include "tile/Tile.h"

namespace WeatherEcology {
float temperature(const Biome* biome) {
    // TCPE has climate-noise biome selection but no reference-style per-biome
    // temperature property. Preserve its snowy biome classification rather
    // than equating its incompatible generator noise with reference climate.
    if (!biome) return 1.f;
    if (biome==Biome::taiga) return 0.05f;
    return biome->hasSnow()?0.f:1.f;
}
static bool loaded(Level& level,int x,int y,int z) {
    return y>=0 && y<LEVEL_HEIGHT && level.hasChunkAtNow(x,y,z);
}
bool shouldFreeze(Level& level,int x,int y,int z,bool checkNeighbors) {
    if (!loaded(level,x,y,z)) return false;
    Biome* biome=level.getBiomeSource()->getBiome(x,z);
    const int tile=level.getTile(x,y,z);
    const bool water=tile==Tile::water->id || tile==Tile::calmWater->id;
    bool enclosed=true;
    if (checkNeighbors) {
        const int dx[4]={-1,1,0,0}, dz[4]={0,0,-1,1};
        for(int i=0;i<4;++i) {
            // Never generate a neighboring chunk as a weather side effect.
            if (!loaded(level,x+dx[i],y,z+dz[i])) return false;
            if (level.getMaterial(x+dx[i],y,z+dz[i])!=Material::water) enclosed=false;
        }
    }
    return canFreeze({temperature(biome),y,level.getBrightness(LightLayer::Block,x,y,z),
        level.getData(x,y,z),water,enclosed},checkNeighbors);
}
bool shouldSnow(Level& level,int x,int y,int z) {
    if (!loaded(level,x,y,z) || y==0) return false;
    const int below=level.getTile(x,y-1,z);
    const bool solid=below>0 && below<Tile::NUM_BLOCK_TYPES && Tile::tiles[below] &&
        Tile::tiles[below]->material->blocksMotion();
    return canSnow({temperature(level.getBiomeSource()->getBiome(x,z)),y,
        level.getBrightness(LightLayer::Block,x,y,z),level.getTile(x,y,z)==0,
        Tile::topSnow->mayPlace(&level,x,y,z),solid,below==Tile::ice->id});
}
bool exposedToRain(Level& level,int x,int y,int z) {
    if (level.isJavaServerLevel() || !level.dimension || level.dimension->hasCeiling ||
        !level.getLevelData()->weather.wet() || !loaded(level,x,y,z)) return false;
    Biome* biome=level.getBiomeSource()->getBiome(x,z);
    return biome && biome->hasRain() && level.getTopRainBlock(x,z)<=y;
}
void tickChunk(Level& level,int chunkX,int chunkZ) {
    if (level.isClientSide || level.isJavaServerLevel() || !level.dimension ||
        level.dimension->hasCeiling || !level.hasChunkNow(chunkX,chunkZ)) return;
    if (level.random.nextInt(16)!=0) return;
    const int x=chunkX*16+level.random.nextInt(16);
    const int z=chunkZ*16+level.random.nextInt(16);
    // setTile notifies six neighbors; defer seam mutations until those
    // chunks exist so notification cannot generate terrain either.
    if (!loaded(level,x-1,64,z) || !loaded(level,x+1,64,z) ||
        !loaded(level,x,64,z-1) || !loaded(level,x,64,z+1)) return;
    const int y=level.getTopRainBlock(x,z);
    if (shouldFreeze(level,x,y-1,z)) level.setTile(x,y-1,z,Tile::ice->id);
    if (level.getLevelData()->weather.wet() && shouldSnow(level,x,y,z))
        level.setTile(x,y,z,Tile::topSnow->id);
    // TCPE has no cauldron/handleRain block API. Fire extinction is handled by
    // its own tick using exposedToRain, so the unrelated random tile sequence
    // does not grant sheltered fires an extra extinction check.
}
}
