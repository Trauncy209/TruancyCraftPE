#include "network/java/JavaWindChargeModel.h"
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
#include "client/renderer/Chunk.h"
int main(){
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_TRUE);glfwWindowHint(GLFW_FOCUS_ON_SHOW,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
 auto* window=glfwCreateWindow(320,240,"TCPE Java chunk air transition",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();
 JavaGameSession session;std::vector<uint16_t> states(4096,0);states[8*16*16+8*16+8]=1;
 TcpeJavaEvent e{};e.kind=TCPE_JAVA_CHUNK;e.height=16;e.states=states.data();e.states_len=states.size();assert(session.chunks.apply(e));JavaLevel level(new MemoryLevelStorage(),session.chunks);
 GLuint buffers[6];glGenBuffers(6,buffers);Chunk chunk(&level,0,0,0,16,0,buffers);chunk.setDirty();chunk.rebuild();assert(!chunk.isEmpty());
 int vertices=0;for(int l=0;l<3;++l)vertices+=chunk.renderChunk[l].vertexCount+chunk.renderChunkAlt[l].vertexCount;assert(vertices>0);
 states.assign(4096,0);e.states=states.data();assert(session.chunks.apply(e));level.synchronize(session.chunks);chunk.setDirty();chunk.rebuild();assert(chunk.isEmpty());
 for(int l=0;l<3;++l){assert(chunk.empty[l]);assert(chunk.renderChunk[l].vertexCount==0&&chunk.renderChunkAlt[l].vertexCount==0);}
 assert(!chunk.skyLit);
 states[8*16*16+8*16+8]=1;assert(session.chunks.apply(e));level.synchronize(session.chunks);chunk.setDirty();chunk.rebuild();assert(!chunk.isEmpty());
 int restored=0;for(int l=0;l<3;++l)restored+=chunk.renderChunk[l].vertexCount+chunk.renderChunkAlt[l].vertexCount;assert(restored==vertices);
 assert(glGetError()==GL_NO_ERROR);glfwSwapBuffers(window);glfwPollEvents();
 std::cout<<"Visible GPU Java chunk: nonair-to-air clears all layer counts, air-to-nonair restores original geometry PASS"<<std::endl;std::_Exit(0);
}
