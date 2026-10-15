#ifndef TCPE_WEATHER_CELESTIAL_POLICY_H
#define TCPE_WEATHER_CELESTIAL_POLICY_H
#include <algorithm>
namespace WeatherCelestialPolicy {
// Pinned reference attenuates celestial bodies with rain alone. Effective
// thunder already contains rain; multiplying another thunder factor is wrong.
inline float visibility(float rain,bool javaWorld=false) {
    return javaWorld?1.f:1.f-std::max(0.f,std::min(1.f,rain));
}
struct Stars {float brightness,alpha;};
inline Stars stars(float clearBrightness,float rain,bool javaWorld=false) {
    const float fade=visibility(rain,javaWorld);
    // Preserve TCPE's clear brightness while matching the reference's relative
    // rain attenuation in both color and alpha (additive SRC_ALPHA blending).
    return {clearBrightness*fade,fade};
}
}
#endif
