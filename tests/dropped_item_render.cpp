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
#include "network/java/JavaItemIcons.h"
#include "network/java/JavaEntityAdapter.h"
#include "network/java/JavaLevel.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/entity/item/ItemEntity.h"
#include "client/renderer/entity/EntityRenderDispatcher.h"
class GameProbe:public Minecraft{public:GameProbe():Minecraft(){}};
class InventoryProbe:public JavaInventoryScreen{public:InventoryProbe():JavaInventoryScreen(0){}using Screen::buttons;};
int main(int argc,char**argv){
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_TRUE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
 auto* window=glfwCreateWindow(800,480,"Native inventory layout",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();
 AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);Font font(&options,"font/default8.png",&textures);font.setJavaTextEnabled(true);
 auto* game=new GameProbe();game->textures=&textures;game->font=&font;game->javaSession=new JavaGameSession();



 glViewport(0,0,800,480);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(-2,2,-1.2,1.2,-10,10);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glEnable2(GL_TEXTURE_2D);glDepthFunc(GL_LESS);
 std::vector<unsigned char> full;unsigned long baseline=0;
 const char* ids[]={"minecraft:diamond_sword","minecraft:mace","minecraft:netherite_sword","minecraft:stone"};
 for(int mode:{0,1,2}){
  glEnable2(GL_DEPTH_TEST);glClearDepthf(mode==1?0:1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  for(int i=0;i<4;++i){assert(JavaItemIcons::has(ids[i]));glPushMatrix2();glTranslatef2(-1.5f+i,-.25f,0);assert(JavaItemIcons::renderWorld(&textures,ids[i],.7f,mode==2?.25f:1.f));glPopMatrix2();assert(glIsEnabled(GL_DEPTH_TEST));assert(glGetError()==GL_NO_ERROR);}
  glFinish();std::vector<unsigned char> pixels(800*480*4);glReadPixels(0,0,800,480,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());unsigned long light=0;for(size_t i=0;i<pixels.size();i+=4)light+=pixels[i]+pixels[i+1]+pixels[i+2];
  if(mode==0){assert(light>0);baseline=light;full=pixels;for(int item=0;item<4;++item){unsigned long region=0;for(int y=130;y<350;++y)for(int x=item*200;x<(item+1)*200;++x){size_t p=(y*800+x)*4;region+=pixels[p]+pixels[p+1]+pixels[p+2];}assert(region>0);}}else if(mode==1)assert(light==0);else assert(light>baseline/8&&light<baseline/2);
 }
 assert(!JavaItemIcons::renderWorld(&textures,"minecraft:missing_test_item",.5f));assert(!JavaItemIcons::renderWorld(&textures,ids[0],0));

 JavaGameSession session;Level* level=new JavaLevel(new MemoryLevelStorage(),session.chunks);EntityRenderDispatcher dispatcher;dispatcher.textures=&textures;dispatcher.playerRotY=180;
 auto frame=[&](const char* resource,int count,int age,float partial){
  JavaGameSession::Entity snapshot{};snapshot.id=777;snapshot.kind="Item";snapshot.position={{0,64,0}};snapshot.droppedItem.resourceId=resource;snapshot.droppedItem.originalId=267;snapshot.droppedItem.originalData=0;snapshot.droppedItem.count=count;session.entities[777]=snapshot;synchronizeJavaEntities(level,session);auto* item=dynamic_cast<ItemEntity*>(level->getEntity(777));if(!item){std::cerr<<"Item fixture lookup failed: resource="<<resource<<" count="<<count<<" entity="<<level->getEntity(777)<<" native_count="<<level->entities.size()<<" java="<<level->isJavaServerLevel()<<std::endl;}assert(item);item->age=age;item->bobOffs=0;
  glClearDepthf(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);assert(renderJavaPresentation(item,dispatcher,0,-.25f,0,partial));glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> pixels(800*480*4);glReadPixels(0,0,800,480,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());return pixels;
 };
 for(const char* resource:{"minecraft:diamond_sword","minecraft:mace","minecraft:stone"}){
  std::set<std::vector<unsigned char>> shapes;std::vector<unsigned char> four;
  for(int count:{1,2,6,21,64}){auto pixels=frame(resource,count,10,0);if(count==64)assert(pixels==four);else{shapes.insert(pixels);if(count==21)four=pixels;}}assert(shapes.size()==4);
 }
 assert(frame("minecraft:stone",1,10,0)!=frame("minecraft:stone",1,20,0));
 assert(frame("minecraft:stone",1,10,0)!=frame("minecraft:stone",1,10,.9f));
 std::vector<unsigned char> flipped(full.size());for(int row=0;row<480;++row)std::copy(full.begin()+row*800*4,full.begin()+(row+1)*800*4,flipped.begin()+(479-row)*800*4);
 png_image image{};image.version=PNG_IMAGE_VERSION;image.width=800;image.height=480;image.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&image,argv[1],0,flipped.data(),0,nullptr));std::cout<<"World item icons: legacy/modern sprites/block visible; depth occlusion and depth-state retention and dark lighting, native stack thresholds and partial animation PASS"<<std::endl;std::_Exit(0);
}
