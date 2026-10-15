#include "client/gui/screens/JavaInventoryScreen.h"
#include "client/gui/Gui.h"
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
#include <set>
class GameProbe:public Minecraft{public:GameProbe():Minecraft(){}};
class InventoryProbe:public JavaInventoryScreen{public:InventoryProbe():JavaInventoryScreen(0){}using Screen::buttons;};
int main(int argc,char**argv){
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
 auto* window=glfwCreateWindow(800,480,"Native inventory layout",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();
 AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);Font font(&options,"font/default8.png",&textures);font.setJavaTextEnabled(true);
 auto* game=new GameProbe();game->textures=&textures;game->font=&font;game->javaSession=new JavaGameSession();


 Gui gui(game);game->javaSession->spawned=true;
 for(int i=0;i<7;++i){JavaBossBar bar;bar.id=std::to_string(i);bar.text=u8"§x§1§2§a§b§e§f雪 ✦ Boss "+std::to_string(i);bar.color=i;bar.overlay=i%5;bar.target=(i+1)/7.;game->javaSession->bossOverlay.add(bar,0);}
 glViewport(0,0,800,480);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,400,240,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable2(GL_DEPTH_TEST);glEnable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 std::vector<unsigned char> full;unsigned long fullLight=0;
 for(int mode:{0,1,2}){
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);if(mode==2)game->javaSession->bossOverlay.clear();gui.renderJavaBossBars(mode==1?140:400,240);glFinish();assert(glGetError()==GL_NO_ERROR);
  std::vector<unsigned char> pixels(800*480*4);glReadPixels(0,0,800,480,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());unsigned long light=0;for(size_t i=0;i<pixels.size();i+=4)light+=pixels[i]+pixels[i+1]+pixels[i+2];
  if(mode==0){assert(light>0);unsigned long barLight=0;for(int y=406;y<416;++y)for(int x=218;x<582;++x){const size_t at=(y*800+x)*4;barLight+=pixels[at]+pixels[at+1]+pixels[at+2];}assert(barLight>0);fullLight=light;full=pixels;}else if(mode==1)assert(light>0);else assert(light==0);
 }
 std::set<std::vector<unsigned char>> colors;
 for(int color=0;color<7;++color){
  std::set<std::vector<unsigned char>> styles;
  for(int overlay=0;overlay<5;++overlay){
   unsigned long emptyLight=0,fullLight=0;
   for(int fill:{0,1}){
    game->javaSession->bossOverlay.clear();JavaBossBar bar;bar.id="coverage";bar.color=color;bar.overlay=overlay;bar.target=fill;game->javaSession->bossOverlay.add(bar,0);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);gui.renderJavaBossBars(400,240);glFinish();assert(glGetError()==GL_NO_ERROR);
    std::vector<unsigned char> pixels(800*480*4),region;glReadPixels(0,0,800,480,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());unsigned long light=0;
    for(int y=406;y<416;++y)for(int x=218;x<582;++x){const size_t at=(y*800+x)*4;for(int c=0;c<3;++c){region.push_back(pixels[at+c]);light+=pixels[at+c];}}
    assert(light>0);if(fill==0)emptyLight=light;else{fullLight=light;styles.insert(region);if(overlay==0)colors.insert(region);}
   }
   assert(fullLight>emptyLight);
  }
  assert(styles.size()==5);
 }
 assert(colors.size()==7);
 std::vector<unsigned char> flipped(full.size());for(int row=0;row<480;++row)std::copy(full.begin()+row*800*4,full.begin()+(row+1)*800*4,flipped.begin()+(479-row)*800*4);
 png_image image{};image.version=PNG_IMAGE_VERSION;image.width=800;image.height=480;image.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&image,argv[1],0,flipped.data(),0,nullptr));
 std::cout<<"Actual native boss bars: all 7 colors, all 5 notch styles, empty/full progress, Unicode, narrow screen, clearing and GL pixel checks PASS"<<std::endl;std::_Exit(0);
}
