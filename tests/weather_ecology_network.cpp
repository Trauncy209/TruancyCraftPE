#include "world/level/WeatherEcology.h"
#include "world/level/Level.h"
#include "world/level/LightLayer.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/biome/Biome.h"
#include "world/level/biome/BiomeSource.h"
#include "world/level/dimension/Dimension.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include <cassert>
#include <iostream>
#include "client/Minecraft.h"
#include "network/RakNetInstance.h"
#include "network/ServerSideNetworkHandler.h"
#include "network/packet/PacketInclude.h"
#include "SharedConstants.h"
#include <chrono>
#include <thread>
#include <functional>
#include <map>
struct GameProbe:Minecraft{GameProbe():Minecraft(){}};
struct EcologyClient:NetEventCallback {
 RakNetInstance peer;bool started=false,failed=false,chunkReceived=false;int snow=0,ice=0;std::string name;
 std::map<int,int> columnY100;
 EcologyClient(const char* n):name(n){}
 void onConnect(const RakNet::RakNetGUID&) override{LoginPacket p(name.c_str(),SharedConstants::NetworkProtocolVersion);peer.send(p);}
 void onUnableToConnect() override{failed=true;}
 void handle(const RakNet::RakNetGUID&,LoginStatusPacket* p) override{assert(p->status==0);peer.setIsLoggedIn(true);}
 void handle(const RakNet::RakNetGUID&,StartGamePacket*) override{started=true;ReadyPacket r(ReadyPacket::READY_CLIENTGENERATION);peer.send(r);}
 void handle(const RakNet::RakNetGUID&,UpdateBlockPacket* p) override{if(p->x>=32&&p->x<48&&p->z>=32&&p->z<48){if(p->blockId==Tile::topSnow->id)++snow;if(p->blockId==Tile::ice->id)++ice;}}
 void handle(const RakNet::RakNetGUID&,ChunkDataPacket* p) override{
  if(p->x!=2||p->z!=2)return;
  for(int i=0;i<256;++i){unsigned char mask;assert(p->chunkData.Read(mask));for(int section=0;section<8;++section)if(mask&(1<<section)){char blocks[16],data[8];assert(p->chunkData.Read(blocks,16)&&p->chunkData.Read(data,8));if(section==6)columnY100[i]=(unsigned char)blocks[4];}}
  chunkReceived=true;
 }
};
struct Climate:BiomeSource {
 Biome* selected=Biome::tundra;
 Biome* getBiome(int,int) override{return selected;}
};
int main() {
 std::cout.setf(std::ios::unitbuf);
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
 auto* level=new Level(new MemoryLevelStorage(),"ecology",LevelSettings(444,GameType::Survival,LGV_ORIGINAL),LGV_ORIGINAL);
 level->getChunk(2,2);
 auto* game=new GameProbe();game->setIsCreativeMode(false);game->level=level;
 RakNetInstance server;int port=29837;assert(server.host("Ecology fixture",port,4));game->raknetInstance=&server;
 ServerSideNetworkHandler handler(game,&server);handler.levelGenerated(level);
 EcologyClient first("EcologyOne"),second("EcologyTwo"),late("EcologyLate");
 auto pump=[&]{server.runEvents(&handler);first.peer.runEvents(&first);second.peer.runEvents(&second);late.peer.runEvents(&late);std::this_thread::sleep_for(std::chrono::milliseconds(3));};
 auto until=[&](std::function<bool()> done){auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(8);while(!done()&&std::chrono::steady_clock::now()<deadline)pump();if(!done())std::cout<<"Timeout state: "<<first.started<<second.started<<" players="<<level->players.size()<<" snow="<<first.snow<<","<<second.snow<<" ice="<<first.ice<<","<<second.ice<<" late="<<late.started<<" chunk="<<late.chunkReceived<<"\n";assert(done());};
 assert(first.peer.connect("127.0.0.1",port)&&second.peer.connect("127.0.0.1",port));until([&]{return first.started&&second.started&&level->players.size()==2;});
 auto* climate=new Climate();level->dimension->biomeSource=climate;
 const int x=40,z=40,y=100;
 for(int yy=y-1;yy<128;++yy)for(int dx=-1;dx<=1;++dx)for(int dz=-1;dz<=1;++dz)
  level->setTileNoUpdate(x+dx,yy,z+dz,0);
 level->setTileNoUpdate(x,y,z,Tile::calmWater->id);level->setBrightness(LightLayer::Block,x,y,z,9);
 assert(WeatherEcology::shouldFreeze(*level,x,y,z));
 level->setData(x,y,z,1);assert(!WeatherEcology::shouldFreeze(*level,x,y,z));level->setData(x,y,z,0);
 level->setBrightness(LightLayer::Block,x,y,z,10);assert(!WeatherEcology::shouldFreeze(*level,x,y,z));
 level->setBrightness(LightLayer::Block,x,y,z,0);
 for(auto d:{std::pair<int,int>{-1,0},{1,0},{0,-1},{0,1}})level->setTileNoUpdate(x+d.first,y,z+d.second,Tile::calmWater->id);
 assert(!WeatherEcology::shouldFreeze(*level,x,y,z));assert(WeatherEcology::shouldFreeze(*level,x,y,z,false));
 climate->selected=Biome::plains;assert(!WeatherEcology::shouldFreeze(*level,x,y,z,false));climate->selected=Biome::tundra;
 level->setTileNoUpdate(x,y,z,Tile::rock->id);level->setBrightness(LightLayer::Block,x,y+1,z,0);
 assert(WeatherEcology::shouldSnow(*level,x,y+1,z));
 level->setTileNoUpdate(x,y,z,Tile::ice->id);assert(!WeatherEcology::shouldSnow(*level,x,y+1,z));
 level->setTileNoUpdate(x,y,z,Tile::rock->id);level->setBrightness(LightLayer::Block,x,y+1,z,10);assert(!WeatherEcology::shouldSnow(*level,x,y+1,z));
 level->setBrightness(LightLayer::Block,x,y+1,z,0);climate->selected=Biome::plains;
 level->getLevelData()->weather.rain=1;assert(WeatherEcology::exposedToRain(*level,x,y+1,z));
 level->setTileNoUpdate(x,y+3,z,Tile::rock->id);assert(!WeatherEcology::exposedToRain(*level,x,y+1,z));
 level->setTileNoUpdate(x,y+3,z,0);climate->selected=Biome::desert;assert(!WeatherEcology::exposedToRain(*level,x,y+1,z));
 climate->selected=Biome::tundra;assert(!WeatherEcology::exposedToRain(*level,x,y+1,z));
 assert(!WeatherEcology::shouldFreeze(*level,256,y,z));assert(!WeatherEcology::shouldSnow(*level,256,y,z));
 // An actual authoritative chunk tick changes terrain, while client and
 // ceiling dimension ticks leave the same prepared terrain untouched.
 climate->selected=Biome::taiga;level->getLevelData()->weather.rain=1;
 for(int xx=32;xx<48;++xx)for(int zz=32;zz<48;++zz){
  level->setTileNoUpdate(xx,y,zz,Tile::rock->id);
  for(int yy=y+1;yy<128;++yy)level->setTileNoUpdate(xx,yy,zz,0);
  level->setBrightness(LightLayer::Block,xx,y+1,zz,0);
 }
 level->random.setSeed(1234);level->isClientSide=true;
 for(int i=0;i<512;++i)WeatherEcology::tickChunk(*level,2,2);
 for(int xx=32;xx<48;++xx)for(int zz=32;zz<48;++zz)assert(level->getTile(xx,y+1,zz)==0);
 level->isClientSide=false;level->dimension->hasCeiling=true;
 for(int i=0;i<512;++i)WeatherEcology::tickChunk(*level,2,2);
 for(int xx=32;xx<48;++xx)for(int zz=32;zz<48;++zz)assert(level->getTile(xx,y+1,zz)==0);
 level->dimension->hasCeiling=false;
 for(int i=0;i<512;++i)WeatherEcology::tickChunk(*level,2,2);
 int accumulated=0;for(int xx=32;xx<48;++xx)for(int zz=32;zz<48;++zz)if(level->getTile(xx,y+1,zz)==Tile::topSnow->id)++accumulated;
 assert(accumulated>0 && accumulated<128);
 assert(!level->hasChunkNow(1,2)&&!level->hasChunkNow(3,2)&&!level->hasChunkNow(2,1)&&!level->hasChunkNow(2,3));
 std::cout<<"Authoritative snow chunk ticks changed "<<accumulated<<" columns; client/ceiling chunks unchanged PASS\n";
 // The reference allows freezing in cold clear weather, without snow.
 level->getLevelData()->weather.rain=0;
 for(int xx=32;xx<48;++xx)for(int zz=32;zz<48;++zz){
  level->setTileNoUpdate(xx,y+1,zz,0);
  level->setTileNoUpdate(xx,y,zz,xx==40?Tile::rock->id:Tile::calmWater->id);
  level->setDataNoUpdate(xx,y,zz,0);level->setBrightness(LightLayer::Block,xx,y,zz,0);
 }
 level->random.setSeed(9876);
 for(int i=0;i<2048;++i)WeatherEcology::tickChunk(*level,2,2);
 int frozen=0;for(int xx=32;xx<48;++xx)for(int zz=32;zz<48;++zz){
  if(level->getTile(xx,y,zz)==Tile::ice->id)++frozen;
  assert(level->getTile(xx,y+1,zz)==0);
 }
 assert(frozen>0);assert(!level->hasChunkNow(1,2)&&!level->hasChunkNow(3,2)&&!level->hasChunkNow(2,1)&&!level->hasChunkNow(2,3));
 std::cout<<"Cold clear chunk ticks froze "<<frozen<<" source-water columns without snow or loading neighbors PASS\n";
 std::cout<<"Weather ecology: actual level source-water, edge/light/biome, snow support, roof/dry/cold exclusion, finite bounds PASS\n";
 until([&]{return first.snow>=accumulated&&second.snow>=accumulated&&first.ice>=frozen&&second.ice>=frozen;});
 assert(first.snow==second.snow&&first.ice==second.ice);
 assert(late.peer.connect("127.0.0.1",port));until([&]{return late.started&&level->players.size()==3;});
 RequestChunkPacket chunk(2,2);late.peer.send(chunk);until([&]{return late.chunkReceived;});
 int recoveredIce=0;for(auto block:late.columnY100){int xx=32+(block.first%16),zz=32+(block.first/16);assert(block.second==level->getTile(xx,100,zz));if(block.second==Tile::ice->id)++recoveredIce;}
 assert(recoveredIce==frozen);
 std::cout<<"Real native networking: both clients received snow/ice updates; late-join chunk matched all256columns and "<<recoveredIce<<" frozen columns PASS\n";
 first.peer.disconnect();second.peer.disconnect();late.peer.disconnect();

}
