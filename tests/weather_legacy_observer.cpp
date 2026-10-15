#include "network/RakNetInstance.h"
#include "network/NetEventCallback.h"
#include "network/packet/PacketInclude.h"
#include "SharedConstants.h"
#include <cassert>
#include <chrono>
#include <thread>
#include <iostream>
#include <cmath>
struct Observer:NetEventCallback{
 RakNetInstance peer;int legacy=-1,moves=0,chats=0,removed=0,weather=0,strikes=0;bool ready=false;float initialX=0,initialZ=0,lastX=0,lastZ=0;
 void onConnect(const RakNet::RakNetGUID&)override{LoginPacket p("LegacyObserver",SharedConstants::NetworkProtocolVersion);peer.send(p);}
 void handle(const RakNet::RakNetGUID&,LoginStatusPacket* p)override{assert(p->status==0);peer.setIsLoggedIn(true);}
 void handle(const RakNet::RakNetGUID&,StartGamePacket*)override{ready=true;ReadyPacket p(ReadyPacket::READY_CLIENTGENERATION);peer.send(p);}
 void handle(const RakNet::RakNetGUID&,AddPlayerPacket* p)override{if(std::string(p->name.C_String())=="LegacyWeatherQA"){legacy=p->entityId;initialX=lastX=p->x;initialZ=lastZ=p->z;std::cout<<"LEGACY_PRESENT id="<<legacy<<" pos="<<lastX<<","<<lastZ<<"\n";}}
 void handle(const RakNet::RakNetGUID&,MovePlayerPacket* p)override{if(p->entityId==legacy){++moves;lastX=p->x;lastZ=p->z;}}
 void handle(const RakNet::RakNetGUID&,MessagePacket* p)override{if(std::string(p->message.C_String()).find("legacy-weather-probe")!=std::string::npos){++chats;std::cout<<"LEGACY_CHAT "<<p->message.C_String()<<"\n";}}
 void handle(const RakNet::RakNetGUID&,UpdateBlockPacket* p)override{if(p->blockId==0){++removed;std::cout<<"BLOCK_REMOVED "<<p->x<<","<<int(p->y)<<","<<p->z<<"\n";}}
 void handle(const RakNet::RakNetGUID&,WeatherPacket* p)override{assert(p->valid);if(p->state.raining&&p->state.thundering)++weather;}
 void handle(const RakNet::RakNetGUID&,WeatherLightningPacket* p)override{assert(p->valid);++strikes;std::cout<<"BOLT phase="<<int(p->phase)<<" id="<<p->bolt.id<<" coords="<<p->bolt.x<<","<<p->bolt.y<<","<<p->bolt.z<<"\n";}
};
int main(){std::cout.setf(std::ios::unitbuf);Observer o;assert(o.peer.connect("127.0.0.1",29841));auto until=std::chrono::steady_clock::now()+std::chrono::seconds(240);int previous=0;
 while(std::chrono::steady_clock::now()<until){o.peer.runEvents(&o);int elapsed=240-int(std::chrono::duration_cast<std::chrono::seconds>(until-std::chrono::steady_clock::now()).count());if(elapsed/10>previous){previous=elapsed/10;std::cout<<"PROGRESS moves="<<o.moves<<" distance="<<std::hypot(o.lastX-o.initialX,o.lastZ-o.initialZ)<<" chats="<<o.chats<<" removed="<<o.removed<<" storms="<<o.weather<<" boltEvents="<<o.strikes<<"\n";}std::this_thread::sleep_for(std::chrono::milliseconds(4));}
 o.peer.disconnect();}
