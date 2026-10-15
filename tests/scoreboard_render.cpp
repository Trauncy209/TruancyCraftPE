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


 assert(font.fitJava(u8"§a雪✦ABCDE",200)==u8"§a雪✦ABCDE");auto fitted=font.fitJava(u8"§x§1§2§3§4§5§0雪✦ABCDEFGHIJKLMNOPQRSTUVWXYZ",35);assert(font.widthJava(fitted)<=35);assert(fitted.find("...")!=std::string::npos);assert(font.fitJava("Too long",1).empty());
 Gui gui(game);game->javaSession->spawned=true;

 auto& board=game->javaSession->scoreboard;board.displays[1]="main";auto& objective=board.objectives["main"];objective.text=u8"§x§1§2§a§b§e§fZEUS 雪";
 for(int i=0;i<15;++i){auto& entry=objective.scores["row"+std::to_string(i)];entry.score=15-i;entry.hasDisplay=true;entry.display=u8"§e✦ §fPlayer "+std::to_string(i);}
 objective.scores["row0"].number.kind="styled";objective.scores["row0"].number.text="§x§1§2§3§4§5§0§l";
 objective.scores["row1"].number.kind="fixed";objective.scores["row1"].number.text=u8"§b雪";
 objective.scores["row2"].number.kind="blank";
 std::vector<unsigned char> saved;
 for(int mode=0;mode<4;++mode){
  const int width=mode==1?140:400;
  if(mode==2)objective.scores["row0"].display=std::string(200,'W');
  if(mode==3)board.clear();
  glViewport(0,0,800,480);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,400,240,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable2(GL_DEPTH_TEST);glEnable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);gui.renderJavaScoreboard(width,240);glFinish();assert(glGetError()==GL_NO_ERROR);
  std::vector<unsigned char> pixels(800*480*4);glReadPixels(0,0,800,480,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());unsigned long light=0;int minX=800,maxX=-1;
  for(int y=0;y<480;++y)for(int x=0;x<800;++x){size_t at=(y*800+x)*4;auto rgb=pixels[at]+pixels[at+1]+pixels[at+2];light+=rgb;if(rgb){minX=std::min(minX,x);maxX=std::max(maxX,x);}}
  if(mode==3)assert(light==0);else{assert(light>0);assert(minX>=0&&maxX<width*2);}
  if(mode!=3)assert(maxX-minX<=int(width*.43f*2)+12);
  if(mode==0){saved=pixels;unsigned yellow=0;for(int y=0;y<480;++y)for(int x=minX;x<=maxX;++x){size_t at=(y*800+x)*4;if(pixels[at]>200&&pixels[at+1]>200&&pixels[at+2]<100)++yellow;}assert(yellow>0);}

 }
 std::vector<unsigned char> flipped(saved.size());for(int row=0;row<480;++row)std::copy(saved.begin()+row*800*4,saved.begin()+(row+1)*800*4,flipped.begin()+(479-row)*800*4);
 png_image image{};image.version=PNG_IMAGE_VERSION;image.width=800;image.height=480;image.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&image,argv[1],0,flipped.data(),0,nullptr));
 std::cout<<"Native scoreboard GL: 15 rows, RGB styles, Unicode, fixed/blank numbers, narrow/long-text bounds and clearing PASS\n";std::_Exit(0);
}
