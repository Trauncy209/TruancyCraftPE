#ifndef TCPE_JAVA_DROPPED_COMPASS_H
#define TCPE_JAVA_DROPPED_COMPASS_H
#include "JavaGameSession.h"
#include "JavaCompassFrames.h"
#include <cstdint>
class JavaDroppedCompass {
 uint64_t randomState=0;bool seeded=false;
 float nextFloat(){randomState=(randomState*25214903917ULL+11ULL)&((1ULL<<48)-1);return float(randomState>>24)/16777216.f;}
 double noTargetRotation=0,noTargetVelocity=0;
 unsigned long long lastTick=0;
 static double wrap(double value){return value-std::floor(value);}
public:
 std::string resource(const JavaGameSession::CompassState& compass,const JavaGameSession::Item& item,double x,double z,float age,float bobOffs,unsigned long long tick,int seed){
  if(!seeded){randomState=(uint64_t(uint32_t(seed))^25214903917ULL)&((1ULL<<48)-1);seeded=true;}
  const bool recovery=item.resourceId=="minecraft:recovery_compass";
  if(item.resourceId!="minecraft:compass"&&!recovery)return item.resourceId;
  const auto& target=recovery?compass.lastDeath:(item.hasLodestoneTracker?item.lodestoneTarget:compass.defaultSpawn);
  const double dx=target.position[0]+.5-x,dz=target.position[2]+.5-z;
  const bool valid=target.valid&&target.dimension==compass.dimension&&std::isfinite(x)&&std::isfinite(z)&&std::isfinite(age)&&std::isfinite(bobOffs)&&dx*dx+dz*dz>=1e-5;
  double angle;
  if(valid){
   // ItemEntity.getVisualRotationYInDegrees uses spin(age+0.5,bobOffs),
   // not server yaw or the local player's smoothed heading.
   const double visualYaw=180.-((age+.5)/20.+bobOffs)*360./6.2831854820251465;
   angle=wrap(.75-visualYaw/360.+std::atan2(dz,dx)/6.2831854820251465);
  }else{
   // Separate no-target wobbler: never update the held-player compass state.
   if(lastTick!=tick){lastTick=tick;noTargetVelocity=(noTargetVelocity+(wrap(nextFloat()-noTargetRotation+.5)-.5)*.1)*.8;noTargetRotation=wrap(noTargetRotation+noTargetVelocity);}
   const uint32_t bits=uint32_t(seed)*uint32_t(1327217883);
   const double hash=bits<=uint32_t(INT32_MAX)?double(bits):double(bits)-4294967296.;
   angle=wrap(noTargetRotation+hash/2147483648.);
  }
  return JavaCompassFrames::resource(angle,recovery);
 }
};
#endif
