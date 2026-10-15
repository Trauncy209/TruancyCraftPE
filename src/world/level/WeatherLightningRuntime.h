#ifndef TCPE_WEATHER_LIGHTNING_RUNTIME_H
#define TCPE_WEATHER_LIGHTNING_RUNTIME_H
#include "WeatherLightning.h"
class Level;
namespace WeatherLightningRuntime {
void tick(Level& level);
void tickChunk(Level& level,int chunkX,int chunkZ);
bool spawn(Level& level,double x,double y,double z);
// Validated, server-originated packet only; phase 0 spawn, 1 flash, 2 remove.
bool receive(Level& level,const WeatherLightning& bolt,unsigned char phase);
}
#endif
