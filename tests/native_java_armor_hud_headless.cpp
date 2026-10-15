#include "client/gui/screens/JavaInventoryScreen.h"
#include "client/gui/Gui.h"
#include "client/player/LocalPlayer.h"
#include "network/java/JavaLevel.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "client/renderer/Tesselator.h"
#include "client/renderer/Textures.h"
#include "network/java/JavaRichText.h"
#include "world/level/tile/Tile.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "util/Mth.h"
#include "AppPlatform_glfw.h"
#include <png.h>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include "headless_egl.h"
static int logicalWidth,armorDraws,foodDraws;
extern "C" void __real__ZN12GuiComponent4blitEiiiiiiii(void*,int,int,int,int,int,int,int,int);
extern "C" void __wrap__ZN12GuiComponent4blitEiiiiiiii(void* p,int x,int y,int sx,int sy,int w,int h,int sw,int sh){if(sy==0&&sx==18){++foodDraws;assert(x>=0&&x+w<=logicalWidth);assert(y>=0&&y+h<=240);}if(sy==9){++armorDraws;assert(x>=0&&x+w<=logicalWidth);assert(y>=0&&y+h<=240);}__real__ZN12GuiComponent4blitEiiiiiiii(p,x,y,sx,sy,w,h,sw,sh);}
class GameProbe:public Minecraft{public:GameProbe():Minecraft(){}};
class InventoryProbe:public JavaInventoryScreen{public:InventoryProbe():JavaInventoryScreen(0){}using Screen::buttons;};
int main(int argc,char**argv){
 NativeHeadlessEGL offscreen(400,240);
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();
 AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);Font font(&options,"font/default8.png",&textures);font.setJavaTextEnabled(true);
 auto* game=new GameProbe();game->textures=&textures;game->font=&font;game->javaSession=new JavaGameSession();

 game->level=new JavaLevel(new MemoryLevelStorage(),game->javaSession->chunks);game->player=new LocalPlayer(game,game->level,nullptr,0,false);
 Gui gui(game);game->javaSession->spawned=true;game->javaSession->experienceLevel=42;
 game->javaSession->health=20;game->javaSession->maxHealth=20;game->javaSession->absorption=0;game->javaSession->armor=20;game->javaSession->food=20;
 for(int width:{400,200,167,166,160,120,80}){
  logicalWidth=width;armorDraws=0;foodDraws=0;glViewport(0,0,400,240);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,width,240,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable2(GL_DEPTH_TEST);glEnable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  gui.renderJavaVitals(width,240);glFinish();assert(glGetError()==GL_NO_ERROR);assert(armorDraws==10);assert(foodDraws==10);
  std::vector<unsigned char> pixels(400*240*4),flipped(pixels.size());glReadPixels(0,0,400,240,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());int colored=0;for(size_t i=0;i<pixels.size();i+=4)if(pixels[i]||pixels[i+1]||pixels[i+2])++colored;assert(colored>0);for(int row=0;row<240;++row)std::copy(pixels.begin()+row*400*4,pixels.begin()+(row+1)*400*4,flipped.begin()+(239-row)*400*4);
  png_image image{};image.version=PNG_IMAGE_VERSION;image.width=400;image.height=240;image.format=PNG_FORMAT_RGBA;std::string name="/home/truancy/Documents/Codex/2026-10-13/referenced-chatgpt-conversation-this-is-an/work/responsive-hud-private/width-"+std::to_string(width)+".png";assert(png_image_write_to_file(&image,name.c_str(),0,flipped.data(),0,nullptr));std::cout<<"width "<<width<<" ten armor glyphs bounded and rendered pixels "<<colored<<std::endl;
 }
 std::cout<<"Actual native Java armor HUD offscreen GLES3 bounds PASS"<<std::endl;std::_Exit(0);
}
