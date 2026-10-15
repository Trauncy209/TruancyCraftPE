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
#include "world/entity/projectile/Arrow.h"
#include "client/renderer/entity/EntityRenderer.h"
int main(){
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_TRUE);glfwWindowHint(GLFW_FOCUS_ON_SHOW,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
 auto* window=glfwCreateWindow(400,400,"TCPE Java arrow verification",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();
 AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);JavaGameSession session;JavaLevel level(new MemoryLevelStorage(),session.chunks);
 JavaGameSession::Entity snapshot{};snapshot.id=901;snapshot.kind="Arrow";snapshot.position={{3,10,4}};snapshot.hitbox={{.5,.5}};snapshot.yaw=30;snapshot.pitch=15;session.entities[901]=snapshot;synchronizeJavaEntities(&level,session);
 auto* entity=static_cast<Level&>(level).getEntity(901);assert(dynamic_cast<Arrow*>(entity));assert(entity->x==3&&entity->y==10&&entity->z==4);for(int i=0;i<40;++i)entity->tick();assert(entity->x==3&&entity->y==10&&entity->z==4);snapshot.position={{5,12,6}};session.entities[901]=snapshot;synchronizeJavaEntities(&level,session);assert(entity->x==5&&entity->y==12&&entity->z==6);
 snapshot.id=902;snapshot.kind="SpectralArrow";session.entities[902]=snapshot;synchronizeJavaEntities(&level,session);assert(dynamic_cast<Arrow*>(static_cast<Level&>(level).getEntity(902)));
 EntityRenderDispatcher dispatcher;dispatcher.textures=&textures;dispatcher.level=&level;dispatcher.options=&options;
 glViewport(0,0,400,400);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(-.8f,.8f,-.8f,.8f,-4,4);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glEnable2(GL_DEPTH_TEST);glEnable2(GL_TEXTURE_2D);glDisable2(GL_LIGHTING);glDisable2(GL_CULL_FACE);glColor4f2(1,1,1,1);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 assert(!renderJavaPresentation(entity,dispatcher,0,0,0,1));dispatcher.getRenderer(entity)->render(entity,0,0,0,30,1);glFinish();assert(glGetError()==GL_NO_ERROR);
 std::vector<unsigned char> pixels(400*400*4);glReadPixels(0,0,400,400,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());size_t lit=0;for(size_t i=0;i<pixels.size();i+=4)if(pixels[i]+pixels[i+1]+pixels[i+2]>30)++lit;assert(lit>100);
 glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);auto* spectral=static_cast<Level&>(level).getEntity(902);dispatcher.getRenderer(spectral)->render(spectral,0,0,0,30,1);glFinish();std::vector<unsigned char> gold(pixels.size());glReadPixels(0,0,400,400,GL_RGBA,GL_UNSIGNED_BYTE,gold.data());assert(gold!=pixels);assert(glGetError()==GL_NO_ERROR);
 std::vector<unsigned char> flipped(pixels.size());for(int row=0;row<400;++row)std::copy(pixels.begin()+row*1600,pixels.begin()+(row+1)*1600,flipped.begin()+(399-row)*1600);png_image image{};image.version=PNG_IMAGE_VERSION;image.width=image.height=400;image.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&image,"../native-java-arrow-proof.png",0,flipped.data(),0,nullptr));
 glfwSwapBuffers(window);glfwPollEvents();std::cout<<"Java arrow: arrow geometry routing, server-owned position/no legacy physics, visible texture PASS; pixels="<<lit<<std::endl;std::_Exit(0);
}
