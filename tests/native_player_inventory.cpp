#include "network/java/JavaPackFontRuntime.h"
#include "network/java/JavaPackItemModels.h"
#include "network/java/JavaGameMode.h"
#include "client/gui/screens/JavaInventoryScreen.h"
#include "client/gui/screens/touch/TouchIngameBlockSelectionScreen.h"
#include "world/level/tile/Tile.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "util/Mth.h"
#include "AppPlatform_glfw.h"
#include <cassert>
#include "client/sound/SoundEngine.h"
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
int main(){
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_TRUE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);auto* window=glfwCreateWindow(400,240,"Semantic touch inventory",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();
 AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);Font font(&options,"font/default8.png",&textures);font.setJavaTextEnabled(true);auto* game=new PackGameProbe();game->textures=&textures;game->font=&font;game->options.useTouchScreen=true;JavaGameSession session;game->javaSession=&session;assert(session.connection.connect("fixture.invalid","fixture",false));
 auto setup=[&](const std::string& kind){session.inventory=JavaGameSession::Inventory{};auto& inv=session.inventory;inv.menuKind=kind;inv.windowId=kind=="player"?0:7;inv.menuGeneration=42;inv.title=kind=="crafting"?"Crafting table":kind=="player"?"Inventory":"Smelting";inv.slots.resize(kind=="player"||kind=="crafting"?46:39);auto r=[&](const char* key,int start,int end){JavaGameSession::SlotRange range;range.start=start;range.end=end;inv.slotRanges[key]=range;};r("result",0,1);if(kind=="player"){r("crafting",1,5);r("armor",5,9);r("player_inventory",9,36);r("hotbar",36,45);r("offhand",45,46);}else if(kind=="crafting"){r("crafting",1,10);r("player_inventory",10,37);r("hotbar",37,46);}else{r("ingredient",0,1);r("fuel",1,2);r("result",2,3);r("player_inventory",3,30);r("hotbar",30,39);}inv.carried.count=3;inv.carried.resourceId="minecraft:stone";};
 auto paint=[&](SemanticInventoryProbe& ui,int w,int h,const std::string& file){projection();glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,w,h,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();ui.render(-1,-1,0);auto p=pixels();if(!file.empty())save(p,file);};

 class NativeProbe:public JavaPlayerInventoryScreen{public:using JavaPlayerInventoryScreen::JavaPlayerInventoryScreen;using Screen::buttons;using Screen::mouseClicked;using Screen::mouseReleased;};
 for(auto size:std::vector<std::pair<int,int>>{{400,240},{200,120}}){setup("player");game->width=400;game->height=240;Gui::GuiScale=400.f/size.first;Gui::InvGuiScale=1.f/Gui::GuiScale;session.inventory.slots[9].originalId=264;session.inventory.slots[9].count=1;session.inventory.slots[9].resourceId="minecraft:diamond";NativeProbe ui;ui.Screen::init(game,size.first,size.second);projection();glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,size.first,size.second,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();ui.render(-1,-1,0);save(pixels(),"/home/truancy/Documents/Codex/2026-10-13/referenced-chatgpt-conversation-this-is-an/work/native-player-inventory-"+std::to_string(size.first)+".png");assert(ui.slotAt(0)==9&&ui.slotAt(26)==35);sent.clear();assert(ui.addItem(nullptr,0));assert(sent.find("\"slot\":9,")!=std::string::npos);for(Button* b:ui.buttons)if(b->id==2)ui.buttonClicked(b);assert(ui.slotAt(4)==45);++session.inventory.menuGeneration;sent.clear();assert(!ui.addItem(nullptr,0)&&sent.empty());}
 // Reproduce the live Hotbar layout with a real populated authoritative slot36.
 for(auto size:std::vector<std::pair<int,int>>{{400,240},{200,120},{284,160}}){
  setup("player");session.inventory.carried.count=0;game->width=400;game->height=240;Gui::GuiScale=400.f/size.first;Gui::InvGuiScale=1.f/Gui::GuiScale;NativeProbe ui;ui.Screen::init(game,size.first,size.second);for(auto* b:ui.buttons)if(b->id==5)ui.buttonClicked(b);assert(ui.slotAt(0)==36&&ui.slotAt(8)==44&&ui.slotAt(9)==-1);
  auto draw=[&](){projection();glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,size.first,size.second,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();ui.render(-1,-1,0);return pixels();};auto empty=draw();auto& stack=session.inventory.slots[36];stack.originalId=1;stack.count=8;stack.resourceId="minecraft:stone";auto filled=draw();
  const int left=(size.first-std::max(1,(size.first-20)/32)*32)/2;int changed=0;for(int y=38*240/size.second;y<54*240/size.second;++y)for(int x=(left+8)*400/size.first;x<(left+24)*400/size.first;++x)if(color(empty,x,y)!=color(filled,x,y))++changed;assert(changed>8);
  save(filled,"/home/truancy/Documents/Codex/2026-10-13/referenced-chatgpt-conversation-this-is-an/work/native-player-hotbar-"+std::to_string(size.first)+".png");
 }
 setup("crafting");NativeProbe manual(true,7);manual.Screen::init(game,400,240);assert(manual.slotAt(0)==1&&manual.slotAt(8)==9&&manual.slotAt(9)==-1);sent.clear();for(Button* b:manual.buttons)if(b->id==8){assert(b->clicked(game,b->x+b->width/2,b->y+b->height/2));manual.buttonClicked(b);}assert(sent.find("\"slot\":0,")!=std::string::npos&&sent.find("\"window_id\":7")!=std::string::npos);
 for(auto size:std::vector<std::pair<int,int>>{{400,240},{200,120}}){setup("crafting");game->width=400;game->height=240;Gui::GuiScale=400.f/size.first;Gui::InvGuiScale=1.f/Gui::GuiScale;session.inventory.carried.count=0;session.inventory.slots[0].count=4;session.inventory.slots[0].resourceId="minecraft:crafting_table";NativeProbe grid(true,7);grid.Screen::init(game,size.first,size.second);projection();glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,size.first,size.second,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();grid.render(-1,-1,0);auto p=pixels();save(p,"/home/truancy/Documents/Codex/2026-10-13/referenced-chatgpt-conversation-this-is-an/work/native-workbench-grid-"+std::to_string(size.first)+".png");assert((color(p,70,2)!=std::array<unsigned char,3>{{0,0,0}}));for(Button* b:grid.buttons)if(b->id==8){assert(b->visible&&b->x>=0&&b->x+b->width<=size.first&&b->y>=24&&b->y+b->height<size.second-22);}}
 // Actual touch dispatch must not let the invisible result button steal header taps.
 for(const auto kind:std::vector<std::string>{"player","crafting"}){
  setup(kind);game->width=400;game->height=240;Gui::GuiScale=1;Gui::InvGuiScale=1;
  game->screen=nullptr;SoundEngine sound(16);options.sound=false;sound.init(game,&options);game->soundEngine=&sound;
  NativeProbe touch(false,session.inventory.windowId);touch.Screen::init(game,400,240);
  for(auto* b:touch.buttons)if(b->id==8)assert(!b->visible&&!b->active);
  sent.clear();touch.mouseClicked(24,12,MouseAction::ACTION_LEFT);touch.mouseReleased(24,12,MouseAction::ACTION_LEFT);
  assert(dynamic_cast<JavaCraftingScreen*>(game->screen));
  assert(dynamic_cast<JavaCraftingScreen*>(game->screen)->window()==session.inventory.windowId&&sent.empty());
  delete game->screen;game->screen=nullptr;game->soundEngine=nullptr;
 }
 // Exercise the actual E/touch inventory entry used by the running game.
 setup("player");Screen* chosen=game->screenChooser.createScreen(SCREEN_BLOCKSELECTION);assert(dynamic_cast<JavaPlayerInventoryScreen*>(chosen)&&!dynamic_cast<JavaInventoryScreen*>(chosen));chosen->init(game,400,240);assert(dynamic_cast<JavaPlayerInventoryScreen*>(chosen)->slotAt(0)==9);delete chosen;
 setup("crafting");chosen=game->screenChooser.createScreen(SCREEN_BLOCKSELECTION);assert(dynamic_cast<JavaCraftingScreen*>(chosen));chosen->init(game,400,240);delete chosen;
 setup("generic_9x3");chosen=game->screenChooser.createScreen(SCREEN_BLOCKSELECTION);assert(dynamic_cast<JavaInventoryScreen*>(chosen));chosen->init(game,400,240);delete chosen;
 game->javaSession=nullptr;chosen=game->screenChooser.createScreen(SCREEN_BLOCKSELECTION);assert(dynamic_cast<Touch::IngameBlockSelectionScreen*>(chosen));delete chosen;game->javaSession=&session;
 std::cout<<"Native shared inventory drawer: bag/armor/offhand exact authoritative mapping, stale generation guard and visible 400/200 screenshots PASS"<<std::endl;std::_Exit(0);}
