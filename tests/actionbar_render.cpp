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
class GameProbe:public Minecraft{public:GameProbe():Minecraft(){}};
class InventoryProbe:public JavaInventoryScreen{public:InventoryProbe():JavaInventoryScreen(0){}using Screen::buttons;};
int main(int argc,char**argv){
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
 auto* window=glfwCreateWindow(800,480,"Native inventory layout",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();
 AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);Font font(&options,"font/default8.png",&textures);font.setJavaTextEnabled(true);
 auto* game=new GameProbe();game->textures=&textures;game->font=&font;game->javaSession=new JavaGameSession();


 Gui gui(game);game->javaSession->spawned=true;game->javaSession->actionBar=u8"§x§1§2§a§b§e§fReady ✦ 雪 — Fight!";
 glViewport(0,0,800,480);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,400,240,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable2(GL_DEPTH_TEST);glEnable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 std::vector<unsigned char> full;unsigned long fullLight=0;
 for(int ticks:{60,10,0}){
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);game->javaSession->actionBarTicks=ticks;gui.renderJavaActionBar(400,221,0);glFinish();assert(glGetError()==GL_NO_ERROR);
  std::vector<unsigned char> pixels(800*480*4);glReadPixels(0,0,800,480,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());unsigned long light=0;for(size_t i=0;i<pixels.size();i+=4)light+=pixels[i]+pixels[i+1]+pixels[i+2];
  if(ticks==60){assert(light>0);fullLight=light;full=pixels;}else if(ticks==10){assert(light>fullLight/4&&light<fullLight*3/4);}else assert(light==0);
 }
 int baselineHeight=-1;for(int width:{400,140}){game->javaSession->actionBar=std::string(200,'A');game->javaSession->actionBarTicks=60;glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);gui.renderJavaActionBar(width,221,0);glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> pixels(800*480*4);glReadPixels(0,0,800,480,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());int minY=480,maxY=-1,minX=800,maxX=-1;for(int y=0;y<480;++y)for(int x=0;x<800;++x){size_t at=(y*800+x)*4;if(pixels[at]+pixels[at+1]+pixels[at+2]){minY=std::min(minY,y);maxY=std::max(maxY,y);minX=std::min(minX,x);maxX=std::max(maxX,x);}}assert(maxX>=minX&&minX>=0&&maxX<width*2);int height=maxY-minY+1;assert(height>8);if(baselineHeight<0)baselineHeight=height;else assert(height==baselineHeight);}
 std::vector<unsigned char> flipped(full.size());for(int row=0;row<480;++row)std::copy(full.begin()+row*800*4,full.begin()+(row+1)*800*4,flipped.begin()+(479-row)*800*4);
 png_image image{};image.version=PNG_IMAGE_VERSION;image.width=800;image.height=480;image.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&image,argv[1],0,flipped.data(),0,nullptr));
 std::cout<<"Actual native action bar: rich Unicode glyphs and timed fade/expiry pixel checks PASS"<<std::endl;std::_Exit(0);
}
