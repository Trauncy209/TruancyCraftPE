#include "network/java/JavaItemIcons.h"
#include "JavaCompassFrames.h"
#include <cassert>
#include <iostream>
int main(){JavaGameSession session;session.compass.dimension="minecraft:overworld";session.compass.defaultSpawn.valid=true;session.compass.defaultSpawn.dimension=session.compass.dimension;session.compass.defaultSpawn.position={{100,64,0}};JavaGameSession::Item item;item.resourceId="minecraft:compass";
 auto settled=[&](double yaw){std::string frame;for(unsigned long long t=1;t<1000;++t)frame=JavaItemIcons::animatedResource(session,item,.5,.5,yaw,t);return frame;};
 const char* expected[]={"minecraft:compass_08","minecraft:compass_00","minecraft:compass_24","minecraft:compass_16"};for(int i=0;i<4;++i){auto got=settled(i*90);std::cout<<"held yaw"<<i*90<<" -> "<<got<<" expected "<<expected[i]<<std::endl;assert(got==expected[i]);}
 for(int n=1;n<=32;++n){double threshold=(n-.5)/32.;auto lower=settled(270.-(threshold-1e-5)*360.),upper=settled(270.-(threshold+1e-5)*360.);assert(lower==JavaCompassFrames::resource(threshold-1e-5,false));assert(upper==JavaCompassFrames::resource(threshold+1e-5,false));}
 item.resourceId="minecraft:recovery_compass";session.compass.lastDeath=session.compass.defaultSpawn;assert(settled(0)=="minecraft:recovery_compass_08");item.resourceId="minecraft:compass";item.hasLodestoneTracker=true;item.lodestoneTarget=session.compass.defaultSpawn;item.lodestoneTarget.position={{-100,64,0}};assert(settled(0)=="minecraft:compass_24");std::cout<<"Actual JavaItemIcons animatedResource:4 cardinal headings,64 near-thresholds,lodestone/recovery PASS (CPU only)"<<std::endl;}
