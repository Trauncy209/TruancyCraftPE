#ifndef TCPE_JAVA_CROSSBOW_ANIMATION_H
#define TCPE_JAVA_CROSSBOW_ANIMATION_H
#include <algorithm>
#include <cmath>
#include <string>
namespace JavaCrossbowAnimation {
struct Arms {float mainX,mainY,otherX,otherY;};
inline Arms armsCharge(float ticks,float duration,int hand=1){
 const float sign=hand<0?-1.f:1.f;
 const float progress=std::isfinite(ticks)&&std::isfinite(duration)&&duration>0?std::max(0.f,std::min(duration,ticks))/duration:0;
 return {-.97079635f,sign*-.8f,-.97079635f+progress*(-1.5707964f+.97079635f),sign*(.4f+progress*(.85f-.4f))};
}
inline Arms armsHold(float headX,float headY,int hand=1){
 const float sign=hand<0?-1.f:1.f;
 return {-1.5707964f+headX+.1f,sign*-.3f+headY,-1.5f+headX,sign*.6f+headY};
}
inline const char* resource(bool usingItem,float ticks,float chargeDuration,const std::string& projectile){
 if(projectile=="arrow")return "minecraft:crossbow_arrow";
 if(projectile=="rocket")return "minecraft:crossbow_firework";
 if(!usingItem)return "minecraft:crossbow";
 const float pull=std::isfinite(ticks)&&std::isfinite(chargeDuration)&&chargeDuration>0?ticks/chargeDuration:0;
 return pull>=1?"minecraft:crossbow_pulling_2":pull>=.58f?"minecraft:crossbow_pulling_1":"minecraft:crossbow_pulling_0";
}
struct Charge {float x,y,z,pitch,yaw,roll,shakeY,drawZ,scaleZ,tailYaw,progress;};
// Java 1.21.11 ItemInHandRenderer crossbow branch. Apply transforms in field
// order: translation, XYZ rotations, shake, draw, scale, final negative-Y turn.
inline Charge charge(float ticks,float partial,float chargeDuration,int hand=1){
 const float sign=hand<0?-1.f:1.f;
 const float elapsed=ticks+partial-1.f;
 const float progress=std::isfinite(elapsed)&&std::isfinite(chargeDuration)&&chargeDuration>0?std::min(1.f,elapsed/chargeDuration):0.f;
 const float shake=progress>.1f?std::sin((elapsed-.1f)*1.3f)*(progress-.1f)*.004f:0.f;
 return {sign*-.4785682f,-.094387f,.05731531f,-11.935f,sign*65.3f,sign*-9.785f,shake,progress*.04f,1.f+progress*.2f,sign*-45.f,progress};
}
}
#endif
