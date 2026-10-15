#include "network/java/JavaPackFontRuntime.h"
#include "network/java/JavaPackItemModels.h"
#include "network/java/JavaGameMode.h"
#include "client/gui/screens/JavaInventoryScreen.h"
#include "network/java/JavaLevel.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/tile/Tile.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "util/Mth.h"
#include "AppPlatform_glfw.h"
#include <cassert>
#include <png.h>
#include <iostream>
class PackGameProbe:public Minecraft{public:PackGameProbe():Minecraft(){}};
class SemanticInventoryProbe:public JavaInventoryScreen{public:explicit SemanticInventoryProbe(int id):JavaInventoryScreen(id){}using Screen::buttons;};
static std::string sent;
extern "C" TcpeJavaSession* tcpe_java_connect(const char*,size_t,const char*,size_t,bool){return reinterpret_cast<TcpeJavaSession*>(1);}
extern "C" void tcpe_java_close(TcpeJavaSession*){}
extern "C" bool tcpe_java_command(TcpeJavaSession*,const char* data,size_t length){sent.assign(data,length);return true;}
static JavaGameSession::PackModelQuad quad(int texture){JavaGameSession::PackModelQuad q;q.texture=texture;q.vertices={{{{0,0,8,0,1}},{{16,0,8,1,1}},{{16,16,8,1,0}},{{0,16,8,0,0}}}};return q;}
static JavaGameSession::PackFonts bundle(const char* pack,unsigned char red,unsigned char green,unsigned char blue,uint64_t generation=4){
 JavaGameSession::PackFonts b;b.id=pack;b.connection=7;b.generation=generation;JavaGameSession::PackFontTexture texture;texture.width=texture.height=2;for(int i=0;i<4;++i)texture.rgba.insert(texture.rgba.end(),{red,green,blue,255});b.textures.push_back(texture);JavaGameSession::PackItemModel m;m.id="test:flat";m.quads.push_back(quad(0));b.models.push_back(m);return b;
}
static void projection(){glViewport(0,0,400,240);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,400,240,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable2(GL_LIGHTING);glDisable2(GL_FOG);glDisable2(GL_CULL_FACE);glDisable2(GL_DEPTH_TEST);glEnable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glClearColor(0,0,0,1);glClearDepthf(1);glDepthMask(GL_TRUE);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);}
static std::vector<unsigned char> pixels(){glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> p(400*240*4);glReadPixels(0,0,400,240,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;}
static std::array<unsigned char,3> color(const std::vector<unsigned char>& p,int x,int y){size_t i=((239-y)*400+x)*4;return {{p[i],p[i+1],p[i+2]}};}
static void save(const std::vector<unsigned char>& p,const std::string& file){std::vector<unsigned char> flipped(p.size());for(int y=0;y<240;++y)std::copy(p.begin()+y*400*4,p.begin()+(y+1)*400*4,flipped.begin()+(239-y)*400*4);png_image image{};image.version=PNG_IMAGE_VERSION;image.width=400;image.height=240;image.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&image,file.c_str(),0,flipped.data(),0,nullptr));}
static Button* button(SemanticInventoryProbe& s,int id){for(auto* b:s.buttons)if(b->id==id)return b;assert(false);return nullptr;}
static bool overlaps(Button* a,Button* b){return a->visible&&b->visible&&a->x<b->x+b->width&&b->x<a->x+a->width&&a->y<b->y+b->height&&b->y<a->y+a->height;}
#include "headless_egl.h"
int main(){
 NativeHeadlessEGL offscreen(400,240);Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();
 AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);Font font(&options,"font/default8.png",&textures);auto* game=new PackGameProbe();game->textures=&textures;game->font=&font;game->options.useTouchScreen=true;JavaGameSession session;game->javaSession=&session;assert(session.connection.connect("headless-fixture.invalid","fixture",false));
 class Probe:public JavaPlayerInventoryScreen{public:using JavaPlayerInventoryScreen::JavaPlayerInventoryScreen;using Screen::buttons;};
 for(auto size:std::vector<std::pair<int,int>>{{400,240},{200,120},{284,160}}){
  session.inventory={};auto& inv=session.inventory;inv.windowId=0;inv.menuKind="player";inv.menuGeneration=42;inv.slots.resize(46);inv.slotRanges["hotbar"]={36,45};inv.slotRanges["player_inventory"]={9,36};inv.slotRanges["armor"]={5,9};inv.slotRanges["offhand"]={45,46};
  game->width=400;game->height=240;Gui::GuiScale=400.f/size.first;Gui::InvGuiScale=1.f/Gui::GuiScale;Probe ui;ui.Screen::init(game,size.first,size.second);
  for(int i=0;i<9;++i)assert(ui.slotAt(i)==36+i);for(int i=0;i<27;++i)assert(ui.slotAt(9+i)==9+i);assert(ui.slotAt(36)==-1);
  auto paint=[&](){projection();glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,size.first,size.second,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();ui.render(-1,-1,0);return pixels();};auto empty=paint();
  for(int i=36;i<45;++i){inv.slots[i].resourceId="minecraft:diamond";inv.slots[i].originalId=264;inv.slots[i].count=i-35;}inv.slots[9].resourceId="minecraft:stone";inv.slots[9].originalId=1;inv.slots[9].count=16;auto filled=paint();
  const int cell=std::max(12,std::min(32,(size.first-20)/9)),left=(size.first-9*cell)/2;
  for(int col=0;col<9;++col){int changed=0;const int cx=left+col*cell+(cell-16)/2;for(int y=32*240/size.second;y<(30+cell-2)*240/size.second;++y)for(int x=(cx)*400/size.first;x<(cx+16)*400/size.first;++x)if(color(empty,x,y)!=color(filled,x,y))++changed;assert(changed>8);}
  save(filled,"/home/truancy/Documents/Codex/2026-10-13/referenced-chatgpt-conversation-this-is-an/work/headless-inventory-hotbar-first-"+std::to_string(size.first)+".png");
  // Actual native callback emits current authoritative slots without mutating them.
  for(int index:{0,8,9,35}){sent.clear();assert(ui.addItem(nullptr,index));assert(sent.find("\"type\":\"click_slot\"")!=std::string::npos&&sent.find("\"slot\":"+std::to_string(ui.slotAt(index))+",")!=std::string::npos&&sent.find("\"menu_generation\":42")!=std::string::npos);}
  assert(inv.slots[36].count==1&&inv.slots[9].count==16&&inv.carried.count==0);
  ++inv.menuGeneration;sent.clear();assert(!ui.addItem(nullptr,0)&&sent.empty());
 }
 assert(glGetError()==GL_NO_ERROR);std::cout<<"HEADLESS EGL: all9 actual hotbar icons top row at400/284/200px, bag below, native pickup/place callback slotmapping and stalegeneration guard PASS; no window/input/server"<<std::endl;std::_Exit(0);
}
