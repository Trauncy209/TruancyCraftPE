#ifndef TCPE_WORLD_WEATHER_ECOLOGY_H
#define TCPE_WORLD_WEATHER_ECOLOGY_H
class Level;
class Biome;

// Independently expressed weather rules; behavioral sources and TCPE biome
// compatibility limits are recorded in docs/weather/REFERENCE.md.
namespace WeatherEcology {
struct FreezeSample {
    float temperature;
    int y, blockLight, data;
    bool sourceWater, enclosedByWater;
};
inline bool canFreeze(const FreezeSample& s, bool checkNeighbors=true) {
    return s.temperature<=0.15f && s.y>=0 && s.y<128 && s.blockLight<10 &&
        s.sourceWater && s.data==0 && (!checkNeighbors || !s.enclosedByWater);
}
struct SnowSample {
    float temperature;
    int y, blockLight;
    bool empty, mayPlace, solidBelow, iceBelow;
};
inline bool canSnow(const SnowSample& s) {
    return s.temperature<=0.15f && s.y>=0 && s.y<128 && s.blockLight<10 &&
        s.empty && s.mayPlace && s.solidBelow && !s.iceBelow;
}
float temperature(const Biome* biome);
bool shouldFreeze(Level& level,int x,int y,int z,bool checkNeighbors=true);
bool shouldSnow(Level& level,int x,int y,int z);
// One eligible, already-loaded simulation chunk; no terrain generation allowed.
void tickChunk(Level& level,int chunkX,int chunkZ);
// Fire rain exposure includes cold/dry exclusions and overhead shelter.
bool exposedToRain(Level& level,int x,int y,int z);
}
#endif
