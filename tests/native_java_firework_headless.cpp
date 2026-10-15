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
#include "headless_egl.h"
int main(){
 NativeHeadlessEGL offscreen(400,400);Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();
 AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);JavaGameSession session;std::vector<uint16_t> air(4096,0);std::vector<uint8_t> sunlight(2048,255);uint8_t flag=1;TcpeJavaEvent chunk{};chunk.kind=TCPE_JAVA_CHUNK;chunk.height=16;chunk.states=air.data();chunk.states_len=air.size();chunk.sky_light=sunlight.data();chunk.sky_light_len=sunlight.size();chunk.light_sections=&flag;chunk.light_sections_len=1;assert(session.chunks.apply(chunk));JavaLevel level(new MemoryLevelStorage(),session.chunks);
 JavaGameSession::Entity e{};e.id=2101;e.kind="FireworkRocket";e.position={{2,10,3}};e.droppedItem.count=1;e.droppedItem.resourceId="minecraft:firework_rocket";e.droppedItem.originalId=-1;
 EntityRenderDispatcher dispatcher;dispatcher.textures=&textures;dispatcher.level=&level;dispatcher.playerRotY=dispatcher.playerRotX=0;
 glViewport(0,0,400,400);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(-.5f,.5f,-.5f,.5f,-4,4);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glEnable2(GL_DEPTH_TEST);glClearColor(0,0,0,1);
 auto render=[&](const char* name,bool visible){session.entities[e.id]=e;synchronizeJavaEntities(&level,session);auto* entity=static_cast<Level&>(level).getEntity(e.id);assert(entity&&entity->getEntityTypeId()==125&&!entity->isMob());for(int i=0;i<30;++i)entity->tick();assert(entity->x==2&&entity->y==10&&entity->z==3);assert(entity->shouldRenderAtSqrDistance(1)==(e.fireworkAttachedTo<0));assert(!entity->shouldRenderAtSqrDistance(4096));
 glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glColor4f2(.2f,.3f,.4f,.5f);glDisable2(GL_BLEND);glEnable2(GL_ALPHA_TEST);glEnable2(GL_CULL_FACE);assert(renderJavaPresentation(entity,dispatcher,0,0,0,1));assert(!glIsEnabled(GL_BLEND)&&glIsEnabled(GL_ALPHA_TEST)&&glIsEnabled(GL_CULL_FACE));GLfloat color[4];glGetFloatv(GL_CURRENT_COLOR,color);assert(std::abs(color[0]-.2f)<.0001f);glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> pixels(400*400*4);glReadPixels(0,0,400,400,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());size_t lit=0;for(size_t i=0;i<pixels.size();i+=4)if(pixels[i]+pixels[i+1]+pixels[i+2]>30)++lit;assert(visible?lit>100:lit==0);
 std::vector<unsigned char> flipped(pixels.size());for(int row=0;row<400;++row)std::copy(pixels.begin()+row*1600,pixels.begin()+(row+1)*1600,flipped.begin()+(399-row)*1600);png_image image{};image.version=PNG_IMAGE_VERSION;image.width=image.height=400;image.format=PNG_FORMAT_RGBA;std::string file="../headless-firework-"+std::string(name)+".png";assert(png_image_write_to_file(&image,file.c_str(),0,flipped.data(),0,nullptr));return pixels;};
 auto normal=render("normal",true);e.fireworkShotAtAngle=true;auto angled=render("angled",true);assert(normal!=angled);e.fireworkAttachedTo=42;render("attached",false);e.fireworkAttachedTo=-1;e.droppedItem={};render("empty",false);e.droppedItem.count=1;e.droppedItem.resourceId="minecraft:firework_rocket";e.droppedItem.originalId=-1;assert(render("restored",true)==angled);
 std::cout<<"HEADLESS EGL pbuffer: real FireworkRocket non-mob item, shot-at-angle transform, attached suppression, explicit Empty, restored metadata, server-owned motion/distance and GL state PASS; no window created"<<std::endl;std::_Exit(0);
}
