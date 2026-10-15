#include "network/RakNetInstance.h"
#include "network/NetEventCallback.h"
#include "network/packet/PacketInclude.h"
#include "world/level/storage/ExternalFileLevelStorage.h"
#include "SharedConstants.h"
#include <chrono>
#include <thread>
#include <cassert>
#include <iostream>
struct Probe:NetEventCallback{
 RakNetInstance peer;std::string command;bool started=false,set=false,failed=false;int snapshots=0;WeatherState last;
 explicit Probe(const std::string& command):command(command){}
 void onConnect(const RakNet::RakNetGUID&)override{LoginPacket p("WeatherRestart",SharedConstants::NetworkProtocolVersion);peer.send(p);}
 void onUnableToConnect()override{failed=true;}
 void handle(const RakNet::RakNetGUID&,LoginStatusPacket* p)override{assert(p->status==0);peer.setIsLoggedIn(true);}
 void handle(const RakNet::RakNetGUID&,StartGamePacket*)override{started=true;ReadyPacket p(ReadyPacket::READY_CLIENTGENERATION);peer.send(p);}
 void handle(const RakNet::RakNetGUID&,WeatherPacket* p)override{assert(p->valid);last=p->state;++snapshots;}
 void tick(){peer.runEvents(this);if(!command.empty()&&started&&!set){ChatPacket p(command=="clear"?"/weather clear 1200":"/weather thunder 1200");peer.send(p);set=true;}}
};
int main(int argc,char**argv){
 std::cout.setf(std::ios::unitbuf);assert(argc==3||argc==4||argc==5);
 if(std::string(argv[1])=="inspect"||std::string(argv[1])=="inspect-clear"||std::string(argv[1])=="inspect-sleep"){
  const bool wet=std::string(argv[1])=="inspect";
  LevelData saved;assert(ExternalFileLevelStorage::readLevelData(argv[2],saved));
  assert(saved.weather.raining==wet&&saved.weather.thundering==wet);
  if(std::string(argv[1])=="inspect-sleep"){assert(saved.weather.rainTicks>0&&saved.weather.thunderTicks>0);std::cout<<"SAVED_SLEEP "<<saved.weather.rainTicks<<" "<<saved.weather.thunderTicks<<"\n";return 0;}
  assert(saved.weather.rainTicks>20000&&saved.weather.rainTicks<=24000);
  assert(saved.weather.rainTicks==saved.weather.thunderTicks);
  std::cout<<(wet?"SAVED ":"SAVED_CLEAR ")<<saved.weather.rainTicks<<" "<<saved.weather.thunderTicks<<" "<<saved.getSeed()<<"\n";return 0;
 }
 const int port=std::stoi(argv[1]);const std::string mode=argv[2];const bool command=mode=="set"||mode=="clear",wet=mode=="set"||mode=="observe";Probe probe(command?(wet?"thunder":"clear"):"");assert(probe.peer.connect("127.0.0.1",port));
 auto end=std::chrono::steady_clock::now()+std::chrono::seconds(command?6:3);
 while(std::chrono::steady_clock::now()<end){probe.tick();assert(!probe.failed);std::this_thread::sleep_for(std::chrono::milliseconds(4));}
 assert(probe.started&&probe.snapshots>=2&&probe.last.raining==wet&&probe.last.thundering==wet);
 assert(wet?(probe.last.rain>.9f&&probe.last.thunder>.9f):(probe.last.rain<.01f&&probe.last.thunder<.01f));
 assert(mode=="observe-sleep"?(probe.last.rainTicks>0&&probe.last.thunderTicks>0):(probe.last.rainTicks>20000&&probe.last.rainTicks<=24000));
 if(!command){assert(argc==4||argc==5);const int saved=std::stoi(argv[3]);assert(probe.last.rainTicks<=saved&&saved-probe.last.rainTicks<200);if(argc==5){const int thunder=std::stoi(argv[4]);assert(probe.last.thunderTicks<=thunder&&thunder-probe.last.thunderTicks<200);}}
 std::cout<<(wet?"CLIENT ":"CLIENT_CLEAR ")<<probe.last.rainTicks<<" "<<probe.last.thunderTicks<<" "<<probe.last.rain<<" "<<probe.last.thunder<<" snapshots="<<probe.snapshots<<"\n";probe.peer.disconnect();
}
