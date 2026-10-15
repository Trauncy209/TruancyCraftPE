#include "world/level/WeatherState.h"
#include <cassert>
#include <cmath>
#include <vector>
#include <limits>
int main(){
 WeatherState command;command.rain=.4f;assert(command.command("thunder",200));assert(command.raining&&command.thundering&&command.rain==.4f&&command.rainTicks==200);assert(!command.command("invalid",200));assert(!command.command("clear",0));assert(command.raining);assert(command.command("clear",100));assert(!command.raining&&!command.thundering&&command.rain==.4f);
 command.clearAfterSleep();assert(command.rainTicks==0&&command.thunderTicks==0&&!command.raining&&!command.thundering&&command.rain==.4f);
 WeatherState w;std::vector<int> bounds;auto low=[&](int bound){bounds.push_back(bound);return 0;};
 w.tick(true,false,low);assert(bounds==std::vector<int>({168000,168000}));assert(w.rainTicks==12000&&w.thunderTicks==12000);assert(!w.raining&&!w.thundering);
 w.rainTicks=w.thunderTicks=1;w.tick(true,false,low);assert(w.raining&&w.thundering&&w.rainTicks==0&&w.thunderTicks==0);assert(std::abs(w.rain-.01f)<1e-6f);
 bounds.clear();w.tick(true,false,low);assert(bounds==std::vector<int>({12000,12000}));assert(w.thunderTicks==3600&&w.rainTicks==12000);
 w.rainTicks=w.thunderTicks=0;auto high=[](int n){return n-1;};w.tick(true,false,high);assert(w.rainTicks==23999&&w.thunderTicks==15599);
 w.raining=w.thundering=false;w.rainTicks=w.thunderTicks=0;w.tick(true,false,high);assert(w.rainTicks==179999&&w.thunderTicks==179999);
 w.raining=w.thundering=true;w.rain=w.thunder=0;w.rainTicks=222;w.thunderTicks=333;
 for(int i=0;i<101;++i)w.tick(false,false,low);assert(w.rain==1&&w.thunder==1&&w.rainTicks==222&&w.thunderTicks==333);assert(w.wet()&&w.storm());
 w.previousRain=.2f;w.rain=.4f;w.previousThunder=.4f;w.thunder=.6f;assert(std::abs(w.rainAt(.5f)-.3f)<1e-6f);assert(std::abs(w.thunderAt(.5f)-.15f)<1e-6f);
 WeatherState old=w;w.tick(true,true,low);assert(w.rain==old.rain&&w.rainTicks==old.rainTicks&&w.previousRain==old.previousRain);
 w.raining=w.thundering=false;for(int i=0;i<101;++i)w.tick(false,false,low);assert(w.rain==0&&w.thunder==0&&!w.wet()&&!w.storm());
 w.raining=true;w.thundering=false;w.restoreIntensity();assert(w.rainAt(0)==1&&w.thunderAt(1)==0);
 assert(WeatherState::bounded(std::numeric_limits<float>::quiet_NaN())==0);
}
