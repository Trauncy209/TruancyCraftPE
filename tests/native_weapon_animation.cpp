#include "network/java/JavaWeaponAnimation.h"
#include <cassert>
#include <cmath>
#include <limits>
int main(){using namespace JavaWeaponAnimation;auto a=jab(0),b=jab(1);assert(a.x==0&&a.y==0&&a.z==0&&a.pitch==0);assert(b.x==0&&b.y==0&&b.z==0&&b.pitch==0);auto peak=jab(.05f);assert(std::abs(peak.x-.1f)<.00001&&std::abs(peak.z-.65f)<.00001&&std::abs(peak.pitch+70)<.00001);auto left=jab(.05f,-1);assert(left.x==-peak.x&&left.z==peak.z);for(int i=0;i<=1000;i++){auto p=jab(i/1000.f);assert(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::isfinite(p.pitch));}assert(jab(std::numeric_limits<float>::quiet_NaN()).z==0);
for(int i=0;i<=300;++i){auto p=thirdPersonArm(.2f,.3f,float(i),10,100,140,200),q=thirdPersonArm(.2f,-.3f,float(i),10,100,140,200,-1);assert(std::isfinite(p.pitch)&&std::isfinite(p.yaw)&&std::isfinite(p.roll));assert(std::abs(p.pitch-q.pitch)<1e-6&&std::abs(p.yaw+q.yaw)<1e-6&&std::abs(p.roll+q.roll)<1e-6);}

for(int tick=0;tick<=300;++tick){auto right=charge(float(tick),10,100,140,200),left=charge(float(tick),10,100,140,200,0,-1);assert(std::isfinite(right.x)&&std::isfinite(right.y)&&std::isfinite(right.z)&&std::isfinite(right.pitch)&&std::isfinite(right.yaw));assert(left.x==-right.x&&left.y==right.y&&left.z==right.z&&left.yaw==-right.yaw);}
auto idle=charge(0,10,100,140,200);assert(idle.x==0&&idle.y==0&&idle.z==0&&idle.pitch==0&&idle.yaw==0);
auto custom=charge(10,20,100,140,200),normal=charge(10,10,100,140,200);assert(custom.pitch!=normal.pitch);
for(int tick=0;tick<30;++tick){auto absent=charge(float(tick),0,0,0,0);assert(std::isfinite(absent.pitch)&&std::isfinite(absent.yaw));}
}
