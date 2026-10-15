#include "client/renderer/ptexture/DynamicTexture.h"
#include "client/renderer/Textures.h"
#include "world/level/tile/Tile.h"
#include "world/level/material/Material.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include "world/level/biome/Biome.h"
#include "util/Mth.h"
#include <cassert>
#include <cstring>
#include <iostream>
struct ExistingAnimation:DynamicTexture {
 int ticks=0;ExistingAnimation():DynamicTexture(0){}
 void tick() override {++ticks;}
};
int main() {
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
 std::vector<unsigned char> strips[2];
 for(int layer=0;layer<2;++layer) {
  strips[layer].resize(16*512*4);
  for(int f=0;f<32;++f)for(int pixel=0;pixel<256;++pixel)for(int c=0;c<4;++c)
   strips[layer][f*1024+pixel*4+c]=unsigned((f*7+layer*23+pixel+c)%256);
 }
 Textures textures(nullptr,nullptr);
 FireTexture* fire[2];
 for(int layer=0;layer<2;++layer) {
  fire[layer]=new FireTexture(layer,strips[layer].data(),16,512);assert(fire[layer]->valid()&&fire[layer]->nativeWorldOnly());
  assert(fire[layer]->tex==31+layer*16);textures.addDynamicTexture(fire[layer]);
 }
 auto* existing=new ExistingAnimation();textures.addDynamicTexture(existing);assert(existing->ticks==1);
 for(unsigned tick=0;tick<96;++tick) {
  for(int layer=0;layer<2;++layer) {
   const int frame=FireTexture::frameAt(layer,tick);assert(frame>=0&&frame<(layer?32:16));
   assert(std::memcmp(fire[layer]->pixels,strips[layer].data()+frame*1024,1024)==0);
  }
  unsigned char frozen[1024];std::memcpy(frozen,fire[0]->pixels,1024);
  textures.tick(false,false);assert(std::memcmp(fire[0]->pixels,frozen,1024)==0);assert(existing->ticks==int(tick*2+2));
  textures.tick(false,true);assert(existing->ticks==int(tick*2+3));
 }
 const int existingBeforeCatchup=existing->ticks;
 textures.tick(false,true,true);assert(existing->ticks==existingBeforeCatchup);
 assert(std::memcmp(fire[0]->pixels,strips[0].data()+FireTexture::frameAt(0,97)*1024,1024)==0);
 FireTexture malformed(0,strips[0].data(),16,511);assert(!malformed.valid());malformed.tick();
 for(auto b:malformed.pixels)assert(b==0);
 assert(FireTexture::frameAt(0,0)==8&&FireTexture::frameAt(0,8)==0&&FireTexture::frameAt(0,16)==8);
 assert(FireTexture::frameAt(1,0)==0&&FireTexture::frameAt(1,31)==31&&FireTexture::frameAt(1,32)==0);
 std::cout<<"Native fire exact frame cycles/byte copies, atlas31+47, malformed strip and Java guard preserving existing animations PASS\n";
}
