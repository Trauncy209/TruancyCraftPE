#include "../original-tcpe/src/network/java/JavaBossState.h"
#include <cassert>
#include <iostream>
#include <limits>
int main(){JavaBossState s;JavaBossBar a;a.id="a";a.text="First";a.target=1;s.add(a,1000);a.id="b";s.add(a,1000);auto* b=s.find("a");b->retarget(0,1000);assert(b->progress(1050)==.5);b->retarget(1,1050);assert(b->from==.5&&b->progress(1100)==.75&&b->progress(1150)==1);assert(b->progress(1000)==.5);a.id="a";a.text="Replacement";a.target=.2;s.add(a,1200);assert(s.bars.size()==2&&s.bars[0].id=="a"&&s.bars[1].id=="b");assert(s.find("a")->progress(1200)==.2);assert(!s.find("unknown"));s.remove("unknown");assert(s.bars.size()==2);s.remove("a");assert(s.bars.size()==1&&s.bars[0].id=="b");assert(JavaBossBar::bounded(std::numeric_limits<double>::quiet_NaN())==0);assert(JavaBossBar::bounded(2)==1);s.clear();assert(s.bars.empty());std::cout<<"Boss state identity/order, replacement, removal, 100ms interpolation, retarget continuity and reset PASS\n";}
