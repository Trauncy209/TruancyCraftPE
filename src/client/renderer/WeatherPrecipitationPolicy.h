#ifndef TCPE_WEATHER_PRECIPITATION_POLICY_H
#define TCPE_WEATHER_PRECIPITATION_POLICY_H
#include "../../world/level/WeatherLightningGeometry.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

// Independently expressed behavioral rules observed at pinned 4jcraft commit
// 529a366ad52a9b66a2ca887647a9abdd9500a0fc, GameRenderer precipitation pass.
// No renderer state, asset loading or reference implementation is embedded here.
namespace WeatherPrecipitationPolicy {
class ColumnRandom {
    WeatherLightningGeometry::Random48 random;
    bool cached;
    double spare;
public:
    explicit ColumnRandom(uint64_t seed): random(seed),cached(false),spare(0) {}
    float unitFloat() { return float(random.bits(24)) / 16777216.f; }
    double unitDouble() {
        const uint64_t fraction=(uint64_t(random.bits(26))<<27)+random.bits(27);
        return double(fraction)/9007199254740992.0;
    }
    double gaussian() {
        if(cached) {cached=false;return spare;}
        double x,y,radius;
        do {x=2.0*unitDouble()-1.0;y=2.0*unitDouble()-1.0;radius=x*x+y*y;}
        while(radius>=1.0 || radius==0.0);
        const double factor=std::sqrt(-2.0*std::log(radius)/radius);
        spare=y*factor;cached=true;
        return x*factor;
    }
};
inline int radius(int localPlayers) {return localPlayers==2?7:localPlayers==3||localPlayers==4?5:9;}
// The pinned scaleTemp implementation returns its input unchanged at every Y.
inline float scaledTemperature(float temperature,int /*height*/) {return temperature;}
inline bool snow(float temperature,int height) {return scaledTemperature(temperature,height)<.15f;}
inline uint32_t xPolynomial(int x) {
    const uint32_t n=uint32_t(x);return n*n*3121u+n*45238971u;
}
inline uint32_t zPolynomial(int z) {
    const uint32_t n=uint32_t(z);return n*n*418711u+n*13761u;
}
inline uint64_t columnSeed(int x,int z) {
    const uint32_t bits=xPolynomial(x)^zPolynomial(z);
    // Emulate the reference's signed 32-bit expression promoted to a 64-bit seed,
    // without C++ signed-overflow UB or implementation-defined unsigned casts.
    const int64_t signedValue=bits<UINT32_C(0x80000000)?int64_t(bits):int64_t(bits)-INT64_C(4294967296);
    return uint64_t(signedValue);
}
// Immutable per-column stochastic terms. Keep the exact reference draw order:
// first float, first Gaussian, second float, cached second Gaussian.
struct Noise {float first,gaussianU,baseV,gaussianV;uint32_t phase;};
inline Noise noise(int x,int z) {
    ColumnRandom random(columnSeed(x,z));
    Noise result;
    result.first=random.unitFloat();
    result.gaussianU=float(random.gaussian());
    result.baseV=random.unitFloat();
    result.gaussianV=float(random.gaussian());
    result.phase=xPolynomial(x)+zPolynomial(z);
    return result;
}
struct Column {
    bool visible=false;
    bool snowy=false;
    float sideX=0,sideZ=0;
    int bottom=0,top=0,lightY=0;
    float lowerAlpha=0,upperAlpha=0;
    float u0=0,u1=1,v0=0,v1=0;
};
// camera* are current simulation coordinates; interpolatedY is only used for
// the light sample. Caller offsets vertices by interpolated camera X/Y/Z.
inline Column sample(int x,int z,int surface,float temperature,bool precipitationBiome,
    double cameraX,double cameraY,double cameraZ,double interpolatedY,
    int32_t tick,float partial,float rain,int localPlayers=1,const Noise* precomputed=nullptr) {
    Column result;
    if(!precipitationBiome || rain<=0) return result;
    const int cx=int(std::floor(cameraX)),cy=int(std::floor(cameraY)),cz=int(std::floor(cameraZ));
    const int range=radius(localPlayers),dx=x-cx,dz=z-cz;
    if(dx < -range || dx > range || dz < -range || dz > range) return result;
    result.bottom=std::max(cy-range,surface);
    result.top=std::max(cy+range,surface);
    result.lightY=std::max(surface,int(std::floor(interpolatedY)));
    if(result.bottom==result.top) return result;
    // At the exact center the reference normal table computes 0/0, producing a
    // degenerate/nonfinite quad. Skip it explicitly to keep GLES buffers finite.
    if(dx==0 && dz==0) return result;
    const float directionLength=std::sqrt(float(dx*dx+dz*dz));
    result.sideX=-float(dz)/directionLength*.5f;
    result.sideZ=float(dx)/directionLength*.5f;
    result.snowy=snow(temperature,surface);
    const double distanceX=(x+.5f)-cameraX,distanceZ=(z+.5f)-cameraZ;
    const float distance=float(std::sqrt(distanceX*distanceX+distanceZ*distanceZ))/range;
    const float radial=1.f-distance*distance;
    result.lowerAlpha=(radial*(result.snowy?.3f:.5f)+.5f)*rain;
    result.upperAlpha=result.snowy?result.lowerAlpha:0.f;
    ColumnRandom random(columnSeed(x,z));
    float scroll,verticalDrift=0.f;
    if(!result.snowy) {
        const uint32_t phase=(uint32_t(tick)+(precomputed?precomputed->phase:xPolynomial(x)+zPolynomial(z)))&31u;
        scroll=(float(phase)+partial)/32.f*(3.f+(precomputed?precomputed->first:random.unitFloat()));
    } else {
        const float time=float(tick)+partial;
        scroll=(float(uint32_t(tick)&511u)+partial)/512.f;
        const float baseU=precomputed?precomputed->first:random.unitFloat();
        const float gaussianU=precomputed?precomputed->gaussianU:float(random.gaussian());
        result.u0=baseU+time*.01f*gaussianU;
        result.u1=1.f+result.u0;
        const float baseV=precomputed?precomputed->baseV:random.unitFloat();
        const float gaussianV=precomputed?precomputed->gaussianV:float(random.gaussian());
        verticalDrift=baseV+time*gaussianV*.001f;
    }
    result.v0=result.bottom/4.f+scroll+verticalDrift;
    result.v1=result.top/4.f+scroll+verticalDrift;
    result.visible=true;
    return result;
}
}
#endif
