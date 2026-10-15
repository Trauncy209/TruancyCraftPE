#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "network/java/JavaEntityAdapter.h"
#include "network/java/JavaItemIcons.h"
#include "network/java/JavaPackItemModels.h"
#include <set>
#include "network/java/JavaLevel.h"
#include "client/renderer/entity/EntityRenderDispatcher.h"
#include "client/renderer/Tesselator.h"
#include "client/renderer/Textures.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "world/entity/item/ItemEntity.h"
#include "world/item/Item.h"
#include "util/Mth.h"
#include <png.h>
#include <cassert>
#include <iostream>
#include <cstdlib>
class FixtureLevel:public JavaLevel{public:FixtureLevel(JavaGameSession& s):JavaLevel(new MemoryLevelStorage(),s.chunks){}float getBrightness(int,int,int)override{return 1.f;}};
static std::vector<std::string> loaded,chosen;
static GLuint lastTexture;
static TextureData textureData;
extern "C" const TextureData* __wrap__ZN8Textures23getTemporaryTextureDataEj(void*,unsigned){return &textureData;}
extern "C" bool __real__ZN13JavaItemIcons11renderWorldEP8TexturesRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEffPKN15JavaGameSession4ItemE(Textures*,const std::string&,float,float,const JavaGameSession::Item*);
extern "C" bool __wrap__ZN13JavaItemIcons11renderWorldEP8TexturesRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEffPKN15JavaGameSession4ItemE(Textures* t,const std::string& id,float size,float brightness,const JavaGameSession::Item* item){chosen.push_back(id);return __real__ZN13JavaItemIcons11renderWorldEP8TexturesRKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEffPKN15JavaGameSession4ItemE(t,id,size,brightness,item);}
extern "C" TextureId __wrap__ZN8Textures18loadAndBindTextureERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE(void*,const std::string& name){loaded.push_back(name);std::string path="outputs/tcpe-java-native/original-tcpe/data/images/"+name;png_image im{};im.version=PNG_IMAGE_VERSION;assert(png_image_begin_read_from_file(&im,path.c_str()));im.format=PNG_FORMAT_RGBA;std::vector<unsigned char> pixels(PNG_IMAGE_SIZE(im));assert(png_image_finish_read(&im,nullptr,pixels.data(),0,nullptr));textureData.w=im.width;textureData.h=im.height;GLuint texture;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,im.width,im.height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());png_image_free(&im);lastTexture=texture;return texture;}
int main(){auto getDisplay=(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");assert(getDisplay);EGLDisplay display=getDisplay(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,nullptr);assert(display!=EGL_NO_DISPLAY);assert(eglInitialize(display,nullptr,nullptr));assert(eglBindAPI(EGL_OPENGL_ES_API));EGLint attributes[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};EGLConfig config;EGLint count;assert(eglChooseConfig(display,attributes,&config,1,&count)&&count==1);EGLint pbuffer[]={EGL_WIDTH,256,EGL_HEIGHT,256,EGL_NONE};EGLSurface surface=eglCreatePbufferSurface(display,config,pbuffer);EGLint contextAttributes[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};EGLContext context=eglCreateContext(display,config,EGL_NO_CONTEXT,contextAttributes);assert(eglMakeCurrent(display,surface,surface,context));assert(TCPEGL3::initialize());Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();
 JavaGameSession session;std::vector<uint16_t> states(256*256,0);for(int x=-1;x<=1;++x)for(int z=-1;z<=1;++z){TcpeJavaEvent e{};e.kind=TCPE_JAVA_CHUNK;e.x=x;e.z=z;e.min_y=0;e.height=256;e.states=states.data();e.states_len=states.size();assert(session.chunks.apply(e));}Level* level=new FixtureLevel(session);EntityRenderDispatcher dispatcher;dispatcher.textures=new Textures(nullptr,nullptr);dispatcher.playerRotY=180;JavaGameSession::Entity snapshot{};snapshot.id=777;snapshot.kind="Item";snapshot.position={{0,64,0}};snapshot.droppedItem.resourceId="minecraft:compass";snapshot.droppedItem.originalId=-1;snapshot.droppedItem.count=1;session.entities[777]=snapshot;synchronizeJavaEntities(level,session);auto* item=dynamic_cast<ItemEntity*>(level->getEntity(777));assert(item);item->age=0;item->bobOffs=0;

 session.compass.dimension="minecraft:overworld";session.compass.defaultSpawn.valid=true;session.compass.defaultSpawn.dimension=session.compass.dimension;
 glViewport(0,0,256,256);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(-.6,.6,-.6,.6,-2,2);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glEnable2(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glEnable2(GL_TEXTURE_2D);glDisable2(GL_LIGHTING);glDisable2(GL_FOG);glClearColor(0,0,0,1);
 auto frame=[&](int age){session.entities[777]=snapshot;synchronizeJavaEntities(level,session);item->age=age;item->bobOffs=-.025f;glClearDepthf(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);chosen.clear();assert(renderJavaPresentation(item,dispatcher,0,-.1f,0,0));glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> pixels(256*256*4);glReadPixels(0,0,256,256,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());size_t lit=0;for(size_t i=0;i<pixels.size();i+=4)if(pixels[i]+pixels[i+1]+pixels[i+2]>30)++lit;assert(lit>100);return pixels;};
 const std::array<int,3> targets[]={{{100,64,0}},{{0,64,100}},{{-100,64,0}},{{0,64,-100}}};const char* expected[]={"minecraft:compass_24","minecraft:compass_00","minecraft:compass_08","minecraft:compass_16"};
 for(int i=0;i<4;++i){session.compass.defaultSpawn.position=targets[i];frame(0);assert(chosen.size()==1);std::cout<<"actual drop cardinal"<<i<<" -> "<<chosen[0]<<" expected "<<expected[i]<<std::endl;assert(chosen[0]==expected[i]);}
 session.compass.defaultSpawn.position=targets[0];auto initial=frame(0);auto initialId=chosen[0];assert(frame(20)!=initial);assert(chosen[0]!=initialId);
 snapshot.droppedItem.hasLodestoneTracker=true;snapshot.droppedItem.lodestoneTracked=false;snapshot.droppedItem.lodestoneTarget=session.compass.defaultSpawn;snapshot.droppedItem.lodestoneTarget.position=targets[2];frame(0);assert(chosen[0]=="minecraft:compass_08");
 snapshot.droppedItem.resourceId="minecraft:recovery_compass";session.compass.lastDeath=session.compass.defaultSpawn;session.compass.lastDeath.position=targets[1];frame(0);assert(chosen[0]=="minecraft:recovery_compass_00");
 session.compass.lastDeath.dimension="minecraft:the_nether";std::set<std::string> spinning;for(int age=1;age<40;++age){frame(age);auto id=chosen[0];frame(age);assert(chosen[0]==id);spinning.insert(id);}assert(spinning.size()>3);
 // A custom resource-pack item model remains authoritative over vanilla needles.
 JavaPackItemModels::Model model;JavaPackItemModels::Quad quad;quad.texture=lastTexture;quad.vertices={{{{0,0,8,0,1}},{{16,0,8,1,1}},{{16,16,8,1,0}},{{0,16,8,0,0}}}};model.quads.push_back(quad);JavaPackItemModels::models()["fixture:compass"]=model;snapshot.droppedItem.itemModel="fixture:compass";frame(0);assert(chosen.empty());
 std::cout<<"Actual offscreen dropped compass:cardinal/spin/lodestone/recovery/dimension,stable-tick and custom pack precedence PASS (software EGL pbuffer,no GLFW/window)"<<std::endl;std::_Exit(0);
}
