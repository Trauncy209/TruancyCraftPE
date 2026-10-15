#include "world/level/WeatherEcology.h"
#include <cassert>
#include <limits>
int main() {
 using namespace WeatherEcology;
 FreezeSample f={.15f,64,9,0,true,false};
 assert(canFreeze(f));f.temperature=.15001f;assert(!canFreeze(f));f.temperature=0;
 f.y=-1;assert(!canFreeze(f));f.y=128;assert(!canFreeze(f));f.y=127;assert(canFreeze(f));
 f.blockLight=10;assert(!canFreeze(f));f.blockLight=9;f.data=1;assert(!canFreeze(f));
 f.data=0;f.sourceWater=false;assert(!canFreeze(f));f.sourceWater=true;
 f.enclosedByWater=true;assert(!canFreeze(f));assert(canFreeze(f,false));
 f.temperature=std::numeric_limits<float>::quiet_NaN();assert(!canFreeze(f));
 SnowSample s={.15f,64,9,true,true,true,false};assert(canSnow(s));
 s.temperature=.15001f;assert(!canSnow(s));s.temperature=0;s.y=-1;assert(!canSnow(s));
 s.y=128;assert(!canSnow(s));s.y=127;assert(canSnow(s));s.blockLight=10;assert(!canSnow(s));
 s.blockLight=9;s.empty=false;assert(!canSnow(s));s.empty=true;s.mayPlace=false;assert(!canSnow(s));
 s.mayPlace=true;s.solidBelow=false;assert(!canSnow(s));s.solidBelow=true;s.iceBelow=true;assert(!canSnow(s));
 s.iceBelow=false;assert(canSnow(s));
}
