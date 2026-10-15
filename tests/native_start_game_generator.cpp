#include "client/Minecraft.h"
#include "client/User.h"
#include "client/player/LocalPlayer.h"
#include "client/player/input/KeyboardInput.h"
#include "network/ClientSideNetworkHandler.h"
#include "network/RakNetInstance.h"
#include "network/packet/PacketInclude.h"
#include "world/level/Level.h"
#include "world/level/LevelListener.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/storage/LevelStorageSource.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include <cassert>
#include <iostream>
#include <set>
struct Storage:LevelStorageSource{
 int deletes=0;std::string getName()override{return "test memory";}LevelData* getDataTagFor(const std::string&)override{return nullptr;}
 LevelStorage* selectLevel(const std::string&,bool)override{return new MemoryLevelStorage();}
 bool isNewLevelIdAcceptable(const std::string&)override{return true;}void clearAll()override{}void deleteLevel(const std::string&)override{++deletes;}void renameLevel(const std::string&,const std::string&)override{}
 bool isConvertible(const std::string&)override{return false;}bool requiresConversion(const std::string&)override{return false;}bool convertLevel(const std::string&,ProgressListener*)override{return false;}
};
struct Game: Minecraft{
 explicit Game(Storage* s):Minecraft(){storageSource=s;user=new User("GeneratorProbe","");}
 void setLevel(Level* l,const std::string&,LocalPlayer* p)override{level=l;player=p;l->setUpdateLights(false);}
};
struct Wire:IRakNetInstance{
 RakNetInstance peer;int generationReady=0,chunksReady=0;std::vector<std::pair<int,int>> requests;
 RakNet::RakPeerInterface* getPeer()override{return peer.getPeer();}
 void send(Packet& p)override{if(auto* request=dynamic_cast<RequestChunkPacket*>(&p))requests.push_back({request->x,request->z});if(auto* ready=dynamic_cast<ReadyPacket*>(&p)){if(ready->type==ReadyPacket::READY_CLIENTGENERATION)++generationReady;if(ready->type==ReadyPacket::READY_REQUESTEDCHUNKS)++chunksReady;}}
};
struct Dirty:LevelListener{int low=-1,high=-1;void setTilesDirty(int,int a,int,int,int b,int)override{low=a;high=b;}};
void run(int wireGenerator,int expected,bool complete){
 Storage storage;auto* game=new Game(&storage);Wire wire;game->raknetInstance=&wire;ClientSideNetworkHandler handler(game,&wire);const RakNet::RakNetGUID source(123456);
 handler.onConnect(source);StartGamePacket start(1792046386,wireGenerator,GameType::Survival,25,expected==LGV_INFINITE?-26.5f:48.5f,65.f,21.5f);handler.handle(source,&start);
 assert(game->level&&game->player&&game->level->isClientSide&&game->level->getLevelData()->getGeneratorVersion()==expected);
 assert(storage.deletes==1&&game->level->getLevelData()->getSeed()==1792046386&&game->level->getLevelData()->getGameType()==GameType::Survival);
 if(expected==LGV_INFINITE)assert(game->player->x<0);
 // Reproduce prepareLevel's local terrain pre-generation before the network callback.
 if(complete)for(int x=-10;x<=5;++x)for(int z=-7;z<=8;++z)game->level->getChunkSource()->create(x,z);
 game->netCallback=&handler;game->player->input=new KeyboardInput(&game->options);
 if(complete){const float x=game->player->x,y=game->player->y;game->player->xd=1;game->player->yd=-1;game->player->zd=1;game->player->move(1,-1,0);assert(game->player->x==x&&game->player->y==y&&game->player->xd==0&&game->player->yd==0&&game->player->zd==0);}
 handler.levelGenerated(game->level);assert(wire.generationReady==1&&!wire.requests.empty());
 const auto center=wire.requests.front();
 ChunkDataPacket spoof;spoof.x=center.first;spoof.z=center.second;for(int col=0;col<256;++col)spoof.chunkData.Write((unsigned char)0);auto beforeSpoof=wire.requests.size();handler.handle(RakNet::RakNetGUID(9999),&spoof);assert(wire.requests.size()==beforeSpoof&&wire.chunksReady==0);
 assert(center.first==(expected==LGV_INFINITE?-2:3)&&center.second==1);
 std::set<std::pair<int,int>> consumed;
 for(int i=0;i<(complete?256:1)&&i<int(wire.requests.size());++i){auto pos=wire.requests[i];assert(consumed.insert(pos).second);ChunkDataPacket chunk;chunk.x=pos.first;chunk.z=pos.second;for(int col=0;col<256;++col)chunk.chunkData.Write((unsigned char)0);if(complete&&i==255)assert(wire.chunksReady==0);
 handler.handle(source,&chunk);
 if(complete&&i==0){auto requests=wire.requests.size();ChunkDataPacket duplicate;duplicate.x=pos.first;duplicate.z=pos.second;for(int col=0;col<256;++col)duplicate.chunkData.Write((unsigned char)0);handler.handle(source,&duplicate);assert(wire.requests.size()==requests&&wire.chunksReady==0);}
 assert(game->level->hasChunkNow(pos.first,pos.second));assert(!game->level->getChunk(pos.first,pos.second)->isEmpty());}
 if(complete){float oldX=game->player->x;game->player->noPhysics=true;game->player->move(1,0,0);assert(game->player->x==oldX+1);assert(consumed.size()==256&&wire.chunksReady==1);for(auto p:consumed)assert(p.first>=-10&&p.first<=5&&p.second>=-7&&p.second<=8);auto count=wire.requests.size();handler.recenterChunkRequests();assert(wire.requests.size()==count&&wire.chunksReady==1);
 Dirty dirty;game->level->addListener(&dirty);ChunkDataPacket snapshot;snapshot.x=center.first;snapshot.z=center.second;
 for(int col=0;col<256;++col){snapshot.chunkData.Write((unsigned char)255);for(int section=0;section<8;++section){unsigned char blocks[16]={0},data[8]={0};if(section==0)blocks[0]=Tile::rock->id;snapshot.chunkData.Write((char*)blocks,16);snapshot.chunkData.Write((char*)data,8);}}
 handler.handle(source,&snapshot);assert(dirty.low==0&&dirty.high==127);assert(game->level->getChunk(center.first,center.second)->getHeightmap(0,0)==1);assert(wire.chunksReady==1);
 auto next=wire.requests.size();game->player->x+=16;handler.recenterChunkRequests();assert(wire.requests.size()>next);const auto entering=wire.requests[next];const int bx=entering.first*16,bz=entering.second*16;
 game->level->getChunkSource()->create(entering.first,entering.second);game->level->setTileNoUpdate(bx,100,bz,0);UpdateBlockPacket update(bx,100,bz,Tile::rock->id,0);handler.handle(source,&update);assert(game->level->getTile(bx,100,bz)==0);
 for(size_t i=next;i<wire.requests.size();++i){assert(i<512);auto pos=wire.requests[i];ChunkDataPacket packet;packet.x=pos.first;packet.z=pos.second;for(int col=0;col<256;++col)packet.chunkData.Write((unsigned char)0);handler.handle(source,&packet);}
 assert(game->level->getTile(bx,100,bz)==Tile::rock->id&&wire.chunksReady==1);
 auto* authoritative=game->level->getChunk(center.first,center.second);game->level->getChunkSource()->create(center.first+32,center.second);assert(game->level->getChunk(center.first,center.second)->isEmpty());auto* blank=game->level->getChunkSource()->create(center.first,center.second);assert(blank!=authoritative&&blank->getTile(0,0,0)==0&&!handler.isChunkLoaded(center.first,center.second));
 float heldX=game->player->x;game->player->move(-16,0,0);assert(game->player->x==heldX);auto beforeEviction=wire.requests.size();handler.recenterChunkRequests();assert(wire.requests.size()==beforeEviction+1&&wire.requests.back()==center);ChunkDataPacket returned;returned.x=center.first;returned.z=center.second;for(int col=0;col<256;++col)returned.chunkData.Write((unsigned char)0);handler.handle(source,&returned);assert(handler.isChunkLoaded(center.first,center.second)&&wire.chunksReady==1);
 std::cout<<"Actual native physics holds pending/evicted chunks; cache-eviction blank identity forces authoritative re-request PASS\n";


}
 else assert(wire.chunksReady==0);
 std::cout<<"Actual StartGame generator="<<wireGenerator<<" preserved="<<expected<<" first chunk="<<center.first<<","<<center.second<<" consumed="<<consumed.size()<<" requested-ready="<<wire.chunksReady<<" PASS\n";
}
int main(){std::cout.setf(std::ios::unitbuf);Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();run(LGV_ORIGINAL,LGV_ORIGINAL,false);run(LGV_INFINITE,LGV_INFINITE,true);run(SharedConstants::StorageVersion,LGV_ORIGINAL,false);std::cout<<"Native StartGame finite/infinite generator propagation and negative-spawn full chunk readiness PASS\n";}
