#include "../src/client/renderer/WeatherEffects.h"
#include <cassert>
#include <limits>
int main() {
    using namespace WeatherImpactPolicy;
    assert(attempts(0, true) == 0);
    assert(attempts(1, true) == 25);
    assert(attempts(.5f, true) == 6);
    assert(attempts(1, false) == 6);
    assert(attempts(1, true, 1) == 12);
    assert(attempts(1, true, 2) == 0);
    assert(attempts(std::numeric_limits<float>::quiet_NaN(), true) == 0);
    assert(attempts(1,true,0,2)==11);
    assert(attempts(1,true,0,4)==4);
    assert(lifetime(0)==40&&lifetime(1)==8&&lifetime(.5)==13);
    assert(impactHeight(65,.5f)>64.59f&&impactHeight(65,.5f)<64.61f);
    assert(impactHeight(65,1.f)>65.09f&&impactHeight(65,1.f)<65.11f);
    assert(submerged(64.5f,64,0,true));
    assert(!submerged(64.95f,64,0,true));
    assert(!submerged(64.5f,64,0,false));
    assert(submerged(64.5f,64,8,true));
    Random48 seeded(0);assert(seeded.nextInt(10)==0);
    assert(seeded.nextInt(10)==8);
    assert(tickSeed(0)==0&&tickSeed(1)==312987231u);
    assert(tickSeed(7)==UINT64_C(18446744071605494937));
    assert(!eligible(false, true, 64, 64));
    assert(!eligible(true, false, 64, 64));
    assert(eligible(true, true, 74, 64));
    assert(eligible(true, true, 54, 64));
    assert(!eligible(true, true, 75, 64));
    assert(!eligible(true, true, 53, 64));
    assert(sheltered(70, 64, 70, 64));
    assert(!sheltered(65, 64, 70, 64));
    assert(!sheltered(70, 64, 64, 64));
}
