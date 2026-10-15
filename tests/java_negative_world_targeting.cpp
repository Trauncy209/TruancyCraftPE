#include <fstream>
#include "network/java/JavaBlockModels.h"
#include "client/renderer/Chunk.h"
#include "client/renderer/RenderList.h"
#include "client/particle/JavaGustParticle.h"
#include "network/java/JavaIronGolemModel.h"
#include "client/renderer/entity/EntityRenderDispatcher.h"
#include "network/java/JavaEntityAdapter.h"
#include "network/java/JavaGameSession.h"
#include "network/java/JavaLevel.h"
#include "world/entity/player/Player.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "client/renderer/Tesselator.h"
#include "client/renderer/Textures.h"
#include "world/level/tile/Tile.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "util/Mth.h"
#include "AppPlatform_glfw.h"
#include <cassert>
#include <png.h>
#include <iostream>
extern "C" TcpeJavaSession* tcpe_java_connect(const char*,size_t,const char*,size_t,bool){return nullptr;}
extern "C" void tcpe_java_close(TcpeJavaSession*){}
extern "C" bool tcpe_java_command(TcpeJavaSession*,const char*,size_t){return true;}
int main(){assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_TRUE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);auto*w=glfwCreateWindow(320,180,"Native negative-coordinate targeting",nullptr,nullptr);assert(w);glfwMakeContextCurrent(w);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();JavaChunkStore store;std::vector<uint16_t> negative(256*384),positive(256*384);negative[(2*16+7)*384+68+64]=1;negative[(3*16+7)*384-20+64]=1;positive[(2*16+7)*384+68+64]=1;TcpeJavaEvent e{};e.kind=TCPE_JAVA_CHUNK;e.x=-31;e.z=-11;e.min_y=-64;e.height=384;e.states=negative.data();e.states_len=negative.size();assert(store.apply(e));e.x=0;e.z=0;e.states=positive.data();assert(store.apply(e));JavaLevel level(new MemoryLevelStorage(),store);assert(level.getLevelData()->getGeneratorVersion()==LGV_INFINITE);auto ray=[&](float x,float top,float z,float bottom){return level.clip(Vec3(x,top,z),Vec3(x,bottom,z),false,true);};level.getLevelData()->setGeneratorVersion(3);assert(!ray(-493.5,70.62,-168.5,67.5).isHit());assert(ray(2.5,70.62,7.5,67.5).isHit());level.getLevelData()->setGeneratorVersion(LGV_INFINITE);auto hit=ray(-493.5,70.62,-168.5,67.5);assert(hit.type==TILE&&hit.x==-494&&hit.y==68&&hit.z==-169);auto below=ray(-492.5,-18,-168.5,-22);assert(below.type==TILE&&below.y==-20);assert(!ray(-490.5,70.62,-168.5,67.5).isHit());assert(!ray(-10000.5,70.62,-168.5,67.5).isHit());assert(ray(2.5,70.62,7.5,67.5).isHit());std::cout<<"Actual Level::clip: previous storage-version3 rejects loaded negative target; corrected Java constructor hits exact negative XYZ and below-zero block; air/unloaded miss; positive control PASS"<<std::endl;std::_Exit(0);}
