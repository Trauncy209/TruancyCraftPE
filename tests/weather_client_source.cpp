#include "client/Minecraft.h"
#include "client/User.h"
#include "network/ClientSideNetworkHandler.h"
#include "network/RakNetInstance.h"
#include "raknet/RakPeerInterface.h"
#include <chrono>
#include <thread>
#include <functional>
#include "network/packet/WeatherPacket.h"
#include "network/packet/WeatherLightningPacket.h"
#include "world/level/Level.h"
#include "world/level/LevelListener.h"
#include "world/level/WeatherLightningRuntime.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include <cassert>
#include <iostream>
struct GameProbe:Minecraft{GameProbe():Minecraft(){}};
struct SoundCounter:LevelListener{int thunder=0,impact=0;void playSound(const std::string& name,float,float,float,float,float)override{if(name=="ambient.weather.thunder")++thunder;if(name=="random.explode")++impact;}};
int main(){
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
 auto* game=new GameProbe();game->user=new User("SourceProbe","");
 auto* level=new Level(new MemoryLevelStorage(),"source-boundary",LevelSettings(123,GameType::Survival,LGV_ORIGINAL),LGV_ORIGINAL);level->isClientSide=true;game->level=level;
 RakNetInstance peer;game->raknetInstance=&peer;ClientSideNetworkHandler handler(game,&peer);
 const RakNet::RakNetGUID authority(UINT64_C(0x123456789ABCDEF0)),unrelated(UINT64_C(0x23456789ABCDEF01));
 handler.onConnect(authority);handler.levelGenerated(level);SoundCounter sounds;level->addListener(&sounds);
 WeatherState wet;assert(wet.command("thunder",12345));wet.restoreIntensity();WeatherPacket snapshot(wet);
 handler.handle(unrelated,&snapshot);assert(!level->getLevelData()->weather.raining&&level->getLevelData()->weather.rainTicks==0);
 WeatherLightning bolt;bolt.id=77;bolt.seed=UINT64_C(0x9ABCDEF012345678);bolt.x=128;bolt.y=100;bolt.z=128;WeatherLightningPacket spawn(bolt,0);assert(spawn.valid);
 handler.handle(unrelated,&spawn);WeatherLightningRuntime::tick(*level);assert(level->weatherLightning.empty()&&sounds.thunder==0&&sounds.impact==0);
 handler.handle(authority,&snapshot);assert(level->getLevelData()->weather.raining&&level->getLevelData()->weather.thundering&&level->getLevelData()->weather.rainTicks==12345);
 handler.handle(authority,&spawn);assert(level->weatherLightning.size()==1&&level->weatherLightning.front().seed==bolt.seed);
 WeatherLightningRuntime::tick(*level);assert(sounds.thunder==1&&sounds.impact==1);
 const int originalLife=level->weatherLightning.front().life;
 WeatherState clear;assert(clear.command("clear",20));WeatherPacket forgedClear(clear);handler.handle(unrelated,&forgedClear);
 WeatherLightning reflash=bolt;reflash.life=1;reflash.flashes=0;reflash.seed=999;WeatherLightningPacket flash(reflash,1),remove(bolt,2);
 handler.handle(unrelated,&flash);handler.handle(unrelated,&remove);
 assert(level->getLevelData()->weather.raining&&level->getLevelData()->weather.rainTicks==12345);
 assert(level->weatherLightning.size()==1&&level->weatherLightning.front().seed==bolt.seed&&level->weatherLightning.front().life==originalLife);
 snapshot.valid=false;handler.handle(authority,&snapshot);spawn.valid=false;handler.handle(authority,&spawn);assert(level->weatherLightning.size()==1);
 spawn.valid=true;handler.handle(authority,&spawn);assert(level->weatherLightning.size()==1&&sounds.thunder==1&&sounds.impact==1);
 handler.handle(authority,&flash);assert(level->weatherLightning.front().seed==999&&level->weatherLightning.front().flashes==0);
 handler.handle(authority,&remove);assert(level->weatherLightning.empty());
 handler.handle(authority,&spawn);WeatherLightningRuntime::tick(*level);assert(level->weatherLightning.empty()&&sounds.thunder==1&&sounds.impact==1);
 // Also deliver actual serialized packets over loopback from two distinct
 // peers. The receiving transport deliberately admits both, so admission
 // filtering cannot mask the client handler's authoritative-GUID check.
 assert(peer.host("Client source boundary",29839,4));
 RakNetInstance goodWire,badWire;
 struct Connected:NetEventCallback{bool ready=false;void onConnect(const RakNet::RakNetGUID&)override{ready=true;}}goodConnected,badConnected;
 assert(goodWire.connect("127.0.0.1",29839)&&badWire.connect("127.0.0.1",29839));
 auto pump=[&]{peer.runEvents(&handler);goodWire.runEvents(&goodConnected);badWire.runEvents(&badConnected);std::this_thread::sleep_for(std::chrono::milliseconds(3));};
 auto hold=[&](int ms){auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(ms);while(std::chrono::steady_clock::now()<end)pump();};
 for(int i=0;i<2000&&(!goodConnected.ready||!badConnected.ready);++i)pump();assert(goodConnected.ready&&badConnected.ready);
 handler.onConnect(goodWire.getPeer()->GetMyGUID());handler.levelGenerated(level);
 WeatherLightning wireBolt=bolt;wireBolt.id=88;WeatherLightningPacket wireSpawn(wireBolt,0),wireRemove(wireBolt,2);
 badWire.send(forgedClear);badWire.send(wireSpawn);hold(250);WeatherLightningRuntime::tick(*level);
 assert(level->getLevelData()->weather.raining&&level->weatherLightning.empty()&&sounds.thunder==1&&sounds.impact==1);
 goodWire.send(forgedClear);goodWire.send(wireSpawn);hold(250);
 assert(!level->getLevelData()->weather.raining&&level->weatherLightning.size()==1&&level->weatherLightning.front().id==88);
 WeatherLightningRuntime::tick(*level);assert(sounds.thunder==2&&sounds.impact==2);
 badWire.send(wireRemove);hold(200);assert(level->weatherLightning.size()==1);
 goodWire.send(wireRemove);hold(200);assert(level->weatherLightning.empty());
 goodWire.disconnect();badWire.disconnect();
 std::cout<<"Actual loopback transport from admitted unrelated/authority peers: client GUID guard rejects weather/spawn/remove and audio, accepts only authority PASS\n";
 std::cout<<"Actual ClientSideNetworkHandler: unrelated GUID cannot mutate weather/spawn/reflash/remove lightning or emit sound; valid server accepted; invalid/duplicate/retired packets cannot replay initial audio PASS\n";
}
