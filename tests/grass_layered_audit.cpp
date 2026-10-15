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
int main(){
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_TRUE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);auto*w=glfwCreateWindow(480,480,"Grass layered audit",nullptr,nullptr);assert(w);glfwMakeContextCurrent(w);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);
 glViewport(0,0,480,480);glMatrixMode(GL_PROJECTION);glLoadIdentity2();gluPerspective(70,1,.05f,256);glMatrixMode(GL_MODELVIEW);glLoadIdentity2();glTranslatef2(0,0,-18);glRotatef2(23,1,0,0);glRotatef2(32,0,1,0);glTranslatef2(-8,-5,-8);glEnable2(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glEnable2(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.1);glDisable2(GL_BLEND);glShadeModel2(GL_FLAT);glEnable2(GL_TEXTURE_2D);glDisable2(GL_CULL_FACE);glClearColor(.2,.4,.7,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);textures.loadAndBindTexture("java-terrain.png");JavaGameSession session;std::vector<uint16_t> states(256*384);std::ifstream in("../grass-live-chunk--31--11.bin",std::ios::binary);in.read((char*)states.data(),states.size()*2);assert(in.good());TcpeJavaEvent event{};event.kind=TCPE_JAVA_CHUNK;event.x=-31;event.z=-11;event.min_y=-64;event.height=384;event.states=states.data();event.states_len=states.size();assert(session.chunks.apply(event));JavaLevel level(new MemoryLevelStorage(),session.chunks);GLuint vbos[6];glGenBuffers2(6,vbos);Chunk chunk(&level,-496,64,-176,16,0,vbos);chunk.rebuild();assert(chunk.getRenderChunkAtlas(0,0).vertexCount==0);assert(JavaBlockModels::layer(27648)==1&&JavaBlockModels::layer(27649)==1&&JavaBlockModels::layer(2051)==1);for(int layer=0;layer<3;++layer){if(layer==0)glDisable2(GL_ALPHA_TEST);else glEnable2(GL_ALPHA_TEST);for(int atlas=0;atlas<2;++atlas){textures.loadAndBindTexture(atlas?"java-terrain.png":"terrain.png");RenderList list;list.init(-496,64,-176);auto&rc=chunk.getRenderChunkAtlas(layer,atlas);std::cout<<"Layer "<<layer<<" vertices "<<rc.vertexCount<<std::endl;if(rc.vertexCount)list.addR(rc);list.render();}}glFinish();std::vector<unsigned char>p(480*480*4),f(p.size());glReadPixels(0,0,480,480,GL_RGBA,GL_UNSIGNED_BYTE,p.data());for(int y=0;y<480;++y)std::copy(p.begin()+y*1920,p.begin()+(y+1)*1920,f.begin()+(479-y)*1920);png_image image{};image.version=PNG_IMAGE_VERSION;image.width=image.height=480;image.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&image,"../grass-layered-audit.png",0,f.data(),0,nullptr));glfwSwapBuffers(w);glfwPollEvents();std::cout<<"Grass isolated GL="<<glGetString(GL_VERSION)<<" err="<<glGetError()<<std::endl;std::_Exit(0);
}
