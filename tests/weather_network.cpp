#include "network/RakNetInstance.h"
#include "network/NetEventCallback.h"
#include "network/packet/PacketInclude.h"
#include "SharedConstants.h"
#include <chrono>
#include <thread>
#include <cassert>
#include <iostream>
struct WeatherClient:NetEventCallback{
 RakNetInstance peer;bool authority=false,started=false,command=false,failed=false;int snapshots=0;WeatherState last;std::string name;
 WeatherClient(const char* n,bool a):authority(a),name(n){}
 void onConnect(const RakNet::RakNetGUID&)override{LoginPacket p(name.c_str(),SharedConstants::NetworkProtocolVersion);peer.send(p);}
 void onUnableToConnect()override{failed=true;}
 void handle(const RakNet::RakNetGUID&,LoginStatusPacket* p)override{if(p->status)failed=true;else peer.setIsLoggedIn(true);}
 void handle(const RakNet::RakNetGUID&,StartGamePacket*)override{started=true;ReadyPacket ready(ReadyPacket::READY_CLIENTGENERATION);peer.send(ready);}
 void handle(const RakNet::RakNetGUID&,WeatherPacket* p)override{assert(p->valid);last=p->state;++snapshots;}
 void tick(){peer.runEvents(this);if(started&&authority&&!command){ChatPacket p("/weather thunder 30");peer.send(p);command=true;}}
};
int main(int argc,char**argv){assert(argc==2);int port=std::stoi(argv[1]);WeatherClient owner("WeatherProbe",true),observer("WeatherObserver",false);assert(owner.peer.connect("127.0.0.1",port));assert(observer.peer.connect("127.0.0.1",port));
 auto end=std::chrono::steady_clock::now()+std::chrono::seconds(7);while(std::chrono::steady_clock::now()<end){owner.tick();observer.tick();assert(!owner.failed&&!observer.failed);std::this_thread::sleep_for(std::chrono::milliseconds(5));}
 assert(owner.started&&observer.started&&owner.snapshots>=4&&observer.snapshots>=4);assert(owner.last.raining&&owner.last.thundering&&observer.last.raining&&observer.last.thundering);assert(owner.last.rain>.9f&&observer.last.rain>.9f);assert(std::abs(owner.last.rain-observer.last.rain)<.03f);
 observer.peer.disconnect();WeatherClient late("WeatherReconnect",false);assert(late.peer.connect("127.0.0.1",port));end=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(std::chrono::steady_clock::now()<end){owner.tick();late.tick();assert(!late.failed);std::this_thread::sleep_for(std::chrono::milliseconds(5));}
 assert(late.started&&late.snapshots>=1&&late.last.raining&&late.last.thundering&&late.last.rain>.9f);assert(late.last.rainTicks>0&&late.last.rainTicks<600);
 std::cout<<"Authoritative thunder command, repeated snapshots, two-client convergence and late reconnect during storm PASS\n";
 owner.peer.disconnect();late.peer.disconnect();
}
