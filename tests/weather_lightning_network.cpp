#include "client/Minecraft.h"
#include "network/RakNetInstance.h"
#include "network/ServerSideNetworkHandler.h"
#include "network/packet/PacketInclude.h"
#include "world/level/Level.h"
#include "world/level/WeatherLightningRuntime.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include "SharedConstants.h"
#include <cassert>
#include <chrono>
#include <thread>
#include <functional>
#include <vector>
#include <iostream>
struct GameProbe:Minecraft{GameProbe():Minecraft(){}};
struct Client:NetEventCallback{
 RakNetInstance peer;std::string name;bool started=false,failed=false;std::vector<WeatherLightningPacket> events;WeatherState last;
 explicit Client(const char* n):name(n){}
 void onConnect(const RakNet::RakNetGUID&)override{LoginPacket p(name.c_str(),SharedConstants::NetworkProtocolVersion);peer.send(p);}
 void onUnableToConnect()override{failed=true;}
 void handle(const RakNet::RakNetGUID&,LoginStatusPacket* p)override{assert(p->status==0);peer.setIsLoggedIn(true);}
 void handle(const RakNet::RakNetGUID&,StartGamePacket*)override{started=true;ReadyPacket p(ReadyPacket::READY_CLIENTGENERATION);peer.send(p);}
 void handle(const RakNet::RakNetGUID&,WeatherPacket* p)override{assert(p->valid);last=p->state;}
 void handle(const RakNet::RakNetGUID&,WeatherLightningPacket* p)override{assert(p->valid);events.push_back(*p);}
};
int main(){
 std::cout.setf(std::ios::unitbuf);Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
 auto* level=new Level(new MemoryLevelStorage(),"lightning-net",LevelSettings(444,GameType::Survival,LGV_ORIGINAL),LGV_ORIGINAL);level->setUpdateLights(false);
 for(int z=6;z<=10;++z)for(int x=6;x<=10;++x)level->getChunk(x,z);
 auto* game=new GameProbe();game->setIsCreativeMode(false);game->level=level;
 RakNetInstance server;const int port=29838;assert(server.host("Lightning test",port,4));game->raknetInstance=&server;level->raknetInstance=&server;
 ServerSideNetworkHandler handler(game,&server);handler.levelGenerated(level);
 Client first("BoltOne"),second("BoltTwo"),late("BoltLate");
 auto pump=[&]{server.runEvents(&handler);first.peer.runEvents(&first);second.peer.runEvents(&second);late.peer.runEvents(&late);assert(!first.failed&&!second.failed&&!late.failed);std::this_thread::sleep_for(std::chrono::milliseconds(3));};
 auto until=[&](std::function<bool()> done){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(8);while(!done()&&std::chrono::steady_clock::now()<end)pump();assert(done());};
 auto hold=[&](int ms){auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(ms);while(std::chrono::steady_clock::now()<end)pump();};
 assert(first.peer.connect("127.0.0.1",port)&&second.peer.connect("127.0.0.1",port));until([&]{return first.started&&second.started&&level->players.size()==2;});
 auto& state=level->getLevelData()->weather;assert(state.command("thunder",24000));state.restoreIntensity();
 assert(WeatherLightningRuntime::spawn(*level,128.5,100,128.5));assert(level->weatherLightning.size()==1);
 const uint64_t id=level->weatherLightning.front().id;until([&]{return first.events.size()==1&&second.events.size()==1;});
 WeatherState malicious;assert(malicious.command("clear",20));malicious.restoreIntensity();WeatherPacket forgedWeather(malicious);first.peer.send(forgedWeather);
 WeatherLightning forged;forged.id=UINT64_C(0xDEADBEEF);forged.seed=123;forged.x=200;forged.y=100;forged.z=200;WeatherLightningPacket forgedBolt(forged,0);assert(forgedBolt.valid);first.peer.send(forgedBolt);
 hold(300);assert(state.raining&&state.thundering&&state.rainTicks==24000&&state.thunderTicks==24000);
 assert(level->weatherLightning.size()==1&&level->weatherLightning.front().id==id&&first.events.size()==1&&second.events.size()==1);
 for(int i=0;i<100&&!level->weatherLightning.empty();++i){WeatherLightningRuntime::tick(*level);hold(6);}
 assert(level->weatherLightning.empty());until([&]{return first.events.size()==3&&second.events.size()==3;});
 for(size_t i=0;i<3;++i){const auto& a=first.events[i];const auto& b=second.events[i];assert(a.phase==i&&b.phase==i&&a.bolt.id==id&&b.bolt.id==id);assert(a.bolt.seed==b.bolt.seed&&a.bolt.x==b.bolt.x&&a.bolt.y==b.bolt.y&&a.bolt.z==b.bolt.z&&a.bolt.life==b.bolt.life&&a.bolt.flashes==b.bolt.flashes);assert(a.bolt.x==128.5&&a.bolt.y==100&&a.bolt.z==128.5);}
 assert(first.events[0].bolt.flashes==1&&first.events[1].bolt.flashes==0&&first.events[0].bolt.seed!=first.events[1].bolt.seed);
 assert(late.peer.connect("127.0.0.1",port));until([&]{return late.started&&level->players.size()==3;});hold(300);
 assert(late.events.empty()&&late.last.raining&&late.last.thundering&&first.events.size()==3&&second.events.size()==3);
 std::cout<<"Real two-client lightning: identical spawn/reflash/end IDs,seeds,coords; exactly one extra flash; expired bolt not replayed to late join; malicious client weather/bolt packets cannot change authority or broadcast PASS\n";
 first.peer.disconnect();second.peer.disconnect();late.peer.disconnect();
}
