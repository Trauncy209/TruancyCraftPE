#ifndef TCPE_WORLD_WEATHER_STATE_H
#define TCPE_WORLD_WEATHER_STATE_H
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

// Independent implementation of the observable scheduling/transition rules
// documented in docs/weather/REFERENCE.md. No reference source/assets included.
struct WeatherState {
    bool raining=false, thundering=false;
    int rainTicks=0, thunderTicks=0;
    float rain=0, previousRain=0, thunder=0, previousThunder=0;
    static constexpr int ReferenceDayTicks=24000;

    // Administrative changes keep the current intensity for a smooth transition.
    bool command(const char* kind,int ticks) {
        if(ticks<1 || ticks>1000000)return false;
        const std::string value(kind);
        if(value!="clear" && value!="rain" && value!="thunder")return false;
        raining=value!="clear";thundering=value=="thunder";
        rainTicks=thunderTicks=ticks;
        return true;
    }
    void clearAfterSleep() {raining=false;thundering=false;rainTicks=thunderTicks=0;}
    void restoreIntensity() {
        rain=previousRain=raining?1.f:0.f;
        thunder=previousThunder=thundering?1.f:0.f;
    }
    static float bounded(float value) {
        return std::isfinite(value)?std::max(0.f,std::min(1.f,value)):0.f;
    }
    float rainAt(float fraction) const {
        fraction=bounded(fraction);
        return bounded(previousRain+(rain-previousRain)*fraction);
    }
    float thunderAt(float fraction) const {
        fraction=bounded(fraction);
        return bounded(previousThunder+(thunder-previousThunder)*fraction)*rainAt(fraction);
    }
    bool wet() const {return rainAt(1.f)>0.2f;}
    bool storm() const {return thunderAt(1.f)>0.9f;}

    // randomBelow(n) supplies a uniformly selected integer in [0,n).
    // Scheduling runs only on the authority; clients advance intensity only.
    template<class RandomBelow> void tick(bool authority,bool ceiling,RandomBelow randomBelow) {
        if(ceiling)return;
        if(authority) {
            if(thunderTicks<=0)thunderTicks=thundering?3600+randomBelow(12000):12000+randomBelow(168000);
            else if(--thunderTicks==0)thundering=!thundering;
            if(rainTicks<=0)rainTicks=12000+randomBelow(raining?12000:168000);
            else if(--rainTicks==0)raining=!raining;
        }
        previousRain=rain;previousThunder=thunder;
        rain=bounded(rain+(raining?0.01f:-0.01f));
        thunder=bounded(thunder+(thundering?0.01f:-0.01f));
    }
};
#endif
