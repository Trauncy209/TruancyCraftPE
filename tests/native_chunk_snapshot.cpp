#include "network/packet/ChunkDataPacket.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include "world/level/biome/Biome.h"
#include "util/Mth.h"
#include <array>
#include <vector>
#include <cassert>
#include <cstring>
#include <iostream>
static void decode(ChunkDataPacket& source,ChunkDataPacket& received) {
 RakNet::BitStream wire;source.write(&wire);unsigned char id=0;assert(wire.Read(id));assert(id==ID_USER_PACKET_ENUM+PACKET_CHUNKDATA);
 received.read(&wire);
}
struct Terrain {
 std::vector<unsigned char> ids=std::vector<unsigned char>(16*16*128,9);
 std::vector<unsigned char> metadata=std::vector<unsigned char>(16*16*128/2,0xee);
 int sections=0;
 void apply(ChunkDataPacket& packet,bool expectFull) {
  for(int z=0;z<16;++z)for(int x=0;x<16;++x) {
   unsigned char mask=0;assert(packet.chunkData.Read(mask));if(expectFull)assert(mask==255);
   for(int group=0;group<8;++group)if(mask&(1<<group)) {
    ++sections;const int offset=(x*16+z)*128+group*16;
    assert(packet.chunkData.Read((char*)ids.data()+offset,16));assert(packet.chunkData.Read((char*)metadata.data()+offset/2,8));
   }
  }
  assert(packet.chunkData.GetNumberOfUnreadBits()==0);
 }
};
int main() {
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
 auto* storage=new unsigned char[16*16*128]();LevelChunk chunk(nullptr,storage,-3,4);
 for(int x=0;x<16;++x)for(int z=0;z<16;++z)for(int y=0;y<128;++y) {
  const int index=(x*16+z)*128+y;
  storage[index]=(y>100 || (x+y+z)%7==0)?0:((x+y+z)%2?1:3);
  chunk.data.set(index,(x*3+y+z*5)&15);
 }
 const std::vector<unsigned char> expected(storage,storage+16*16*128),expectedMetadata(chunk.data.data,chunk.data.data+16*16*128/2);
 std::array<unsigned char,256> clean{};assert(!std::memcmp(chunk.updateMap,clean.data(),clean.size()));chunk.unsaved=false;
 ChunkDataPacket old(-3,4,&chunk);assert(!old.fullSnapshot);ChunkDataPacket sparseClean;decode(old,sparseClean);assert(sparseClean.chunkData.GetNumberOfBytesUsed()==256);Terrain untouched;untouched.apply(sparseClean,false);assert(untouched.sections==0&&untouched.ids[127]==9);
 ChunkDataPacket full(-3,4,&chunk,true);ChunkDataPacket first;decode(full,first);assert(first.x==-3&&first.z==4);assert(first.chunkData.GetNumberOfBytesUsed()==49408);
 Terrain replaced;replaced.apply(first,true);assert(replaced.sections==2048&&replaced.ids==expected&&replaced.metadata==expectedMetadata);
 // Clean, unmodified server terrain is still authoritative: client stone must
 // become server air at all elevations, including top/bottom section edges.
 assert(replaced.ids[127]==0&&replaced.ids[0]==0&&replaced.ids[(15*16+15)*128+127]==0);
 assert(!std::memcmp(chunk.updateMap,clean.data(),clean.size())&&!chunk.unsaved);
 for(int i=0;i<256;++i)chunk.updateMap[i]=(i%3==0)?0:((i%2)?0x81:0x08);
 const std::array<unsigned char,256> mixed=[&]{std::array<unsigned char,256> copy;std::memcpy(copy.data(),chunk.updateMap,256);return copy;}();
 ChunkDataPacket sparse(-3,4,&chunk);ChunkDataPacket delta;decode(sparse,delta);Terrain partial;partial.apply(delta,false);assert(partial.sections>0&&partial.sections<2048);assert(partial.ids[127]==9); // column0 is deliberately clean.
 ChunkDataPacket second;decode(full,second);Terrain again;again.apply(second,true);assert(again.ids==expected&&again.metadata==expectedMetadata);
 assert(!std::memcmp(chunk.updateMap,mixed.data(),mixed.size())&&!chunk.unsaved);
 assert(std::memcmp(storage,expected.data(),expected.size())==0&&std::memcmp(chunk.data.data,expectedMetadata.data(),expectedMetadata.size())==0);
 std::cout<<"Native full snapshot legacy-wire roundtrip: all2048sections, air replacement, metadata, negative coordinates, clean/mixed dirty maps, repeat writes and no server dirty mutation PASS\n";
 chunk.deleteBlockData();
}
