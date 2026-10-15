#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "network/java/JavaEntityAdapter.h"
#include "network/java/JavaItemIcons.h"
#include "network/java/JavaLevel.h"
#include "client/renderer/entity/EntityRenderDispatcher.h"
#include "client/renderer/Tesselator.h"
#include "client/renderer/Textures.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "world/entity/item/ItemEntity.h"
#include "world/level/LightLayer.h"
#include "world/level/dimension/Dimension.h"
#include "world/item/Item.h"
#include "util/Mth.h"
#include <png.h>
#include <cassert>
#include <iostream>
#include <cstdlib>
static std::vector<std::string> loaded;
extern "C" TextureId __wrap__ZN8Textures18loadAndBindTextureERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE(void*,const std::string& name){loaded.push_back(name);std::string path="outputs/tcpe-java-native/original-tcpe/data/images/"+name;png_image im{};im.version=PNG_IMAGE_VERSION;assert(png_image_begin_read_from_file(&im,path.c_str()));im.format=PNG_FORMAT_RGBA;std::vector<unsigned char> pixels(PNG_IMAGE_SIZE(im));assert(png_image_finish_read(&im,nullptr,pixels.data(),0,nullptr));GLuint texture;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,im.width,im.height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());png_image_free(&im);return texture;}
int main(){auto getDisplay=(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");assert(getDisplay);EGLDisplay display=getDisplay(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,nullptr);assert(display!=EGL_NO_DISPLAY);assert(eglInitialize(display,nullptr,nullptr));assert(eglBindAPI(EGL_OPENGL_ES_API));EGLint attributes[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};EGLConfig config;EGLint count;assert(eglChooseConfig(display,attributes,&config,1,&count)&&count==1);EGLint pbuffer[]={EGL_WIDTH,256,EGL_HEIGHT,256,EGL_NONE};EGLSurface surface=eglCreatePbufferSurface(display,config,pbuffer);EGLint contextAttributes[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};EGLContext context=eglCreateContext(display,config,EGL_NO_CONTEXT,contextAttributes);assert(eglMakeCurrent(display,surface,surface,context));assert(TCPEGL3::initialize());Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();

 JavaGameSession session;std::vector<uint16_t> states(256*256,0);std::vector<uint8_t> sky(128*256),block(128*256),flags(16,3);
 auto lighting=[&](int skyValue,int blockValue){std::fill(sky.begin(),sky.end(),uint8_t(skyValue*17));std::fill(block.begin(),block.end(),uint8_t(blockValue*17));TcpeJavaEvent e{};e.kind=TCPE_JAVA_CHUNK;e.x=e.z=0;e.min_y=0;e.height=256;e.states=states.data();e.states_len=states.size();e.sky_light=sky.data();e.sky_light_len=sky.size();e.block_light=block.data();e.block_light_len=block.size();e.light_sections=flags.data();e.light_sections_len=flags.size();assert(session.chunks.apply(e));};lighting(0,0);
 auto* level=new JavaLevel(new MemoryLevelStorage(),session.chunks);EntityRenderDispatcher dispatcher;dispatcher.textures=new Textures(nullptr,nullptr);dispatcher.playerRotY=180;
 for(auto pair:{std::make_pair(777,"Allay"),std::make_pair(778,"Bee")}){JavaGameSession::Entity e{};e.id=pair.first;e.kind=pair.second;e.position={{.5,64,.5}};session.entities[e.id]=e;}synchronizeJavaEntities(level,session);
 glViewport(0,0,256,256);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(-.8,.8,-.2,1.1,-2,2);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glRotatef2(25,0,1,0);glEnable2(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glEnable2(GL_TEXTURE_2D);glDisable2(GL_LIGHTING);glDisable2(GL_FOG);glClearColor(0,0,0,1);
 auto frame=[&](int entityId,int skyValue,int blockValue){lighting(skyValue,blockValue);level->synchronize(session.chunks);assert(level->getBrightness(LightLayer::Sky,0,64,0)==skyValue);assert(level->getBrightness(LightLayer::Block,0,64,0)==blockValue);Entity* entity=static_cast<Level*>(level)->getEntity(entityId);assert(entity);glClearDepthf(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);assert(renderJavaPresentation(entity,dispatcher,0,0,0,0));glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> pixels(256*256*4);glReadPixels(0,0,256,256,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());return pixels;};
 auto sum=[](const std::vector<unsigned char>& pixels){unsigned long long total=0;for(size_t i=0;i<pixels.size();i+=4)total+=pixels[i]+pixels[i+1]+pixels[i+2];return total;};
 auto allayDark=frame(777,0,0),allaySky=frame(777,15,0),allayBlock=frame(777,0,15),beeDark=frame(778,0,0),beeSky=frame(778,15,0),beeBlock=frame(778,0,15);std::cout<<"Actual Allay RGB sums dark="<<sum(allayDark)<<" sky15="<<sum(allaySky)<<" block15="<<sum(allayBlock)<<std::endl;std::cout<<"Actual Bee RGB sums dark="<<sum(beeDark)<<" sky15="<<sum(beeSky)<<" block15="<<sum(beeBlock)<<std::endl;
 assert(sum(allayDark)>1000);assert(allayDark==allaySky&&allaySky==allayBlock);assert(sum(beeSky)>sum(beeDark)*2);assert(beeSky==beeBlock);assert(loaded.size()==6);for(size_t i=0;i<loaded.size();++i)assert(loaded[i]==(i<3?"mob/java-allay.png":"mob/java-bee.png"));
 const int skyDarken=level->skyDarken;level->skyDarken=15;assert(frame(777,15,0)==allayDark);assert(frame(778,15,0)==beeDark);assert(frame(778,0,15)==beeBlock);level->skyDarken=skyDarken;
 const float maximum=level->dimension->brightnessRamp[15];level->dimension->brightnessRamp[15]=maximum*.5f;auto transferred=frame(777,0,0);assert(sum(transferred)<sum(allayDark)*.6&&sum(transferred)>sum(allayDark)*.4);level->dimension->brightnessRamp[15]=maximum;
 auto save=[](const std::vector<unsigned char>& pixels,const char* path){std::vector<unsigned char> flip(pixels.size());for(int y=0;y<256;++y)std::copy(pixels.begin()+y*1024,pixels.begin()+(y+1)*1024,flip.begin()+(255-y)*1024);png_image out{};out.version=PNG_IMAGE_VERSION;out.width=out.height=256;out.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&out,path,0,flip.data(),0,nullptr));};save(allayDark,"work/allay-light-private/allay-dark.png");save(beeDark,"work/allay-light-private/bee-dark.png");save(beeSky,"work/allay-light-private/bee-sky15.png");
 std::cout<<"Actual entity renderer Allay blocklight15, sky/block plane combinations, Bee environmental-light positivecontrol PASS (Mesa software EGL pbuffer,no GLFW/window)"<<std::endl;std::_Exit(0);
}
