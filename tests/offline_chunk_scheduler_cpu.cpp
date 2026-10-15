#include "client/Minecraft.h"
#include "client/renderer/LevelRenderer.h"
#include "client/renderer/Chunk.h"
#include "client/renderer/DirtyChunkSorter.h"
#include "client/player/LocalPlayer.h"
#include "network/java/JavaLevel.h"
#include "world/level/LevelSettings.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "util/Mth.h"
#include <cassert>
#include <iostream>
#include <cstdlib>
#include <algorithm>
static std::vector<Chunk*> created,rebuilt;
static bool exhausted=false;
static unsigned budgetCalls=0;
static int farBudget=-1;
extern "C" void __wrap_glGenBuffers(GLsizei n,GLuint* names){static GLuint next=1;for(int i=0;i<n;++i)names[i]=next++;}
extern "C" void __wrap_glBindBuffer(GLenum,GLuint){}
extern "C" void __wrap_glBufferData(GLenum,GLsizeiptr,const void*,GLenum){}
extern "C" void __real__ZN5ChunkC1EP5LeveliiiiiPj(Chunk*,Level*,int,int,int,int,int,GLuint*);
extern "C" void __wrap__ZN5ChunkC1EP5LeveliiiiiPj(Chunk* chunk,Level* level,int x,int y,int z,int size,int lists,GLuint* buffers){__real__ZN5ChunkC1EP5LeveliiiiiPj(chunk,level,x,y,z,size,lists,buffers);created.push_back(chunk);}
extern "C" void __wrap__ZN5Chunk7rebuildEv(Chunk* chunk){if(!chunk->isDirty())return;rebuilt.push_back(chunk);}
extern "C" float __wrap__ZN9Stopwatch12stopContinueEv(void*){++budgetCalls;return exhausted||(farBudget>=0&&budgetCalls>unsigned(farBudget))?.02f:0.f;}
class Probe:public Minecraft{public:Probe():Minecraft(){}};
static size_t remaining(){return std::count_if(created.begin(),created.end(),[](Chunk* c){return c->isDirty();});}
static void run(bool java){auto* game=new Probe;game->options.viewDistance=3;JavaChunkStore store;Level* level=java?static_cast<Level*>(new JavaLevel(new MemoryLevelStorage(),store)):new Level(new MemoryLevelStorage(),"CPU scheduler",LevelSettings(1,GameType::Creative,LGV_ORIGINAL),LGV_ORIGINAL);game->level=level;game->player=new LocalPlayer(game,level,nullptr,0,false);game->player->setPos(8,8,8);auto* renderer=new LevelRenderer(game);game->levelRenderer=renderer;created.clear();rebuilt.clear();renderer->setLevel(level);assert(!created.empty());const size_t total=created.size();exhausted=true;assert(!renderer->updateDirtyChunks(game->player,false));const size_t completed=rebuilt.size();assert(budgetCalls>0);for(Chunk* c:rebuilt)assert(c->distanceToSqr(game->player)<=1024.f);assert(completed>0&&completed<total);exhausted=false;bool complete=false;unsigned passes=0;while(passes++<total+10){if(renderer->updateDirtyChunks(game->player,false)){complete=true;break;}}size_t dirty=remaining();std::cout<<(java?"Java":"Offline")<<" total="<<total<<" initialCompleted="<<completed<<" drainPasses="<<passes<<" queueComplete="<<complete<<" remainingDirty="<<dirty<<std::endl;for(Chunk* c:created)if(c->isDirty())assert(c->empty[0]&&c->renderChunk[0].vertexCount==0);assert(complete&&dirty==0);
 // Independent top-k oracle: sort ALL eligible jobs by the actual production comparator.
 created.clear();rebuilt.clear();renderer->allChanged();std::vector<Chunk*> expected;for(Chunk* c:created)if(c->distanceToSqr(game->player)>1024.f)expected.push_back(c);std::sort(expected.begin(),expected.end(),DirtyChunkSorter(game->player));std::reverse(expected.begin(),expected.end());assert(expected.size()>3);renderer->updateDirtyChunks(game->player,false);std::vector<Chunk*> far;for(Chunk* c:rebuilt)if(c->distanceToSqr(game->player)>1024.f)far.push_back(c);assert(far.size()==3);for(size_t i=0;i<3;++i)assert(far[i]==expected[i]);
 // Exhaustion after one far job must retain the other selected jobs as well.
 created.clear();rebuilt.clear();renderer->allChanged();budgetCalls=0;farBudget=1;assert(!renderer->updateDirtyChunks(game->player,false));size_t farCompleted=0;for(Chunk* c:rebuilt)if(c->distanceToSqr(game->player)>1024.f)++farCompleted;assert(farCompleted==1);farBudget=-1;complete=false;for(size_t i=0;i<created.size()+10;++i)if(renderer->updateDirtyChunks(game->player,false)){complete=true;break;}assert(complete&&remaining()==0);
 // Normal mode still eventually processes hidden jobs, one per pass.
 created.clear();rebuilt.clear();renderer->allChanged();for(Chunk* c:created)c->visible=false;complete=false;for(size_t i=0;i<created.size()+10;++i)if(renderer->updateDirtyChunks(game->player,false)){complete=true;break;}assert(complete&&remaining()==0);
 // Force mode rebuilds visible chunks but retains hidden jobs for later visibility.
 created.clear();rebuilt.clear();renderer->allChanged();created[0]->visible=false;assert(!renderer->updateDirtyChunks(game->player,true));assert(remaining()==1&&created[0]->isDirty());renderer->setAllVisible();assert(renderer->updateDirtyChunks(game->player,true));assert(remaining()==0);std::cout<<(java?"Java":"Offline")<<" zero/partial-budget retention, exact nearest3 order, hidden normal+force retry PASS"<<std::endl;
 // Leak fixture-owned levels/renderers intentionally; no application teardown/init.
}
int main(){std::cout.setf(std::ios::unitbuf);Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();run(false);run(true);std::cout<<"CPU actual LevelRenderer.updateDirtyChunks regression PASS; no window/context/network"<<std::endl;std::_Exit(0);}
