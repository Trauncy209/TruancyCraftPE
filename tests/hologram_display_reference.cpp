#include "client/gui/screens/JavaInventoryScreen.h"
#include "client/renderer/Tesselator.h"
#include "client/renderer/Textures.h"
#include "network/java/JavaRichText.h"
#include "network/java/JavaDisplayRenderer.cpp"
#include "world/level/tile/Tile.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "util/Mth.h"
#include "AppPlatform_glfw.h"
#include <png.h>
#include <thread>
#include "network/java/JavaLocalPlayer.h"
#include "network/java/JavaLevel.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
class GameProbe:public Minecraft{public:GameProbe():Minecraft(){}};
class InventoryProbe:public JavaInventoryScreen{public:explicit InventoryProbe(int id=0):JavaInventoryScreen(id){}using Screen::buttons;};
static std::string sent;
extern "C" TcpeJavaSession* tcpe_java_connect(const char*,size_t,const char*,size_t,bool){return reinterpret_cast<TcpeJavaSession*>(1);}
extern "C" void tcpe_java_close(TcpeJavaSession*){}
extern "C" bool tcpe_java_command(TcpeJavaSession*,const char* text,size_t length){sent.assign(text,length);return true;}
int main(int argc,char**argv){
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
 auto* window=glfwCreateWindow(800,480,"Native inventory layout",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();
 AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);Font font(&options,"font/default8.png",&textures);font.setJavaTextEnabled(true);

 GLuint tx;glGenTextures(1,&tx);glBindTexture(GL_TEXTURE_2D,tx);const unsigned char pixel[]={255,255,255,255};glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
 Font::PackGlyph g;g.texture=tx;g.uw=g.vh=1;g.width=4;g.height=8;g.ascent=8;g.advance=6;font.setPackGlyph(0xe002,g,"test:font");
 auto frame=[&](unsigned alpha,bool anchor){glViewport(0,0,800,480);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,400,240,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glEnable2(GL_TEXTURE_2D);glDisable2(GL_ALPHA_TEST);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glTranslatef2(200,120,0);glScalef2(-40,-40,40);text(u8"§ztest:font;\ue002\n\ue002",font,200,3,alpha,0,anchor);glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char>p(800*480*4);glReadPixels(0,0,800,480,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;};
 auto sample=[](const std::vector<unsigned char>&p,int x,int y){return int(p[((479-y)*800+x)*4]);};
 auto full=frame(255,true);auto half=frame(127,true);auto name=frame(255,false);
 assert(sample(full,394,204)==255);assert(std::abs(sample(half,394,204)-127)<=1);
 assert(sample(full,402,218)==63);assert(std::abs(sample(half,402,218)-31)<=1);
 assert(sample(full,396,224)==255);assert(sample(full,396,244)==0);
 assert(sample(name,396,244)==255);assert(sample(name,396,204)==0);
 font.clearPackGlyphs();glDeleteTextures(1,&tx);std::cout<<"Display pixel regression PASS: 255/127 authoritative alpha, quarterRGB shadow, above-origin 10px line spacing, unchanged name anchor"<<std::endl;std::_Exit(0);
}
