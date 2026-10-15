#include "network/RakNetInstance.h"
#include "network/NetEventCallback.h"
#include "world/entity/player/Player.h"
#include "world/entity/player/Inventory.h"
#include "network/packet/LoginPacket.h"
#include "network/packet/LoginStatusPacket.h"
#include "network/packet/MovePlayerPacket.h"
#include "network/packet/UpdateBlockPacket.h"
#include "network/packet/MessagePacket.h"
#include "network/packet/SendInventoryPacket.h"
#include "network/packet/RequestChunkPacket.h"
#include "SharedConstants.h"
#include "client/gui/screens/NativeHostResolver.h"
#include "tcpe_java_network.h"
#include <cassert>
#include <functional>
#include "raknet/RakPeerInterface.h"
#include <chrono>
#include <thread>
#include <iostream>
struct Client:NetEventCallback{
 RakNetInstance* peer;std::string name;bool logged=false,failed=false;int moves=0,blocks=0,messages=0,inventory=0;Client(RakNetInstance* p,const char* n):peer(p),name(n){}
 void onUnableToConnect()override{failed=true;std::cout<<name<<" unable to connect"<<std::endl;}
 void onConnect(const RakNet::RakNetGUID&)override{std::cout<<name<<" connected; sending login"<<std::endl;LoginPacket p(name.c_str(),SharedConstants::NetworkProtocolVersion);peer->send(p);}
 void handle(const RakNet::RakNetGUID&,LoginStatusPacket* p)override{assert(p->status==0);logged=true;peer->setIsLoggedIn(true);}
 void handle(const RakNet::RakNetGUID&,MovePlayerPacket* p)override{assert(p->entityId==42&&p->x==1.25f&&p->z==9.f);++moves;}
 void handle(const RakNet::RakNetGUID&,UpdateBlockPacket* p)override{assert(p->x==4&&p->y==65&&p->z==7&&p->blockId==1);++blocks;}
 void handle(const RakNet::RakNetGUID&,MessagePacket* p)override{assert(p->message=="native round trip");++messages;}
 void handle(const RakNet::RakNetGUID&,SendInventoryPacket* p)override{assert(p->entityId==42&&p->numItems==0&&p->items.size()==4);++inventory;}
};
struct Server:NetEventCallback{
 RakNetInstance* peer;int logins=0,moves=0,chunks=0;explicit Server(RakNetInstance* p):peer(p){}
 void onNewClient(const RakNet::RakNetGUID&)override{std::cout<<"Host accepted native peer"<<std::endl;}
 void handle(const RakNet::RakNetGUID& from,LoginPacket* p)override{std::cout<<"Host got login "<<p->clientName.C_String()<<std::endl;assert(p->clientNetworkVersion==SharedConstants::NetworkProtocolVersion);assert(p->clientName=="alpha"||p->clientName=="beta");++logins;LoginStatusPacket reply(0);peer->send(from,reply);}
 void handle(const RakNet::RakNetGUID&,MovePlayerPacket* p)override{assert(p->entityId==42);++moves;peer->send(*p);}
 void handle(const RakNet::RakNetGUID&,RequestChunkPacket* p)override{assert(p->x==2&&p->z==3);++chunks;}
};
int main(int argc,char** argv){
 assert(NativeHostResolver::resolve("127.0.0.1")=="127.0.0.1");auto dnsEnd=std::chrono::steady_clock::now()+std::chrono::seconds(5);std::string resolved;while(resolved.empty()&&std::chrono::steady_clock::now()<dnsEnd){resolved=NativeHostResolver::resolve("localhost");std::this_thread::sleep_for(std::chrono::milliseconds(5));}assert(resolved=="127.0.0.1");std::cout<<"Exact native IPv4 hostname resolution PASS\n";std::cout.setf(std::ios::unitbuf);assert(argc==2);int port=std::atoi(argv[1]);RakNetInstance host,a,b,c;std::cout<<"GUIDs "<<host.getPeer()->GetMyGUID().g<<" "<<a.getPeer()->GetMyGUID().g<<" "<<b.getPeer()->GetMyGUID().g<<std::endl;Server server(&host);Client ca(&a,"alpha"),cb(&b,"beta"),cc(&c,"refused");assert(host.host("Native fixture",port,4));host.announceServer("Native fixture");
 auto pump=[&]{host.runEvents(&server);a.runEvents(&ca);b.runEvents(&cb);c.runEvents(&cc);std::this_thread::sleep_for(std::chrono::milliseconds(5));};
 auto until=[&](std::function<bool()> done){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(8);while(!done()&&std::chrono::steady_clock::now()<end)pump();assert(done());};
 std::cout<<"Starting discovery on "<<port<<std::endl;assert(a.pingForHost("127.0.0.1",port));until([&]{return !a.getServerList().empty();});assert(a.getServerList().front().name=="Native fixture"&&a.getServerList().front().address.GetPort()==port);std::cout<<"Actual native UDP discovery/pong PASS\n";
 std::cout<<"Starting two logins"<<std::endl;assert(a.connect("127.0.0.1",port)&&b.connect("127.0.0.1",port));until([&]{return ca.logged&&cb.logged&&server.logins==2;});
 MovePlayerPacket move(42,1.25f,64.f,9.f,15.f,30.f);a.send(move);RequestChunkPacket request(2,3);b.send(request);UpdateBlockPacket block(4,65,7,1,0);host.send(block);MessagePacket text("native round trip");host.send(text);SendInventoryPacket inv;inv.entityId=42;inv.extra=0;inv.numItems=0;inv.items.resize(4);host.send(inv);
 until([&]{return server.moves==1&&server.chunks==1&&ca.moves==1&&cb.moves==1&&ca.blocks==1&&cb.blocks==1&&ca.messages==1&&cb.messages==1&&ca.inventory==1&&cb.inventory==1;});std::cout<<"Two actual RakNet clients: native login, movement relay, chunk request, block, chat and inventory packet round trips PASS\n";
 TcpeJavaStatusRequest* status=tcpe_java_status_start("127.0.0.1",9,port);assert(status);TcpeJavaStatusView view{};int result=0;until([&]{result=tcpe_java_status_poll(status,&view);return result!=0;});assert(result>0&&view.online==3&&view.max==5&&view.icon_len==64*64*4);assert(view.icon[0]==17&&view.icon[1]==34&&view.icon[2]==51&&view.icon[3]==255);assert(std::string(reinterpret_cast<const char*>(view.motd),view.motd_len).find("Native fixture")!=std::string::npos);tcpe_java_status_free(status);std::cout<<"Actual listing client: same port TCP status, 3/5 count, PNG decoded to exact 64x64 RGBA PASS\n";
 a.disconnect();until([&]{return host.getPeer()->NumberOfConnections()==1;});ca.logged=false;assert(a.connect("127.0.0.1",port));until([&]{return ca.logged&&server.logins==3;});host.getPeer()->SetMaximumIncomingConnections(0);assert(c.connect("127.0.0.1",port));until([&]{return cc.failed;});std::cout<<"Full/refusing host reaches failure callback instead of endless progress PASS\n";host.disconnect();std::cout<<"Native disconnect/reconnect and listener shutdown PASS\n";
}
