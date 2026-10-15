#include "JavaChunkStore.h"
#include <cassert>
#include <iostream>
#include <limits>
#include <algorithm>
int main(){
 JavaChunkStore store;std::vector<uint16_t> states(384*256);
 for(size_t y=0;y<384;y++)for(size_t z=0;z<16;z++)for(size_t x=0;x<16;x++)states[(x*16+z)*384+y]=static_cast<uint16_t>((y*53+z*17+x)%65536);
 TcpeJavaEvent event{};event.kind=TCPE_JAVA_CHUNK;event.x=-1;event.z=-2;event.min_y=-64;event.height=384;event.states=states.data();event.states_len=states.size();assert(store.apply(event));
 for(int y=-64;y<320;y++)for(int z=0;z<16;z++)for(int x=0;x<16;x++){uint16_t value=0;assert(store.state(x-16,y,z-32,value));assert(value==states[(x*16+z)*384+y+64]);}
 uint16_t value;assert(!store.state(-16,-65,-32,value));assert(!store.state(-16,320,-32,value));assert(!store.state(0,0,0,value));
 event.states_len--;assert(!store.apply(event));event.states_len++;event.min_y=std::numeric_limits<int>::max();assert(!store.apply(event));
 const auto previous=store;const auto revision=store.version();const auto original=store.chunk(-1,-2);
 event.min_y=-64;states[0]=65535;assert(store.apply(event));assert(store.version()>revision);assert(previous.chunk(-1,-2)==original);assert(store.chunk(-1,-2)!=original);assert(previous.state(-16,-64,-32,value)&&value==0);assert(store.state(-16,-64,-32,value)&&value==65535);
 // Java nibble ordering differs from the state-plane ordering above.
 std::vector<uint8_t> sky(384*128),block(384*128),flags(24,3);
 for(int y=0;y<384;++y)for(int z=0;z<16;++z)for(int x=0;x<16;++x){
  size_t index=(y*16+z)*16+x;const uint8_t sv=(y+x+z)%16,bv=(y*3+x*5+z*7)%16;
  sky[index/2]|=sv<<((index&1)*4);block[index/2]|=bv<<((index&1)*4);
 }
 TcpeJavaEvent lighting{};lighting.kind=TCPE_JAVA_LIGHT;lighting.x=-1;lighting.z=-2;lighting.min_y=-64;lighting.height=384;
 lighting.sky_light=sky.data();lighting.sky_light_len=sky.size();lighting.block_light=block.data();lighting.block_light_len=block.size();lighting.light_sections=flags.data();lighting.light_sections_len=flags.size();
 const auto unlit=store;assert(store.applyLight(lighting));
 for(int y=0;y<384;++y)for(int z=0;z<16;++z)for(int x=0;x<16;++x){uint8_t sv,bv;assert(store.light(true,x-16,y-64,z-32,sv)&&sv==(y+x+z)%16);assert(store.light(false,x-16,y-64,z-32,bv)&&bv==(y*3+x*5+z*7)%16);for(int darken: {0,7,15})assert(store.rawBrightness(x-16,y-64,z-32,darken)==std::max(int(bv),std::max(0,int(sv)-darken)));}
 uint8_t nibble;assert(!unlit.light(true,-16,-64,-32,nibble));assert(!store.light(true,-16,-65,-32,nibble));assert(!store.light(true,-16,320,-32,nibble));
 assert(store.rawBrightness(-16,-65,-32,0)==0);assert(store.rawBrightness(-16,320,-32,7)==8);
 const auto lit=store;const auto litRevision=store.version();
 assert(store.applyLight(lighting));assert(store.version()==litRevision);
 lighting.sky_light=nullptr;assert(!store.applyLight(lighting));lighting.sky_light=sky.data();
 lighting.sky_light_len--;assert(!store.applyLight(lighting));assert(store.version()==litRevision);lighting.sky_light_len++;
 flags[0]=4;assert(!store.applyLight(lighting));assert(store.version()==litRevision);flags[0]=3;
 // Empty sky mask supplies explicit zero to section 1, but omitted block mask preserves torch light.
 std::fill(flags.begin(),flags.end(),0);flags[1]=1;std::fill(sky.begin(),sky.end(),0);lighting.block_light=nullptr;lighting.block_light_len=0;
 assert(store.applyLight(lighting));assert(store.light(true,-15,-47,-32,nibble)&&nibble==0);assert(store.light(false,-15,-47,-32,nibble)&&nibble==(17*3+5)%16);
 assert(lit.light(true,-15,-47,-32,nibble)&&nibble==(17+1)%16);assert(store.light(true,-15,-63,-32,nibble)&&nibble==2);
 // A later state packet must not erase already delivered server lighting.
 event.min_y=-64;assert(store.apply(event));assert(store.light(true,-15,-47,-32,nibble)&&nibble==0);assert(store.light(false,-15,-47,-32,nibble)&&nibble==(17*3+5)%16);
 lighting.min_y=0;assert(!store.applyLight(lighting));
 store.unload(-1,-2);assert(previous.size()==1);assert(store.size()==0);std::cout<<"PASS full-height modern chunk retention and 98304 negative-coordinate state lookups, full-height nibble light channels, partial masks, sky darkening and immutable snapshots\n";
}
