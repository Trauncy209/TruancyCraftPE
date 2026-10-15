#ifndef TCPE_WEATHER_LIGHTNING_H
#define TCPE_WEATHER_LIGHTNING_H
#include <cmath>
#include <cstdint>

// Independent behavior model; provenance and deliberate network choices are in
// docs/weather/LIGHTNING.md. The caller owns all world mutation and permissions.
struct WeatherLightning {
    struct Effects {
        bool sound=false, illuminate=false, hitEntities=false;
        bool retryFire=false, removed=false;
    };
    double x=0,y=0,z=0;
    uint64_t id=0, seed=0;
    int life=2, flashes=1;
    bool removed=false;

    bool visible() const {return !removed && life>=0;}
    bool valid() const {
        return id!=0 && std::isfinite(x) && std::isfinite(y) && std::isfinite(z)
            && std::abs(x)<=30000000.0 && std::abs(z)<=30000000.0
            && y>=-4096 && y<=4096 && life>=-10 && life<=2
            && flashes>=0 && flashes<=1;
    }
    // Authority advances randomness. Clients receive snapshots on both flashes;
    // they never independently choose a delay or seed that could diverge.
    template<class RandomBelow,class RandomSeed>
    Effects tick(bool authority, RandomBelow randomBelow,RandomSeed randomSeed) {
        Effects effect;
        if(removed){effect.removed=true;return effect;}
        effect.sound=life==2;
        --life;
        if(life<0) {
            if(flashes==0)removed=true;
            else if(authority && life < -randomBelow(10)) {
                --flashes;life=1;seed=randomSeed();effect.retryFire=true;
            }
        }
        effect.illuminate=!removed && life>=0;
        effect.hitEntities=authority && effect.illuminate;
        effect.removed=removed;
        return effect;
    }
    static bool initialFireAllowed(bool authority,int difficulty,bool loaded) {
        return authority && difficulty>=2 && loaded;
    }
    static bool retryFireAllowed(bool authority,bool loaded) {return authority && loaded;}
    static bool fireAtAllowed(bool tracked,bool air,bool mayPlace) {return tracked && air && mayPlace;}
    bool withinHitBox(double ex,double ey,double ez,double width,double height) const {
        return ex+width/2>x-3 && ex-width/2<x+3 && ey+height>y-3 && ey<y+9
            && ez+width/2>z-3 && ez-width/2<z+3;
    }
};
#endif
