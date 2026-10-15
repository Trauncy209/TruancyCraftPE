#include "network/packet/ChunkDataPacket.h"
#include "network/packet/RequestChunkPacket.h"
#include "network/ServerSideNetworkHandler.h"
#include "network/RakNetInstance.h"
#include "client/Minecraft.h"
#include "world/level/Level.h"
#include "world/level/storage/MemoryLevelStorage.h"
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
struct Game: Minecraft {Game():Minecraft(){}};
#include "world/level/storage/ExternalFileLevelStorage.h"
#include "world/level/chunk/ChunkSource.h"
int main(int argc,char**argv){assert(argc==2);Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
LevelSettings settings(1792046386,GameType::Survival,LGV_INFINITE,true,true,true,false,true,false,true,true,false,true);
auto* disk=new ExternalFileLevelStorage("population",argv[1]);auto* level=new Level(disk,"population",settings,LGV_INFINITE);level->setUpdateLights(false);
for(int x=-67;x<=-65;++x)for(int z=0;z<=2;++z)level->getChunk(x,z);auto* target=level->getChunk(-66,1);assert(target->terrainPopulated&&target->unsaved);for(int i=0;i<256;++i)assert(target->updateMap[i]==0);std::vector<unsigned char> ids(target->getBlockData(),target->getBlockData()+32768),meta(target->data.data,target->data.data+16384);level->saveGame();level->getChunkSource()->saveAll(true);
auto* reopened=new ExternalFileLevelStorage("population",argv[1]);auto* fresh=new Level(reopened,"population",settings,LGV_INFINITE);fresh->setUpdateLights(false);auto* loaded=reopened->load(fresh,-66,1);assert(loaded&&loaded->terrainPopulated);assert(!memcmp(ids.data(),loaded->getBlockData(),32768));assert(!memcmp(meta.data(),loaded->data.data,16384));std::cout<<"New populated terrain saved by saveAll(true), reload exact32768IDs+16384metadata, clean network map PASS"<<std::endl;}
