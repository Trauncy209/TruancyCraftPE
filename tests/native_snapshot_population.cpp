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
int main(){Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
auto* level=new Level(new MemoryLevelStorage(),"population-stability",LevelSettings(1792046386,GameType::Survival,LGV_INFINITE,true,true,true,false,true,false,true,true,false,true),LGV_INFINITE);level->setUpdateLights(false);auto* game=new Game();game->level=level;RakNetInstance wire;game->raknetInstance=&wire;ServerSideNetworkHandler handler(game,&wire);handler.levelGenerated(level);
for(auto pos:{std::pair<int,int>{-66,1},{-2,1},{5,-7}}){RequestChunkPacket request(pos.first,pos.second);handler.handle(RakNet::UNASSIGNED_RAKNET_GUID,&request);auto* target=level->getChunk(pos.first,pos.second);assert(target->terrainPopulated);std::vector<unsigned char> before(target->getBlockData(),target->getBlockData()+32768);std::vector<unsigned char> metadata(target->data.data,target->data.data+16384);
for(int ring=1;ring<=4;++ring)for(int dx=-ring;dx<=ring;++dx)for(int dz=-ring;dz<=ring;++dz)if(abs(dx)==ring||abs(dz)==ring){RequestChunkPacket neighbor(pos.first+dx,pos.second+dz);handler.handle(RakNet::UNASSIGNED_RAKNET_GUID,&neighbor);}
target=level->getChunk(pos.first,pos.second);assert(!memcmp(before.data(),target->getBlockData(),32768));assert(!memcmp(metadata.data(),target->data.data,16384));std::cout<<"Snapshot population stable after outward rings4 center="<<pos.first<<","<<pos.second<<" PASS"<<std::endl;}
}
