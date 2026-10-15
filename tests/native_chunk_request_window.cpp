#define main generatorFixtureMain
#include "native_start_game_generator.cpp"
#undef main
int main(){Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
 Storage storage;auto* game=new Game(&storage);Wire wire;game->raknetInstance=&wire;ClientSideNetworkHandler handler(game,&wire);RakNet::RakNetGUID source(123456);handler.onConnect(source);StartGamePacket start(17,LGV_INFINITE,GameType::Survival,25,-26.5f,65.f,21.5f);handler.handle(source,&start);game->netCallback=&handler;handler.levelGenerated(game->level);
 assert(wire.requests.size()==4);assert(wire.requests.front()==std::make_pair(-2,1));handler.recenterChunkRequests();assert(wire.requests.size()==4);
 std::set<std::pair<int,int>> received;int rounds=0;size_t consumed=0;
 while(consumed<wire.requests.size()){const size_t end=wire.requests.size();assert(end-consumed<=4);++rounds;
 // Reverse responses in each simulated latency round to exercise out-of-order arrivals.
 for(size_t i=end;i>consumed;--i){auto pos=wire.requests[i-1];assert(received.insert(pos).second);ChunkDataPacket packet;packet.x=pos.first;packet.z=pos.second;for(int col=0;col<256;++col)packet.chunkData.Write((unsigned char)0);handler.handle(source,&packet);
 auto count=wire.requests.size();ChunkDataPacket duplicate;duplicate.x=pos.first;duplicate.z=pos.second;for(int col=0;col<256;++col)duplicate.chunkData.Write((unsigned char)0);handler.handle(source,&duplicate);assert(count==wire.requests.size());}
 consumed=end;
 }
 assert(received.size()==256&&rounds==64&&wire.chunksReady==1);handler.recenterChunkRequests();assert(wire.requests.size()==256&&wire.chunksReady==1);
 std::cout<<"Actual request scheduler: 256 chunks / 64 simulated RTT rounds, max4 outstanding, nearest first, no duplicate sends on recenter or duplicate replies, READY exactly once PASS\n";
 // A recenter before outstanding old replies must neither duplicate nor strand requests.
 game->player->x-=1024;handler.recenterChunkRequests();size_t begin=wire.requests.size()-4;game->player->x-=1024;handler.recenterChunkRequests();assert(wire.requests.size()==begin+4);
 for(size_t i=begin;i<begin+4;++i){auto pos=wire.requests[i];ChunkDataPacket packet;packet.x=pos.first;packet.z=pos.second;for(int col=0;col<256;++col)packet.chunkData.Write((unsigned char)0);handler.handle(source,&packet);}
 assert(wire.requests.size()==begin+8);auto next=wire.requests[begin+4];assert(next.first==-130&&next.second==1);std::cout<<"Old-window replies refill newly centered window without duplicate requests PASS\n";
 ChunkDataPacket compressed;compressed.x=next.first;compressed.z=next.second;compressed.fullSnapshot=true;for(int col=0;col<256;++col)compressed.chunkData.Write((unsigned char)0);handler.handle(source,&compressed);assert(wire.requests.size()==begin+21);std::cout<<"Verified full compressed snapshot expands window to16; legacy stays4 PASS\n";
}
