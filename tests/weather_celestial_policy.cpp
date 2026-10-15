#include "../src/client/renderer/WeatherCelestialPolicy.h"
#include <cassert>
#include <cmath>
int main() {
 using namespace WeatherCelestialPolicy;
 assert(visibility(0)==1&&visibility(1)==0&&visibility(.5f)==.5f);
 assert(visibility(2)==0&&visibility(-1)==1);
 auto clear=stars(.4f,0);assert(clear.brightness==.4f&&clear.alpha==1);
 auto rain=stars(.4f,.5f);assert(rain.brightness==.2f&&rain.alpha==.5f);
 assert(std::abs(rain.brightness*rain.alpha/(clear.brightness*clear.alpha)-.25f)<1e-6f);
 auto storm=stars(.4f,1);assert(storm.brightness==0&&storm.alpha==0);
 auto java=stars(.4f,1,true);assert(java.brightness==.4f&&java.alpha==1);
}
