#ifndef TCPE_JAVA_COMPASS_FRAMES_H
#define TCPE_JAVA_COMPASS_FRAMES_H
#include <cmath>
#include <cstdio>
#include <string>
namespace JavaCompassFrames {
// Official 1.21.11 compass/recovery_compass item range_dispatch: scale32,
// frame16 at0; frame17 at0.5; ... frame16 again at31.5.
inline int frame(double angle){if(!std::isfinite(angle))return 16;angle-=std::floor(angle);return (int(std::floor(angle*32.+.5))+16)%32;}
inline std::string resource(double angle,bool recovery){char suffix[4];std::snprintf(suffix,sizeof(suffix),"%02d",frame(angle));return std::string(recovery?"minecraft:recovery_compass_":"minecraft:compass_")+suffix;}
}
#endif
