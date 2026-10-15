#ifndef TCPE_JAVA_WEAPON_ANIMATION_H
#define TCPE_JAVA_WEAPON_ANIMATION_H
#include <cmath>
#include <algorithm>
// Java 1.21.11 SpearAnimations.firstPersonAttack timing/translation.
// Pure transform state so both touch and keyboard attacks use the same animation.
namespace JavaWeaponAnimation {
struct Jab {float x,y,z,pitch,tailZ;};
inline float progress(float t,float a,float b){return std::max(0.f,std::min(1.f,(t-a)/(b-a)));}
inline float inOutSine(float x){return -(std::cos(3.14159265358979323846f*x)-1.f)/2.f;}
inline float outBack(float x){const float c=1.70158f;const float y=x-1;return 1+(c+1)*y*y*y+c*y*y;}
inline float inOutExpo(float x){return x==0?0:x==1?1:x<.5f?std::pow(2.f,20*x-10)/2:(2-std::pow(2.f,-20*x+10))/2;}
inline Jab jab(float swing,int hand=1){
 if(!std::isfinite(swing))swing=0; swing=std::max(0.f,std::min(1.f,swing));
 const float a=inOutSine(progress(swing,0,.05f)),b=outBack(progress(swing,.05f,.2f)),c=inOutExpo(progress(swing,.4f,1));
 return {hand*.1f*(a-b),-.075f*(a-c),.65f*(a-b),-70.f*(a-c),-.25f*(c-b)};
}
// Component-driven Java SpearAnimations.firstPersonUse parameters.
struct Charge {float x,y,z,pitch,yaw,feedback;};
inline float interval(float t,float a,float b){return a==b?(t>=b?1.f:0.f):progress(t,a,b);}
inline float inOutElastic(float x){if(x==0||x==1)return x;const double wave=std::sin((20.*x-11.125)*1.3962634801864624);return float(x<.5?-std::pow(2.,20.*x-10)*wave/2:std::pow(2.,-20.*x+10)*wave/2+1);}
inline float inOutBack(float x){if(x<.5f)return 4*x*x*(7.189819f*x-2.5949094f)/2;const float y=2*x-2;return (y*y*(3.5949094f*y+2.5949094f)+2)/2;}
struct UsePhases {float raise,start,middle,end,sway,lower,back,intensity,slow,fast;};
inline UsePhases usePhases(float ticks,float delay,float dismount,float knockback,float damage){
 if(!std::isfinite(ticks)||ticks<0)ticks=0;
 const float raise=interval(ticks,0,delay),start=progress(raise,0,.5f),middle=progress(raise,.5f,.8f),end=progress(raise,.8f,1);
 const float sway=interval(ticks,delay+dismount-20,delay+knockback-40);
 const float elastic=inOutElastic(interval(ticks-20,delay+knockback-40,delay+knockback));
 const float lower=1-std::pow(1-elastic,3.f),back=interval(ticks,delay+damage-5,delay+damage);
 const float intensity=2*std::sqrt(std::max(0.f,1-(sway-1)*(sway-1)))-2*(1-std::sqrt(std::max(0.f,1-back*back)));
 return {raise,start,middle,end,sway,lower,back,intensity,std::sin(ticks*19*.017453292f)*intensity,std::sin(ticks*30*.017453292f)*intensity};
}
inline Charge charge(float ticks,float delay,float dismount,float knockback,float damage,float hitAge=0,int hand=1){
 const auto p=usePhases(ticks,delay,dismount,knockback,damage);
 const float feedback=.4f*((1-std::pow(1-progress(hitAge,1,3),4.f))-inOutSine(progress(hitAge,3,10)));
 return {hand*(p.raise*.15f-p.end*.05f-p.sway*.1f+p.slow*.005f),p.raise*-.075f+p.middle*.075f+p.fast*.01f,p.start*.05f-p.end*.05f+p.slow*.005f,-65*inOutBack(p.raise)-35*p.lower+100*p.back-.5f*p.fast,hand*(-90*progress(p.raise,.5f,.55f)+90*p.sway+2*p.slow),feedback};
}
struct Arm {float pitch,yaw,roll;};
inline Arm thirdPersonArm(float headPitch,float headYaw,float ticks,float delay,float dismount,float knockback,float damage,int hand=1){
 const auto p=usePhases(ticks,delay,dismount,knockback,damage);constexpr float rad=.017453292f;
 const float pitch=std::max(-120*rad,std::min(30*rad,-1.5707964f+headPitch+.8f));
 const float yaw=std::max(-60*rad,std::min(60*rad,headYaw-hand*.1f));
 if(ticks<=0)return {pitch,yaw,0};
 return {pitch+rad*(-40*p.start+30*p.middle-20*p.end+20*p.lower+10*p.back+.6f*p.slow*p.intensity),yaw-hand*p.fast*rad*p.intensity,-hand*p.slow*rad*p.intensity*.5f};
}

}
#endif
