#include "JavaDroppedCompass.h"
#include <cassert>
#include <iostream>
#include <set>
int main(int argc,char**){
 if(argc>1){for(int n=-4096;n<=4096;++n)std::cout<<JavaCompassFrames::frame(double(n)/2048.)<<"\n";return 0;}
 JavaGameSession::CompassState compass;compass.dimension="minecraft:overworld";compass.defaultSpawn.valid=true;compass.defaultSpawn.dimension=compass.dimension;
 JavaGameSession::Item item;item.resourceId="minecraft:compass";JavaDroppedCompass drop;
 // bob=-0.025 exactly cancels age+0.5 at age0, yielding official visualYaw180.
 // The vectors deliberately avoid block-centre ambiguity.
 for(auto target:std::initializer_list<std::array<int,3>>{{100,64,0},{0,64,100},{-100,64,0},{0,64,-100}}){compass.defaultSpawn.position=target;std::cout<<drop.resource(compass,item,.5,.5,0,-.025f,1,7)<<"\n";}
 compass.defaultSpawn.position={{100,64,0}};assert(drop.resource(compass,item,.5,.5,0,-.025f,1,7)=="minecraft:compass_24");compass.defaultSpawn.position={{0,64,100}};assert(drop.resource(compass,item,.5,.5,0,-.025f,1,7)=="minecraft:compass_00");compass.defaultSpawn.position={{-100,64,0}};assert(drop.resource(compass,item,.5,.5,0,-.025f,1,7)=="minecraft:compass_08");compass.defaultSpawn.position={{0,64,-100}};assert(drop.resource(compass,item,.5,.5,0,-.025f,1,7)=="minecraft:compass_16");
 item.hasLodestoneTracker=true;item.lodestoneTarget=compass.defaultSpawn;item.lodestoneTarget.position={{100,64,0}};assert(drop.resource(compass,item,.5,.5,0,-.025f,1,7)=="minecraft:compass_24");item.lodestoneTracked=false;assert(drop.resource(compass,item,.5,.5,0,-.025f,1,7)=="minecraft:compass_24");
 item.resourceId="minecraft:recovery_compass";compass.lastDeath=compass.defaultSpawn;assert(drop.resource(compass,item,.5,.5,0,-.025f,1,7)=="minecraft:recovery_compass_16");
 compass.lastDeath.dimension="minecraft:the_nether";std::set<std::string> spinning;for(unsigned t=1;t<100;++t){auto frame=drop.resource(compass,item,.5,.5,0,-.025f,t,7);assert(frame==drop.resource(compass,item,.5,.5,0,-.025f,t,7));spinning.insert(frame);}assert(spinning.size()>3);
 compass.lastDeath.valid=false;auto invalid=drop.resource(compass,item,.5,.5,0,-.025f,100,7);compass.lastDeath=compass.defaultSpawn;compass.lastDeath.position={{100,64,0}};assert(drop.resource(compass,item,.5,.5,0,-.025f,100,7)=="minecraft:recovery_compass_24");assert(drop.resource(compass,item,.5,.5,20,-.025f,100,7)!="minecraft:recovery_compass_24");
 // Exact official range-dispatch thresholds, including wrap and both sides.
 const int mapped[]={16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
 assert(JavaCompassFrames::frame(0)==mapped[0]);for(int n=1;n<=32;++n){double threshold=(n-.5)/32.;assert(JavaCompassFrames::frame(threshold-1e-8)==mapped[n-1]);assert(JavaCompassFrames::frame(threshold)==mapped[n]);assert(JavaCompassFrames::frame(threshold+1e-8)==mapped[n]);}assert(JavaCompassFrames::frame(1)==16);assert(JavaCompassFrames::frame(-1)==16);
 std::cout<<"CPU dropped compass official cardinal/spin, lodestone(false tracked), recovery, invalid-dimension stable tick/spin, exact33threshold mapping PASS\n";
}
