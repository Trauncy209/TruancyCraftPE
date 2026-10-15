#ifndef TCPE_JAVA_BOSS_STATE_H
#define TCPE_JAVA_BOSS_STATE_H
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <chrono>
struct JavaBossBar {
 std::string id,text;int color=0,overlay=0;bool darkenScreen=false,playMusic=false,createWorldFog=false;
 double from=0,target=0,changed=0;
 static double bounded(double p){return std::isfinite(p)?std::max(0.,std::min(1.,p)):0.;}
 double progress(double now)const{return from+(target-from)*bounded((now-changed)/100.);}
 void retarget(double value,double now){from=progress(now);target=bounded(value);changed=now;}
};
struct JavaBossState {
 std::vector<JavaBossBar> bars;
 static double now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
 JavaBossBar* find(const std::string& id){for(auto& b:bars)if(b.id==id)return &b;return nullptr;}
 void add(JavaBossBar bar,double time){bar.from=bar.target=JavaBossBar::bounded(bar.target);bar.changed=time;if(auto* old=find(bar.id))*old=bar;else bars.push_back(bar);}
 void remove(const std::string& id){bars.erase(std::remove_if(bars.begin(),bars.end(),[&](const JavaBossBar& b){return b.id==id;}),bars.end());}
 void clear(){bars.clear();}
};
#endif
