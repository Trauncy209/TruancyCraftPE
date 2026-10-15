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
class GameProbe:public Minecraft{public:GameProbe():Minecraft(){}};
class InventoryProbe:public JavaInventoryScreen{public:InventoryProbe():JavaInventoryScreen(0){}using Screen::buttons;};
int main(int argc,char**argv){
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
 auto* window=glfwCreateWindow(800,480,"Native inventory layout",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();
 AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);Font font(&options,"font/default8.png",&textures);font.setJavaTextEnabled(true);
 auto* game=new GameProbe();game->textures=&textures;game->font=&font;game->javaSession=new JavaGameSession();

 game->level=new JavaLevel(new MemoryLevelStorage(),game->javaSession->chunks);game->player=new LocalPlayer(game,game->level,nullptr,0,false);
 Gui gui(game);game->javaSession->spawned=true;game->javaSession->experienceLevel=42;
 glViewport(0,0,800,480);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,400,240,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable2(GL_DEPTH_TEST);glEnable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 for(double progress:{0.,0.5,1.,-1.,2.}) {
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);game->javaSession->experienceProgress=progress;gui.renderJavaExperience(400,221);glFinish();assert(glGetError()==GL_NO_ERROR);
  std::vector<unsigned char> row(800*4);glReadPixels(0,480-430,800,1,GL_RGBA,GL_UNSIGNED_BYTE,row.data());
  int green=0;for(int x=0;x<800;++x)if(row[x*4+1]>200&&row[x*4]<180)++green;
  int expected=int(182*std::max(0.,std::min(1.,progress)))*2;assert(std::abs(green-expected)<=2);
 }
 glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);game->javaSession->health=35;game->javaSession->maxHealth=40;game->javaSession->absorption=5;game->javaSession->food=13;gui.renderJavaVitals(400,240);game->javaSession->experienceProgress=0.75;gui.renderJavaExperience(400,221);glFinish();
 std::vector<unsigned char> pixels(800*480*4),flipped(pixels.size());glReadPixels(0,0,800,480,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());for(int row=0;row<480;++row)std::copy(pixels.begin()+row*800*4,pixels.begin()+(row+1)*800*4,flipped.begin()+(479-row)*800*4);
 png_image image{};image.version=PNG_IMAGE_VERSION;image.width=800;image.height=480;image.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&image,argv[1],0,flipped.data(),0,nullptr));
 std::cout<<"Actual native experience HUD: authoritative progress fill, clamp and level render PASS"<<std::endl;std::_Exit(0);
}
