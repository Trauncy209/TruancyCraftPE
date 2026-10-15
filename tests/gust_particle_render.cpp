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
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_TRUE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);auto* window=glfwCreateWindow(400,400,"TCPE gust verification",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);JavaGameSession session;JavaLevel level(new MemoryLevelStorage(),session.chunks);
 JavaGustParticle particle(&textures,&level,0,0,0);assert(JavaGustParticle::frame(0,12)==0&&JavaGustParticle::frame(12,12)==11);
 glViewport(0,0,400,400);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(-1.2f,1.2f,-1.2f,1.2f,-4,4);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable2(GL_CULL_FACE);glEnable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);glBlendFunc2(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glClearColor(0,0,0,1);
 auto frame=[&](){glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);particle.render(Tesselator::instance,0,1,1,0,0,0);glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> pixels(400*400*4);glReadPixels(0,0,400,400,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());glfwSwapBuffers(window);glfwPollEvents();return pixels;};
 auto first=frame();for(int i=0;i<6;++i)particle.tick();auto second=frame();assert(first!=second);size_t lit=0;for(size_t i=0;i<second.size();i+=4)if(second[i]+second[i+1]+second[i+2]>30)++lit;assert(lit>100);int ticks=6;while(!particle.removed&&ticks<20){particle.tick();++ticks;}assert(ticks>=13&&ticks<=16);
 for(bool large:{false,true}){ParticleEngine engine(&level,&textures);engine.add(new JavaGustEmitter(&engine,&textures,&level,0,0,0,large));for(int i=0;i<(large?7:3);++i)engine.tick();assert(engine.countParticles()==(large?"21":"3"));for(int i=0;i<20;++i)engine.tick();assert(engine.countParticles()=="0");}
 std::vector<unsigned char> flipped(second.size());for(int row=0;row<400;++row)std::copy(second.begin()+row*1600,second.begin()+(row+1)*1600,flipped.begin()+(399-row)*1600);png_image image{};image.version=PNG_IMAGE_VERSION;image.width=image.height=400;image.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&image,"../native-gust-proof.png",0,flipped.data(),0,nullptr));std::cout<<"Official gust frames/animation/lifetime, small/large emitter counts and cleanup PASS; pixels="<<lit<<std::endl;std::_Exit(0);
}
