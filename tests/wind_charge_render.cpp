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
int main(){
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_TRUE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
 auto* window=glfwCreateWindow(400,400,"TCPE wind charge verification",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();
 AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);JavaGameSession session;
 JavaLevel level(new MemoryLevelStorage(),session.chunks);
 for(auto pair:{std::make_pair(1001,"WindCharge"),std::make_pair(1002,"BreezeWindCharge")}){JavaGameSession::Entity e{};e.id=pair.first;e.kind=pair.second;e.position={{0,10,0}};session.entities[e.id]=e;}
 synchronizeJavaEntities(&level,session);EntityRenderDispatcher dispatcher;dispatcher.textures=&textures;
 auto* entity=static_cast<Level&>(level).getEntity(1001);assert(entity&&entity->getEntityTypeId()==126);assert(static_cast<Level&>(level).getEntity(1002)->getEntityTypeId()==126);
 assert(std::abs(JavaWindChargeModel::scroll(10)-.3f)<.00001f);assert(std::abs(JavaWindChargeModel::scroll(40)-.2f)<.00001f);
 glViewport(0,0,400,400);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(-.8f,.8f,-.8f,.8f,-4,4);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glRotatef2(20,1,0,0);glEnable2(GL_DEPTH_TEST);glClearColor(0,0,0,1);
 auto frame=[&](){glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glColor4f2(.2f,.3f,.4f,.5f);glDisable2(GL_BLEND);glEnable2(GL_ALPHA_TEST);assert(renderJavaPresentation(entity,dispatcher,0,0,0,0));assert(!glIsEnabled(GL_BLEND)&&glIsEnabled(GL_ALPHA_TEST));GLfloat color[4];glGetFloatv(GL_CURRENT_COLOR,color);assert(std::abs(color[0]-.2f)<.0001f);glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> pixels(400*400*4);glReadPixels(0,0,400,400,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());glfwSwapBuffers(window);glfwPollEvents();return pixels;};
 auto first=frame();for(int i=0;i<8;++i)entity->tick();auto second=frame();assert(first!=second);size_t lit=0;for(size_t i=0;i<second.size();i+=4)if(second[i]+second[i+1]+second[i+2]>40)++lit;assert(lit>100);size_t shaded=0;for(size_t i=0;i<second.size();i+=4)if(second[i]>20&&second[i]<245&&second[i+1]>20)++shaded;assert(shaded>20);
 std::vector<unsigned char> flipped(second.size());for(int row=0;row<400;++row)std::copy(second.begin()+row*1600,second.begin()+(row+1)*1600,flipped.begin()+(399-row)*1600);png_image image{};image.version=PNG_IMAGE_VERSION;image.width=image.height=400;image.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&image,"../native-wind-charge-proof.png",0,flipped.data(),0,nullptr));
 std::cout<<"Wind charge kind routing, official textured geometry, animation, UV scroll and GL state restoration PASS; pixels="<<lit<<std::endl;std::_Exit(0);
}
