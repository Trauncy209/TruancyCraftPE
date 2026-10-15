#ifndef TCPE_WEATHER_PRECIPITATION_CACHE_H
#define TCPE_WEATHER_PRECIPITATION_CACHE_H
#include "WeatherPrecipitationPolicy.h"
#include <array>
// Single-renderer cache of immutable position noise, never frame/biome/world
// state. A 19x19 precipitation window fits without any modulo collisions.
class WeatherPrecipitationCache {
    struct Entry {
        bool valid=false;
        int x=0,z=0;
        WeatherPrecipitationPolicy::Noise value;
    };
    std::array<Entry,32*32> entries;
public:
    static const int SlotCount=32*32;
    // Rain only needs one cheap LCG draw; measured lookup overhead exceeds
    // that cost. Cache Gaussian snow terms without slowing the rain path.
    const WeatherPrecipitationPolicy::Noise* getForSnow(int x,int z,float temperature,int height) {
        return WeatherPrecipitationPolicy::snow(temperature,height)?&get(x,z):nullptr;
    }
    void clear(){for(Entry& entry:entries)entry.valid=false;}
    // Reference remains valid until a colliding coordinate lookup or clear.
    const WeatherPrecipitationPolicy::Noise& get(int x,int z) {
        const unsigned slot=(uint32_t(x)&31u)+((uint32_t(z)&31u)<<5);
        Entry& entry=entries[slot];
        if(!entry.valid || entry.x!=x || entry.z!=z) {
            entry.value=WeatherPrecipitationPolicy::noise(x,z);
            entry.x=x;entry.z=z;entry.valid=true;
        }
        return entry.value;
    }
};
#endif
