#include "network/java/JavaCrossbowAnimation.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main(){
 using JavaCrossbowAnimation::charge;
 auto start=charge(1,0,25),half=charge(13.5f,0,25),full=charge(26,0,25),late=charge(80,0,25);
 assert(start.progress==0&&start.scaleZ==1);assert(half.progress==.5f&&half.drawZ==.02f);assert(full.progress==1&&full.scaleZ==1.2f&&late.progress==1);
 auto quick=charge(11,0,10);assert(quick.progress==1);
 auto left=charge(13.5f,0,25,-1);assert(left.x==-half.x&&left.y==half.y&&left.yaw==-half.yaw&&left.roll==-half.roll&&left.tailYaw==-half.tailYaw&&left.shakeY==half.shakeY);
 assert(std::abs(half.shakeY-std::sin((12.5f-.1f)*1.3f)*.4f*.004f)<1e-7f);
 auto arms0=JavaCrossbowAnimation::armsCharge(0,25),arms1=JavaCrossbowAnimation::armsCharge(25,25),armsLeft=JavaCrossbowAnimation::armsCharge(25,25,-1);
 assert(arms0.mainX==-.97079635f&&arms0.mainY==-.8f&&arms0.otherY==.4f);assert(std::abs(arms1.otherX+1.5707964f)<1e-6f&&arms1.otherY==.85f);assert(armsLeft.mainY==-arms1.mainY&&armsLeft.otherY==-arms1.otherY&&armsLeft.otherX==arms1.otherX);
 auto hold=JavaCrossbowAnimation::armsHold(.2f,.3f);assert(std::abs(hold.mainY)<1e-7f&&std::abs(hold.otherY-.9f)<1e-7f);
 assert(charge(0,0,25).progress==-1.f/25);assert(charge(5,0,0).progress==0);
 using JavaCrossbowAnimation::resource;
 assert(std::string(resource(false,0,25,""))=="minecraft:crossbow");assert(std::string(resource(true,14,25,""))=="minecraft:crossbow_pulling_0");assert(std::string(resource(true,14.5f,25,""))=="minecraft:crossbow_pulling_1");assert(std::string(resource(true,25,25,""))=="minecraft:crossbow_pulling_2");assert(std::string(resource(true,0,25,"arrow"))=="minecraft:crossbow_arrow");assert(std::string(resource(false,0,25,"rocket"))=="minecraft:crossbow_firework");
 std::cout<<"Crossbow charge: official timing, quick-charge duration, mirroring, shake and transform parameters PASS\n";
}
