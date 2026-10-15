#include "client/gui/screens/JavaInventoryScreen.h"
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
 game->javaSession->inventory.slots.resize(90);game->javaSession->inventory.carried.count=3;game->javaSession->inventory.carried.resourceId="minecraft:stone";
 const std::string title=u8"§x§1§2§a§b§e§f雪 ✦ A very long native server inventory title with gradients";
 game->javaSession->inventory.title=title;
 for(int available:{0,5,12,24,60,100,200}){
  const auto fitted=JavaInventoryScreen::fitTitle(font,title,available);assert(font.widthJava(fitted)<=available);
  for(const auto& glyph:JavaRichText::parse(fitted))assert(glyph.codepoint!=0xfffd);
  if(available>=60)assert((JavaRichText::parse(fitted).front().color&0xffffff)==0x12abef);
 }
 assert(JavaInventoryScreen::fitTitle(font,"Short",200)=="Short");
 assert(JavaInventoryScreen::fitTitle(font,"First\nSecond",100).find('\n')==std::string::npos);
 InventoryProbe inventory;
 for(int width:{200,240,284,400}){
  inventory.Screen::init(game,width,240);
  for(int i=0;i<4;++i){auto* a=inventory.buttons[i];assert(a->x>=0&&a->x+a->width<=width);for(int j=i+1;j<4;++j){auto* b=inventory.buttons[j];assert(a->x+a->width<=b->x);}}
 }
 inventory.buttonClicked(inventory.buttons[2]);inventory.Screen::init(game,400,240);assert(inventory.buttons[2]->msg=="Half"&&inventory.buttons[67]->msg=="Drop one");inventory.buttonClicked(inventory.buttons[2]);
 glViewport(0,0,800,480);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,400,240,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable2(GL_DEPTH_TEST);glEnable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 inventory.render(-1,-1,1);glFinish();assert(glGetError()==GL_NO_ERROR);
 std::vector<unsigned char> pixels(800*480*4),flipped(pixels.size());glReadPixels(0,0,800,480,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());for(int row=0;row<480;++row)std::copy(pixels.begin()+row*800*4,pixels.begin()+(row+1)*800*4,flipped.begin()+(479-row)*800*4);
 png_image image{};image.version=PNG_IMAGE_VERSION;image.width=800;image.height=480;image.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&image,argv[1],0,flipped.data(),0,nullptr));
 std::cout<<"Actual native inventory layout: responsive footer, rich Unicode title bounds and GL render PASS"<<std::endl;std::_Exit(0);
}
