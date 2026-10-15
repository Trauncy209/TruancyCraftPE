#include "network/java/JavaHeartState.h"
#include <cassert>
#include <iostream>
int main(){
 using H=JavaHeartState;
 assert(H::kind(false,false,139)==H::Normal);assert(H::kind(false,false,140)==H::Frozen);
 assert(H::kind(false,true,140)==H::Withered);assert(H::kind(true,true,140)==H::Poisoned);
 H state;state.update(1,20,0,0,0);state.update(1,18,20,1,50);assert(state.display==20&&state.blinkUntil==21);assert(!state.blinking(1)&&state.blinking(6));
 state.update(1,19,10,7,350);assert(state.blinkUntil==17&&state.display==20);state.update(1,19,0,30,1351);assert(state.display==19&&!state.blinking(30));
 state.update(2,5,20,31,1400);assert(state.display==5&&!state.blinking(31));state.reset();assert(!state.initialized);state.update(2,12,0,32,1500);assert(state.display==12&&!state.blinking(32));
 // A health decrease without a server hurt/health-transition signal never flashes.
 state.update(2,10,0,33,1550);assert(state.blinkUntil==0);
 for(bool hc:{false,true})for(bool half:{false,true})for(bool blink:{false,true})assert(H::atlasX(hc,half,blink)>=0&&H::atlasX(hc,half,blink)+9<=72);
 std::cout<<"Official-derived heart priority,140tick freeze,damage/heal blink,1000ms catchup andworld/reset PASS\n";
}
