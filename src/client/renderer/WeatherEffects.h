#ifndef TCPE_WEATHER_EFFECTS_H
#define TCPE_WEATHER_EFFECTS_H
#include "../../world/level/WeatherLightningGeometry.h"
#include <algorithm>
#include <cmath>
class Minecraft;
class Level;
// Pure impact policy is shared by the runtime and boundary tests.
namespace WeatherImpactPolicy {
inline int attempts(float rain, bool fancy, int particles = 0, int localPlayers = 1) {
    if (!std::isfinite(rain) || rain <= 0 || particles >= 2) return 0;
    rain = std::min(1.f, rain) * (fancy ? 1.f : .5f);
    rain /= std::max(1,localPlayers)+1;
    int n = int(100.f * rain * rain);
    return particles == 1 ? n / 2 : n;
}
class Random48 {
    WeatherLightningGeometry::Random48 source;
public:
    explicit Random48(uint64_t seed=0):source(seed){}
    void setSeed(uint64_t seed){source=WeatherLightningGeometry::Random48(seed);}
    int nextInt(int bound){return source.below(bound);}
    float nextFloat(){return float(source.bits(24))/16777216.f;}
};
inline uint64_t tickSeed(uint32_t tick) {
    const uint32_t value=tick*312987231u;
    return uint64_t(value<UINT32_C(0x80000000)?int64_t(value):int64_t(value)-INT64_C(4294967296));
}
inline int lifetime(double uniform) {
    return int(8.0/(std::max(0.0,std::min(1.0,uniform))*.8+.2));
}
inline float impactHeight(int surface,float shapeTop) {return surface-1+shapeTop+.1f;}
inline bool submerged(float y,int tileY,int data,bool solidOrLiquid) {
    if(!solidOrLiquid)return false;
    if(data>=8)data=0;
    return y<tileY+1-(data+1)/9.f;
}
inline bool eligible(bool loaded, bool rainBiome, int surface, int eye) {
    return loaded && rainBiome && surface >= eye - 10 && surface <= eye + 10;
}
inline bool sheltered(float impactY, float cameraY, int roof, int eye) {
    return impactY > cameraY + 1.f && roof > eye;
}
}
// Client-only, once per simulation tick; neither sends packets nor changes weather.
class WeatherEffects {
public:
    WeatherEffects() : previousLevel(0), rainSoundTime(0), tickCounter(0), random(0) {}
    void tick(Minecraft& minecraft);
    void tick(Minecraft& minecraft,uint32_t rendererTick);
    void reset() { previousLevel = 0; rainSoundTime = 0; tickCounter = 0; }
private:
    Level* previousLevel;
    int rainSoundTime;
    unsigned tickCounter;
    WeatherImpactPolicy::Random48 random;
};
#endif
