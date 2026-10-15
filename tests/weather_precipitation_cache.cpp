#include "../src/client/renderer/WeatherPrecipitationCache.h"
#include <cassert>
#include <cstring>
#include <climits>
#include <iostream>
static bool same(float a,float b){return std::memcmp(&a,&b,sizeof(a))==0;}
static void equivalent(const WeatherPrecipitationPolicy::Column& a,const WeatherPrecipitationPolicy::Column& b){
 assert(a.visible==b.visible&&a.snowy==b.snowy&&a.bottom==b.bottom&&a.top==b.top&&a.lightY==b.lightY);
 assert(same(a.sideX,b.sideX)&&same(a.sideZ,b.sideZ)&&same(a.lowerAlpha,b.lowerAlpha)&&same(a.upperAlpha,b.upperAlpha));
 assert(same(a.u0,b.u0)&&same(a.u1,b.u1)&&same(a.v0,b.v0)&&same(a.v1,b.v1));
}
int main(){
 using namespace WeatherPrecipitationPolicy;
 WeatherPrecipitationCache cache;
 assert(cache.getForSnow(12,34,.15f,60)==nullptr);
 assert(cache.getForSnow(12,34,0.f,60)!=nullptr);
 for(int cx:{-400000,-33,-1,0,31,32,400000})for(int cz:{-33,-1,0,31,32})
 for(int x=cx-9;x<=cx+9;++x)for(int z=cz-9;z<=cz+9;++z)
 for(float temp:{0.f,.15f,.8f})for(int tick:{0,31,32,511,512,999999,INT_MAX,INT_MIN}){
  const auto& n=cache.get(x,z);
  equivalent(sample(x,z,60,temp,true,cx+.25,64.8,cz+.75,64.2,tick,.5f,.8f),
             sample(x,z,60,temp,true,cx+.25,64.8,cz+.75,64.2,tick,.5f,.8f,1,&n));
 }
 for(int x:{INT_MIN,-33,-1,0,31,32,INT_MAX})for(int z:{INT_MIN,-1,0,INT_MAX}){
  auto n=cache.get(x,z),expected=noise(x,z);
  assert(same(n.first,expected.first)&&same(n.gaussianU,expected.gaussianU)&&same(n.baseV,expected.baseV)&&same(n.gaussianV,expected.gaussianV)&&n.phase==expected.phase);
 }
 auto original=cache.get(-1,-1);
 auto collision=cache.get(31,31);(void)collision;
 auto restored=cache.get(-1,-1);
 assert(same(original.gaussianU,restored.gaussianU));
 cache.clear();auto reset=cache.get(-1,-1);assert(same(original.first,reset.first));
 static_assert(sizeof(WeatherPrecipitationCache)<=32768,"Noise cache exceeded phone memory budget");
 std::cout<<"Bitwise cached/uncached rain and snow samples, negative/wrap/eviction/time/temperature PASS; cache bytes="<<sizeof(cache)<<"\n";
}
