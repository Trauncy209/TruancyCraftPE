#include "network/RakNetInstance.h"
#include "network/NetEventCallback.h"
#include "network/packet/PacketInclude.h"
#include "SharedConstants.h"
#include "raknet/CCRakNetSlidingWindow.h"
#include <limits>
#include <cassert>
#include <chrono>
#include <thread>
#include <functional>
#include <iostream>
#include <string>
struct Client:NetEventCallback {
    RakNetInstance& peer;std::string name;int id=-1,others=0,mobs=0,moves=0,messages=0,removed=0,tile=-1;bool started=false,failed=false;
    Client(RakNetInstance& peer,const char* name):peer(peer),name(name){}
    void onConnect(const RakNet::RakNetGUID&)override{LoginPacket login(name.c_str(),SharedConstants::NetworkProtocolVersion);peer.send(login);}
    void onUnableToConnect()override{failed=true;}
    void handle(const RakNet::RakNetGUID&,LoginStatusPacket* packet)override{assert(packet->status==0);peer.setIsLoggedIn(true);}
    void handle(const RakNet::RakNetGUID&,StartGamePacket* packet)override{assert(packet->levelSeed==123456&&packet->gameType==0);id=packet->entityId;started=true;ReadyPacket ready(ReadyPacket::READY_CLIENTGENERATION);peer.send(ready);}
    void handle(const RakNet::RakNetGUID&,AddPlayerPacket*)override{++others;}
    void handle(const RakNet::RakNetGUID&,AddMobPacket*)override{++mobs;}
    void handle(const RakNet::RakNetGUID&,MovePlayerPacket*)override{++moves;}
    void handle(const RakNet::RakNetGUID&,MessagePacket* packet)override{if(std::string(packet->message.C_String()).find("dedicated-roundtrip")!=std::string::npos)++messages;}
    void handle(const RakNet::RakNetGUID&,UpdateBlockPacket* packet)override{if(packet->x==8&&packet->z==8&&packet->y==0&&packet->blockId==0)++removed;}
    void handle(const RakNet::RakNetGUID&,ChunkDataPacket* packet)override{
        if(packet->x!=0||packet->z!=0)return;
        for(int i=0;i<256;++i){unsigned char mask;assert(packet->chunkData.Read(mask));for(int section=0;section<8;++section)if(mask&(1<<section)){char blocks[16],data[8];assert(packet->chunkData.Read(blocks,16)&&packet->chunkData.Read(data,8));if(i==136&&section==0)tile=(unsigned char)blocks[0];}}
    }
};
int main(int argc,char** argv){
    std::cout.setf(std::ios::unitbuf);struct AckProbe:RakNet::CCRakNetSlidingWindow{using RakNet::CCRakNetSlidingWindow::GetSenderRTOForACK;};AckProbe ack;ack.Init(0,1400);assert(ack.GetSenderRTOForACK()==std::numeric_limits<CCTimeType>::max());assert(ack.ShouldSendACKs(0,0));std::cout<<"Defined ACK timestamp sentinel regression PASS\n";assert(argc==3);int port=std::stoi(argv[1]);bool modify=std::string(argv[2])=="modify";RakNetInstance a,b,c;Client ca(a,"Alpha"),cb(b,"Beta"),cc(c,"Refused");
    auto pump=[&]{a.runEvents(&ca);b.runEvents(&cb);c.runEvents(&cc);std::this_thread::sleep_for(std::chrono::milliseconds(5));};
    auto until=[&](std::function<bool()> done){auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(!done()&&std::chrono::steady_clock::now()<deadline)pump();assert(done());};
    assert(a.pingForHost("127.0.0.1",port));until([&]{return !a.getServerList().empty();});assert(a.getServerList().front().address.GetPort()==port);
    assert(a.connect("127.0.0.1",port)&&b.connect("127.0.0.1",port));until([&]{return ca.started&&cb.started&&ca.others>0&&cb.others>0&&ca.mobs>0&&cb.mobs>0;});assert(ca.id!=cb.id);
    MovePlayerPacket move(ca.id,128.f,70.f,128.f,15.f,0.f);a.send(move);ChatPacket chat("dedicated-roundtrip");a.send(chat);until([&]{return cb.moves>0&&ca.messages>0&&cb.messages>0;});
    if(modify){RemoveBlockPacket block;block.entityId=ca.id;block.x=8;block.y=0;block.z=8;a.send(block);until([&]{return ca.removed>0&&cb.removed>0;});}
    RequestChunkPacket request(0,0);a.send(request);until([&]{return ca.tile==0;});
    assert(c.connect("127.0.0.1",port));until([&]{return cc.failed;});
    std::cout<<"CLIENTS_READY: actual dedicated world: two logins/spawns, player visibility, movement, chat, block/chunk delta and capacity rejection PASS\n";
    auto hold=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(std::chrono::steady_clock::now()<hold)pump();a.disconnect();b.disconnect();
    std::cout<<(modify?"World mutation confirmed":"Saved block delta and original seed recovered after restart")<<" PASS\n";
}
