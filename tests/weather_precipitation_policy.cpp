#include "../src/client/renderer/WeatherPrecipitationPolicy.h"
#include <cassert>
#include <cmath>
#include <iostream>
static void close(double actual,double expected,double tolerance=1e-6) {
    assert(std::abs(actual-expected)<=tolerance);
}
int main() {
    using namespace WeatherPrecipitationPolicy;
    // Java-compatible 48-bit generator known answers, independently checked.
    ColumnRandom floats(0),doubles(0),gaussians(0);
    close(floats.unitFloat(),.7309677600860596,1e-12);
    close(doubles.unitDouble(),.730967787376657,1e-15);
    close(gaussians.gaussian(),.8025330637390305,1e-14);
    close(gaussians.gaussian(),-.9015460884175122,1e-14);
    assert(columnSeed(12,34)==UINT64_C(1015186730));
    assert(columnSeed(-12,34)==UINT64_C(18446744072691090354));
    assert(columnSeed(2147483647,-2147483647)==UINT64_C(18446744073664157742));
    assert(radius(1)==9&&radius(2)==7&&radius(3)==5&&radius(4)==5);
    // Pinned scaleTemp has no altitude adjustment. The .15 boundary is rain.
    assert(snow(.14999f,0)&&snow(.14999f,127));
    assert(!snow(.15f,127)&&scaledTemperature(.8f,10000)==.8f);
    const auto rain=sample(12,34,60,.5f,true,10.25,64.8,30.75,64.2,321,.5f,.8f);
    assert(rain.visible&&!rain.snowy&&rain.bottom==60&&rain.top==73&&rain.lightY==64);
    close(rain.sideX,-.4472135901451111);close(rain.sideZ,.2236067950725555);
    close(rain.lowerAlpha,.7055555582046509);assert(rain.upperAlpha==0);
    assert(rain.u0==0&&rain.u1==1);
    close(rain.v0,17.25702667236328,2e-6);close(rain.v1,20.50702667236328,2e-6);
    const auto flakes=sample(12,34,60,0.f,true,10.25,64.8,30.75,64.2,321,.5f,.8f);
    assert(flakes.visible&&flakes.snowy);
    close(flakes.lowerAlpha,.5833333730697632);
    assert(flakes.upperAlpha==flakes.lowerAlpha);
    close(flakes.u0,-.9143063426017761);close(flakes.u1,.08569365739822388);
    close(flakes.v0,16.411897659301758,2e-6);close(flakes.v1,19.661897659301758,2e-6);
    const auto wrapped=sample(12,34,60,.5f,true,10.25,64.8,30.75,64.2,353,.5f,.8f);
    assert(wrapped.v0==rain.v0&&wrapped.v1==rain.v1);
    assert(!sample(12,34,73,.5f,true,10.25,64.8,30.75,64.2,321,.5f,.8f).visible);
    assert(!sample(12,34,60,.5f,false,10.25,64.8,30.75,64.2,321,.5f,.8f).visible);
    assert(!sample(12,34,60,.5f,true,10.25,64.8,30.75,64.2,321,.5f,0).visible);
    // Center normal is undefined in the reference; explicitly omit that quad.
    assert(!sample(10,30,60,.5f,true,10.25,64.8,30.75,64.2,321,.5f,.8f).visible);
    const auto cardinal=sample(10,31,60,.5f,true,10.25,64.8,30.75,67.2,0,0,1);
    close(cardinal.sideX,-.5);close(cardinal.sideZ,0);assert(cardinal.lightY==67);
    assert(!sample(18,30,60,.5f,true,10.25,64.8,30.75,64.2,0,0,1,2).visible);
    // Shader/output clamps can handle negative corner alpha; policy preserves
    // reference math instead of silently imposing a different radial fade.
    const auto corner=sample(19,39,60,.5f,true,10.,64.,30.,64.,0,0,1);
    assert(corner.visible&&corner.lowerAlpha<0);
    std::cout<<"Precipitation seed/LCG/Gaussian, UV, directions, alpha, heights, biome and temperature policy PASS\n";
}
