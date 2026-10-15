#include "client/gui/screens/JavaInventoryScreen.h"
#include "client/gui/Gui.h"
#include "client/player/LocalPlayer.h"
#include "network/java/JavaLevel.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/tile/Tile.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "util/Mth.h"
#include "AppPlatform_glfw.h"
#include <cassert>
#include <iostream>
class GameProbe:public Minecraft{public:GameProbe():Minecraft(){}};
// Exercise the private native hotbar draw without changing its public API.
struct SlotAccess {using type=void(Gui::*)(int,int,int,float);friend type slotMethod(SlotAccess);};
template<class Tag,typename Tag::type Value>struct PrivateMethod {friend typename Tag::type slotMethod(Tag){return Value;}};
template struct PrivateMethod<SlotAccess,&Gui::renderSlot>;
int main(){
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);auto* window=glfwCreateWindow(160,96,"Pack hotbar",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();
 AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);Font font(&options,"font/default8.png",&textures);auto* game=new GameProbe();game->textures=&textures;game->font=&font;game->javaSession=new JavaGameSession();game->level=new JavaLevel(new MemoryLevelStorage(),game->javaSession->chunks);game->player=new LocalPlayer(game,game->level,nullptr,0,false);Gui gui(game);
 auto& item=game->javaSession->inventory.hotbar[0];item.count=7;item.resourceId="minecraft:paper";item.itemModel="test:red";
 TextureData image;image.w=image.h=1;image.numBytes=4;image.data=new unsigned char[4]{255,255,255,255};auto texture=textures.assignTexture("test/hotbar-red",image);
 JavaGameSession::PackItemModel model;model.id="test:red";JavaGameSession::PackModelQuad quad;quad.texture=0;quad.tint=0;JavaGameSession::PackModelTint tint;tint.kind="constant";tint.color=0xff0000;model.tints.push_back(tint);quad.vertices={{{{0,0,8,0,1}},{{16,0,8,1,1}},{{16,16,8,1,0}},{{0,16,8,0,0}}}};model.quads.push_back(quad);JavaPackItemModels::install(model,{texture});
 auto frame=[&](bool base=false,bool batch=false){glViewport(0,0,160,96);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,80,48,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glEnable2(GL_TEXTURE_2D);glDisable2(GL_DEPTH_TEST);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);auto& t=Tesselator::instance;if(batch)t.beginOverride();if(base)assert(JavaItemIcons::render(&textures,"minecraft:paper",20,20));else (gui.*slotMethod(SlotAccess{}))(0,20,20,0);if(batch){assert(t.isOverridden());t.endOverrideAndDraw();}glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> pixels(160*96*4);glReadPixels(0,0,160,96,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());return pixels;};
 auto packed=frame(false,true);int red=0;for(int y=0;y<96;++y)for(int x=0;x<160;++x){auto p=&packed[(y*160+x)*4];if(p[0]==255&&p[1]==0&&p[2]==0)++red;}assert(red==32*32);assert(item.count==7);
 model.tints[0].kind="custom_model_data";model.tints[0].index=1;model.tints[0].defaultColor=0x00ff00;JavaPackItemModels::install(model,{texture});item.customModelData.present=true;item.customModelData.colors={0x0000ff,0xffff00};auto tinted=frame();auto center=[](const std::vector<unsigned char>& p,int c){return p[((95-50)*160+50)*4+c];};assert(center(tinted,0)==255&&center(tinted,1)==255&&center(tinted,2)==0);
 item.customModelData.colors.resize(1);auto missing=frame();assert(center(missing,0)==0&&center(missing,1)==255&&center(missing,2)==0);
 model.tints[0].kind="unsupported";model.tints[0].defaultColor=0x0000ff;JavaPackItemModels::install(model,{texture});auto unsupported=frame();assert(center(unsupported,0)==0&&center(unsupported,1)==0&&center(unsupported,2)==255);
 model.tints[0].kind="constant";model.tints[0].color=0xff0000;JavaPackItemModels::install(model,{texture});
 JavaPackItemModels::clear();auto cleared=frame();auto base=frame(true);assert(cleared==base&&cleared!=packed);assert(item.count==7);
 model.id="minecraft:paper";JavaPackItemModels::install(model,{texture});item.itemModel.clear();assert(frame()==packed);JavaPackItemModels::clear();item.itemModel="test:unknown";assert(frame()==base);
 std::cout<<"Native hotbar pack models: exact16px slot, explicit model/resource fallback, retained batch/count and cleanup restoration PASS"<<std::endl;std::_Exit(0);
}
