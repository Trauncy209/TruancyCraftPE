#include "../src/world/level/WeatherLightning.h"
#include "../src/world/level/WeatherLightningGeometry.h"
#include <cassert>
#include <limits>
#include <iostream>
int main() {
    WeatherLightning bolt;bolt.id=1;bolt.seed=123;bolt.x=20;bolt.y=64;bolt.z=30;
    assert(bolt.valid());
    auto zero=[](int bound){assert(bound==10);return 0;};
    auto seed=[](){return uint64_t(456);};
    auto a=bolt.tick(true,zero,seed);assert(a.sound&&a.hitEntities&&a.illuminate&&bolt.life==1);
    auto b=bolt.tick(true,zero,seed);assert(!b.sound&&b.hitEntities&&bolt.life==0);
    auto c=bolt.tick(true,zero,seed);assert(c.retryFire&&c.hitEntities&&bolt.life==1&&bolt.flashes==0&&bolt.seed==456);
    assert(bolt.tick(true,zero,seed).illuminate);
    assert(bolt.tick(true,zero,seed).removed);assert(!bolt.visible());
    WeatherLightning slow;slow.id=2;
    auto nine=[](int){return 9;};
    for(int i=0;i<11;++i){slow.tick(true,nine,seed);assert(slow.flashes==1);}
    assert(slow.life==-9);assert(slow.tick(true,nine,seed).retryFire);
    WeatherLightning client;client.id=3;
    for(int i=0;i<8;++i)assert(!client.tick(false,zero,seed).hitEntities);
    assert(client.flashes==1 && client.seed==0); // Wait for authority's next flash.
    assert(!WeatherLightning::initialFireAllowed(true,1,true));
    assert(WeatherLightning::initialFireAllowed(true,2,true));
    assert(!WeatherLightning::initialFireAllowed(false,3,true));
    assert(!WeatherLightning::retryFireAllowed(true,false));
    assert(!WeatherLightning::fireAtAllowed(false,true,true));
    assert(bolt.withinHitBox(20,72.9,30,0.6,1.8));
    assert(!bolt.withinHitBox(24,64,30,0.6,1.8));
    bolt.x=std::numeric_limits<double>::quiet_NaN();assert(!bolt.valid());
    bolt.x=20;bolt.flashes=2;assert(!bolt.valid());
    auto geometry=[](uint64_t seed) {
        unsigned count=0;double checksum=0;
        WeatherLightningGeometry::generate(seed,[&](const WeatherLightningGeometry::Vertex& a,const WeatherLightningGeometry::Vertex& b,const WeatherLightningGeometry::Vertex& c,const WeatherLightningGeometry::Vertex& d) {
            ++count;for(auto v:{a,b,c,d}) {assert(std::isfinite(v.x)&&std::isfinite(v.z));assert(v.y>=0&&v.y<=128);checksum+=v.x*3+v.y*7+v.z*11;}
        });assert(count==224);return checksum;
    };
    assert(geometry(123)==geometry(123));assert(geometry(123)!=geometry(456));
    std::cout<<"weather lightning lifecycle and authority gates: PASS\n";
}
