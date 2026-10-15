#include "network/java/JavaPackFontRuntime.h"
#include "client/gui/screens/JavaServerRow.h"
#include <set>
#include "platform/input/TouchMouseOwner.h"
#include <map>
#include <unistd.h>
#include <fstream>
#include "network/java/JavaPackItemModels.h"
#include "network/java/JavaGameMode.h"
#include "client/gui/screens/JavaInventoryScreen.h"
#include "network/java/JavaLevel.h"
#include "client/renderer/GameRenderer.h"
#include "world/entity/Mob.h"
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
#ifdef TCPE_TEST_MOBILE_PICK
void tcpeQaWorldAtlasPass(int,int){} // Unused desktop-only QA hook in the shared headless link.
#endif
class PackGameProbe:public Minecraft{public:PackGameProbe():Minecraft(){}};
// Access only the real deferred-screen gate for a headless event-loop fixture.
template<class Tag,typename Tag::type Member>struct TestMember{friend typename Tag::type field(Tag){return Member;}};
struct ScreenMutexTag{using type=bool Minecraft::*;friend type field(ScreenMutexTag);};
struct ScheduledTag{using type=bool Minecraft::*;friend type field(ScheduledTag);};
struct NextScreenTag{using type=Screen* Minecraft::*;friend type field(NextScreenTag);};
template struct TestMember<ScreenMutexTag,&Minecraft::screenMutex>;
template struct TestMember<ScheduledTag,&Minecraft::hasScheduledScreen>;
template struct TestMember<NextScreenTag,&Minecraft::scheduledScreen>;
struct ModelTag{using type=float (GameRenderer::*)[16];friend type field(ModelTag);};
struct ProjTag{using type=float (GameRenderer::*)[16];friend type field(ProjTag);};
template struct TestMember<ModelTag,&GameRenderer::lastModelMatrix>;
template struct TestMember<ProjTag,&GameRenderer::lastProjMatrix>;

class SemanticInventoryProbe:public JavaInventoryScreen{public:explicit SemanticInventoryProbe(int id):JavaInventoryScreen(id){}using Screen::buttons;};
static std::string sent;
static unsigned sentCalls=0;
struct TcpeJavaStatusRequest{std::string host;bool ready=false;bool failed=false;};
static std::set<TcpeJavaStatusRequest*> statusPending;static std::set<std::string> statusActive;static std::map<std::string,std::vector<int>> statusStarts;static int statusClock=0;
extern "C" TcpeJavaStatusRequest* tcpe_java_status_start(const char* host,size_t len,uint16_t){if(statusPending.size()>=4)return nullptr;std::string key(host,len);assert(statusActive.insert(key).second);statusStarts[key].push_back(statusClock);auto* r=new TcpeJavaStatusRequest{key};statusPending.insert(r);return r;}
extern "C" int32_t tcpe_java_status_poll(TcpeJavaStatusRequest* r,TcpeJavaStatusView* out){if(!r->ready)return 0;if(r->failed)return -1;static const std::string motd="Live count";*out={reinterpret_cast<const uint8_t*>(motd.data()),motd.size(),nullptr,0,statusClock/1000,400,10};return 1;}
extern "C" void tcpe_java_status_free(TcpeJavaStatusRequest* r){assert(statusPending.erase(r)==1);statusActive.erase(r->host);delete r;}

extern "C" TcpeJavaSession* tcpe_java_connect(const char*,size_t,const char*,size_t,bool){return reinterpret_cast<TcpeJavaSession*>(1);}
extern "C" void tcpe_java_close(TcpeJavaSession*){}
extern "C" bool tcpe_java_command(TcpeJavaSession*,const char* data,size_t length){sent.assign(data,length);++sentCalls;return true;}
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
 char previousDirectory[4096];assert(getcwd(previousDirectory,sizeof(previousDirectory)));char preferenceDirectory[]="/tmp/tcpe-view-preference-XXXXXX";assert(mkdtemp(preferenceDirectory));assert(symlink((std::string(previousDirectory)+"/data").c_str(),(std::string(preferenceDirectory)+"/data").c_str())==0);assert(chdir(preferenceDirectory)==0);{std::ofstream f("options.txt");f<<"mp_username:Fixture\nmp_saved_server_0_address:fixture.invalid\n";}assert(!JavaInventoryViewPreference::read());assert(!JavaInventoryViewPreference::storage());
 NativeHeadlessEGL offscreen(400,240);
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();
 AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);Font font(&options,"font/default8.png",&textures);font.setJavaTextEnabled(true);auto* game=new PackGameProbe();game->textures=&textures;game->font=&font;game->options.useTouchScreen=true;JavaGameSession session;game->javaSession=&session;assert(session.connection.connect("headless-fixture.invalid","fixture",false));
 auto setup=[&](const std::string& kind){session.recipeGhost=JavaGameSession::RecipeGhost{};session.inventory=JavaGameSession::Inventory{};auto& inv=session.inventory;inv.menuKind=kind;inv.windowId=kind=="player"?0:7;inv.menuGeneration=42;inv.title=kind=="crafting"?"Crafting table":kind=="player"?"Inventory":"Smelting";inv.slots.resize(kind=="player"||kind=="crafting"?46:39);auto r=[&](const char* key,int start,int end){JavaGameSession::SlotRange range;range.start=start;range.end=end;inv.slotRanges[key]=range;};r("result",0,1);if(kind=="player"){r("crafting",1,5);r("armor",5,9);r("player_inventory",9,36);r("hotbar",36,45);r("offhand",45,46);}else if(kind=="crafting"){r("crafting",1,10);r("player_inventory",10,37);r("hotbar",37,46);}else{r("ingredient",0,1);r("fuel",1,2);r("result",2,3);r("player_inventory",3,30);r("hotbar",30,39);}inv.carried.count=3;inv.carried.resourceId="minecraft:stone";};
 class Probe:public JavaCraftingScreen{public:explicit Probe(int id):JavaCraftingScreen(id){}using Screen::buttons;};
 auto find=[](Probe& ui,int id){for(auto* b:ui.buttons)if(b->id==id)return b;assert(false);return static_cast<Button*>(nullptr);};
 for(const std::string kind:{std::string("player"),std::string("crafting")})for(auto size:std::vector<std::pair<int,int>>{{400,240},{200,120}}){
  setup(kind);session.inventory.carried={};game->width=400;game->height=240;Gui::GuiScale=400.f/size.first;Gui::InvGuiScale=1.f/Gui::GuiScale;session.recipes.clear();
  JavaGameSession::Recipe recipe;recipe.id=17;recipe.width=recipe.height=2;recipe.kind="shaped";recipe.category="CraftingBuildingBlocks";recipe.placeable=true;recipe.previewComplete=true;recipe.result.count=1;recipe.result.resourceId="minecraft:crafting_table";recipe.result.originalId=58;JavaGameSession::Item ingredient;ingredient.resourceId="minecraft:oak_planks";ingredient.originalId=5;ingredient.count=1;recipe.ingredients.assign(4,ingredient);session.recipes.push_back(recipe);++session.recipesRevision;
  auto large=recipe;large.id=19;large.width=large.height=3;large.result.resourceId="minecraft:bone_block";large.result.originalId=-1;auto iron=ingredient;iron.resourceId="minecraft:bone_meal";iron.originalId=351;iron.originalData=15;large.ingredients.assign(9,iron);session.recipes.push_back(large);
  Probe ui(session.inventory.windowId);ui.Screen::init(game,size.first,size.second);assert(ui.getItems(nullptr).size()==(kind=="player"?1:2));
  session.inventory.carried=ingredient;sent.clear();ui.onItemSelected(nullptr,0);assert(sent.empty());session.inventory.carried={};ui.onItemSelected(nullptr,0);assert(sent.find("\"recipe_id\":17")!=std::string::npos);sent.clear();ui.onItemSelected(nullptr,0);assert(sent.empty());++session.inventoryRevision;ui.tick();
  // A server book reorder and a layout rebuild must retain the advertised ID.
  auto other=recipe;other.id=18;other.result.resourceId="minecraft:torch";other.result.originalId=50;session.recipes.insert(session.recipes.begin(),other);++session.recipesRevision;ui.tick();sent.clear();ui.buttonClicked(find(ui,1));assert(sent.find("\"recipe_id\":17")!=std::string::npos);++session.inventoryRevision;ui.tick();ui.setupPositions();sent.clear();ui.buttonClicked(find(ui,1));assert(sent.find("\"recipe_id\":17")!=std::string::npos);++session.inventoryRevision;ui.tick();
  ui.buttonClicked(find(ui,102));assert(ui.getItems(nullptr).empty());ui.buttonClicked(find(ui,102));assert(ui.getItems(nullptr).size()==(kind=="player"?2:3));sent.clear();ui.buttonClicked(find(ui,1));assert(sent.find("\"recipe_id\":17")!=std::string::npos);++session.inventoryRevision;ui.tick();
  session.recipeGhost.present=true;session.recipeGhost.windowId=session.inventory.windowId;session.recipeGhost.preview=recipe;++session.recipesRevision;ui.tick();assert(!find(ui,1)->active);sent.clear();ui.buttonClicked(find(ui,1));assert(sent.empty());
  projection();glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,size.first,size.second,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();ui.render(-1,-1,0);save(pixels(),"/home/truancy/Documents/Codex/2026-10-13/referenced-chatgpt-conversation-this-is-an/work/headless-crafting-ghost-"+kind+"-"+std::to_string(size.first)+".png");
  session.recipeGhost.windowId=999;ui.tick();assert(find(ui,1)->active);session.recipeGhost={};++session.inventoryRevision;ui.tick();
  // Native One puts a real result into an empty hotbar slot. No fabricated
  // result, no shift modifier on the destination, and no repeated transfer.
  session.inventory.slots[0]=recipe.result;++session.inventoryRevision;ui.tick();
  sent.clear();ui.buttonClicked(find(ui,1));assert(sent.find("\"type\":\"click_slot\"")!=std::string::npos);
  ++session.inventoryRevision;sent.clear();ui.tick();assert(sent.empty()); // unrelated pre-pickup snapshot
  session.inventory.slots[0]={};session.inventory.carried=recipe.result;++session.inventoryRevision;
  Keyboard::feed(Keyboard::KEY_LSHIFT,KeyboardAction::KEYDOWN);sent.clear();ui.tick();
  assert(sent.find("\"type\":\"click_slot\"")!=std::string::npos);
  assert(sent.find("\"slot\":"+std::to_string(kind=="player"?36:37))!=std::string::npos);
  Keyboard::feed(Keyboard::KEY_LSHIFT,KeyboardAction::KEYUP);Keyboard::reset();session.inventory.carried={};++session.inventoryRevision;sent.clear();ui.tick();assert(sent.empty());
  // Server rejection cannot trigger a placement or manufacture an item.
  session.inventory.slots[0]=recipe.result;++session.inventoryRevision;ui.tick();ui.buttonClicked(find(ui,1));
  session.inventory.carried={};++session.inventoryRevision;sent.clear();ui.tick();assert(sent.empty());session.inventory.slots[0]={};for(int tick=0;tick<41;++tick)ui.tick();
  // A full bag leaves the real cursor accessible; never overwrite any item.
  session.inventory.slots[0]=recipe.result;++session.inventoryRevision;ui.tick();ui.buttonClicked(find(ui,1));
  for(const char* role:{"hotbar","player_inventory"}){const auto range=session.inventory.slotRanges.at(role);for(int slot=range.start;slot<range.end;++slot)session.inventory.slots[slot]=ingredient;}
  session.inventory.slots[0]={};session.inventory.carried=recipe.result;++session.inventoryRevision;sent.clear();ui.tick();assert(sent.empty()&&session.inventory.carried.count==1);
  for(const char* role:{"hotbar","player_inventory"}){const auto range=session.inventory.slotRanges.at(role);for(int slot=range.start;slot<range.end;++slot)session.inventory.slots[slot]={};}session.inventory.carried={};++session.inventoryRevision;ui.tick();
  // An unchanged ready result clears placement Waiting without waiting for a
  // new snapshot; lack of any response releases the retry after two seconds.
  ui.onItemSelected(nullptr,1);for(int tick=0;tick<41;++tick)ui.tick();assert(find(ui,1)->active);
  ui.onItemSelected(nullptr,1);session.inventory.slots[0]=recipe.result;ui.tick();assert(find(ui,1)->active);session.inventory.slots[0]={};
  if(find(ui,4)->visible){sent.clear();ui.buttonClicked(find(ui,4));assert(sent.find("\"all\":true")!=std::string::npos);session.inventory.slots[0]=recipe.result;++session.inventoryRevision;ui.tick();sent.clear();ui.buttonClicked(find(ui,1));assert(sent.find("\"type\":\"quick_move\"")!=std::string::npos&&sent.find("\"slot\":0")!=std::string::npos);session.inventory.slots[0]={};}
  // Bone blocks exercise the native recipe-list 3x3 result, not the manual grid.
  if(kind=="crafting"){
   Probe bone(session.inventory.windowId);bone.Screen::init(game,size.first,size.second);
   bone.onItemSelected(nullptr,2);session.inventory.slots[0]=large.result;++session.inventoryRevision;bone.tick();
   sent.clear();bone.buttonClicked(find(bone,1));assert(sent.find("\"slot\":0")!=std::string::npos);
   session.inventory.slots[0]={};session.inventory.carried=large.result;++session.inventoryRevision;sent.clear();bone.tick();assert(sent.find("\"slot\":37")!=std::string::npos);
   session.inventory.carried={};++session.inventoryRevision;bone.tick();
  }
  // Removed IDs cannot silently retarget the recipe occupying their old row.
  session.recipes.erase(session.recipes.begin()+1);++session.recipesRevision;ui.tick();sent.clear();ui.buttonClicked(find(ui,1));assert(sent.empty());
  ++session.inventory.menuGeneration;sent.clear();ui.onItemSelected(nullptr,0);assert(sent.empty());
 }
 // Secondary fingers must not overwrite or release the menu cursor; a fresh
 // menu permits a new finger even while the world movement thumb stays down.
 {TouchMouseOwner owner;owner.observe(0);assert(owner.down(2));assert(!owner.down(5));assert(!owner.move(5)&&!owner.up(5)&&owner.move(2));owner.observe(7);assert(!owner.up(2));assert(owner.down(5));assert(!owner.move(2));assert(owner.up(5));assert(owner.down(9));owner.cancel();assert(!owner.up(9));}
 // Real touch recognizer: air taps use held menu items; short entity taps
 // still attack, while a deliberate hold interacts exactly once.
 {game->width=400;game->height=240;TouchInputHolder holder(game,&game->options);auto* turn=dynamic_cast<UnifiedTurnBuild*>(holder.getBuildInput());assert(turn);int tx=-1,ty=-1;for(int y=30;y<180&&tx<0;y+=10)for(int x=150;x<350;x+=10)if(turn->isInsideArea(x,y)){tx=x;ty=y;break;}assert(tx>=0);BuildActionIntention gesture;
 auto reset=[](){Multitouch::reset();Multitouch::resetThisUpdate();Multitouch::commit();};
 game->hitResult=HitResult();Multitouch::feed(1,MouseAction::DATA_DOWN,tx,ty,1);turn->tickBuild(nullptr,&gesture);Multitouch::feed(1,MouseAction::DATA_UP,tx,ty,1);assert(turn->tickBuild(nullptr,&gesture)&&gesture.isInteract()&&!gesture.isAttack()&&!gesture.isRemove());reset();
 game->hitResult.type=ENTITY;Multitouch::feed(1,MouseAction::DATA_DOWN,tx,ty,1);turn->tickBuild(nullptr,&gesture);Multitouch::feed(1,MouseAction::DATA_UP,tx,ty,1);assert(turn->tickBuild(nullptr,&gesture)&&gesture.isAttack()&&!gesture.isInteract());reset();
 Multitouch::feed(1,MouseAction::DATA_DOWN,tx,ty,1);Multitouch::commit();turn->getTurnDelta();turn->tickBuild(nullptr,&gesture);usleep(450000);turn->getTurnDelta();assert(turn->tickBuild(nullptr,&gesture)&&gesture.isInteract()&&!gesture.isRemove()&&!gesture.isAttack());assert(!turn->tickBuild(nullptr,&gesture));Multitouch::feed(1,MouseAction::DATA_UP,tx,ty,1);Multitouch::commit();turn->getTurnDelta();turn->tickBuild(nullptr,&gesture);reset();game->hitResult=HitResult();}
 std::cout<<"Mobile taps: menu absent-pointer frame, single-finger ownership/context/cancel, Java air use, entity tap attack and one held interaction PASS"<<std::endl;
 // Exercise the real keyboard queue, not just a direct virtual callback. Keep
 // screen changes deferred as in Minecraft's event loop; no native window exists.
 game->*field(ScreenMutexTag{})=true;
 // Storage is an explicit presentation switch, independent of names/items.
 class StorageProbe:public JavaStorageScreen{public:explicit StorageProbe(int id):JavaStorageScreen(id){}using Screen::buttons;};
 auto storageButton=[](StorageProbe& ui,int id){for(auto* b:ui.buttons)if(b->id==id)return b;assert(false);return static_cast<Button*>(nullptr);};
 std::vector<std::pair<std::string,int>> storageKinds{{"hopper",5},{"shulker_box",27},{"generic_3x3",9}};for(int rows=1;rows<=6;++rows)storageKinds.push_back({"generic_9x"+std::to_string(rows),rows*9});
 for(const auto& kind:storageKinds)for(auto size:std::vector<std::pair<int,int>>{{400,240},{200,120}}){
  auto& inv=session.inventory;inv={};inv.windowId=7;inv.menuGeneration=91;inv.menuKind=kind.first;inv.title="§bA plugin menu, or an ordinary chest";inv.slots.resize(kind.second+36);
  auto range=[&](const char* key,int start,int end){JavaGameSession::SlotRange r;r.start=start;r.end=end;inv.slotRanges[key]=r;};range("container",0,kind.second);range("player_inventory",kind.second,kind.second+27);range("hotbar",kind.second+27,kind.second+36);
  inv.carried.count=3;inv.carried.resourceId="minecraft:stone";assert(JavaInventoryScreen::storageSupported(inv));auto broken=inv;broken.slotRanges["hotbar"].end--;assert(!JavaInventoryScreen::storageSupported(broken));broken=inv;broken.menuKind="merchant";assert(!JavaInventoryScreen::storageSupported(broken));
  game->width=400;game->height=240;Gui::GuiScale=400.f/size.first;Gui::InvGuiScale=1.f/Gui::GuiScale;
  inv.slots[3]=inv.carried;inv.slots[3].originalId=1;inv.slots[kind.second+27]=inv.carried;inv.slots[kind.second+27].resourceId="minecraft:compass";inv.slots[kind.second+27].originalId=345;inv.slots[kind.second+27].count=1;
  SemanticInventoryProbe popup(7);popup.Screen::init(game,size.first,size.second);assert(popup.mappedSlotForButton(0)==3);assert(button(popup,-6)->msg=="Storage");{auto* choice=button(popup,0);const int x=choice->x+choice->width/2,y=choice->y+choice->height/2;sent.clear();sentCalls=0;popup.mouseClicked(x,y,1);projection();popup.render(-9999,-9999,0);popup.mouseReleased(x,y,1);assert(sentCalls==1&&sent.find("\"slot\":3")!=std::string::npos);}
sent.clear();sentCalls=0;(game->*field(ScheduledTag{}))=false;popup.buttonClicked(button(popup,-6));assert(sent.empty()&&sentCalls==0);assert(dynamic_cast<JavaStorageScreen*>(game->*field(NextScreenTag{})));assert(JavaInventoryViewPreference::storage()&&JavaInventoryViewPreference::read());
  StorageProbe drawer(7);drawer.Screen::init(game,size.first,size.second);assert(drawer.slotAt(false,0)==0&&drawer.slotAt(false,kind.second-1)==kind.second-1&&drawer.slotAt(true,0)==kind.second+27&&drawer.slotAt(true,9)==kind.second);assert(drawer.slotAt(false,kind.second)==-1&&drawer.slotAt(true,36)==-1);assert(drawer.getItems(drawer.paneFor(false)).size()==size_t(kind.second)&&drawer.getItems(drawer.paneFor(true)).size()==36);
  sent.clear();assert(drawer.addItem(drawer.paneFor(false),kind.second-1));assert(sent.find("\"slot\":"+std::to_string(kind.second-1)+",")!=std::string::npos);sent.clear();assert(drawer.addItem(drawer.paneFor(true),0));assert(sent.find("\"slot\":"+std::to_string(kind.second+27)+",")!=std::string::npos);
  drawer.buttonClicked(storageButton(drawer,5));sent.clear();drawer.addItem(drawer.paneFor(false),0);assert(sent.find("\"right\":true")!=std::string::npos);drawer.buttonClicked(storageButton(drawer,5));sent.clear();drawer.addItem(drawer.paneFor(true),9);assert(sent.find("\"type\":\"quick_move\"")!=std::string::npos);
  sent.clear();sentCalls=0;drawer.buttonClicked(storageButton(drawer,4));assert(sent.empty()&&sentCalls==0&&dynamic_cast<JavaInventoryScreen*>(game->*field(NextScreenTag{})));assert(inv.carried.count==3&&inv.windowId==7&&inv.menuGeneration==91);assert(!JavaInventoryViewPreference::storage()&&!JavaInventoryViewPreference::read());
  projection();glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,size.first,size.second,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();drawer.render(80,70,0);assert(glGetError()==GL_NO_ERROR);if(kind.first=="generic_9x3"||kind.first=="generic_9x6")save(pixels(),"/home/truancy/Documents/Codex/2026-10-13/referenced-chatgpt-conversation-this-is-an/work/native-storage-"+kind.first+"-"+std::to_string(size.first)+".png");
  ++inv.menuGeneration;sent.clear();sentCalls=0;assert(!drawer.addItem(drawer.paneFor(false),0));drawer.buttonClicked(storageButton(drawer,5));assert(sent.empty()&&sentCalls==0);
 }
 // Real menu routing retains the explicit preference across new container
 // generations, reconnect-style IDs and options saves, then resets on Menu.
 game->level=new JavaLevel(new MemoryLevelStorage(),session.chunks);session.dead=false;
 {TouchInputHolder holder(game,&game->options);auto* turn=dynamic_cast<UnifiedTurnBuild*>(holder.getBuildInput());assert(turn);int tx=200,ty=70;assert(turn->isInsideArea(tx,ty));BuildActionIntention action;session.spawned=true;session.targetingEpoch=77;session.worldRevision=13;session.dead=false;session.pickInputRevision=20;session.pickRayEnabled=true;session.pickRaySent=true;
 // Three complete gestures in one tick survive finger-up without a current hit.
 Multitouch::reset();Multitouch::resetThisUpdate();for(int i=0;i<3;++i){Multitouch::feed(1,MouseAction::DATA_DOWN,tx+i,ty,1);Multitouch::feed(1,MouseAction::DATA_UP,tx+i,ty,1);}Multitouch::commit();sentCalls=0;turn->tickBuild(nullptr,&action);assert(turn->javaTaps.taps.size()==3&&sentCalls==0);Multitouch::reset();Multitouch::resetThisUpdate();Multitouch::commit();
 session.entities[900].id=900;
#ifdef TCPE_TEST_MOBILE_PICK
 struct CameraMob:Mob{explicit CameraMob(Level* l):Mob(l){}int getEntityTypeId()const override{return 0;}};CameraMob camera(game->level);camera.health=20;game->cameraTargetPlayer=&camera;game->inputHolder=&holder;GameRenderer renderer(game);for(int i=0;i<16;++i){(renderer.*field(ModelTag{}))[i]=(i%5==0)?1:0;(renderer.*field(ProjTag{}))[i]=(i%5==0)?1:0;}
 renderer.pick(0);assert(turn->javaTaps.locked(session.pickInputRevision));const auto capturedRevision=session.pickInputRevision;(renderer.*field(ModelTag{}))[12]=.01f;renderer.pick(0);assert(session.pickInputRevision==capturedRevision); // Render/camera jitter cannot invalidate a pending tap.
#endif

 for(int i=0;i<3;++i){assert(turn->javaTaps.taps.front().x==tx+i);
#ifdef TCPE_TEST_MOBILE_PICK
 renderer.pick(0);assert(turn->javaTaps.locked(session.pickInputRevision));
#else
 ++session.pickInputRevision;turn->javaTaps.bind(session.pickInputRevision);
#endif
session.targeting.epoch=77;session.targeting.world=13;session.targeting.age=0;session.targeting.inputRevision=session.pickInputRevision-1;session.targeting.kind="entity";session.targeting.entityId=900;
 unsigned before=sentCalls;turn->tickBuild(nullptr,&action);assert(sentCalls==before);session.targeting.inputRevision=session.pickInputRevision;turn->tickBuild(nullptr,&action);assert(sentCalls==before+1&&sent.find("\"type\":\"attack\"")!=std::string::npos&&sent.find("900")!=std::string::npos);}
 assert(turn->javaTaps.taps.empty());turn->javaTaps.push(tx,ty,getTimeS(),77,13);session.worldRevision=14;assert(!turn->pendingJavaTap());turn->javaTaps.push(tx,ty,getTimeS()-1,77,14);assert(!turn->pendingJavaTap());turn->javaTaps.push(tx,ty,getTimeS(),77,14);turn->cancelJavaTaps();assert(!turn->pendingJavaTap());
 JavaTouchTapQueue bounded;for(int i=0;i<8;++i)assert(bounded.push(i,0,0,1,1));assert(!bounded.push(9,0,0,1,1));bounded.prune(1,1,1);assert(bounded.taps.empty());
#ifdef TCPE_TEST_MOBILE_PICK
 game->inputHolder=nullptr;game->cameraTargetPlayer=nullptr;
#endif
 session.entities.erase(900);session.spawned=false;session.clearTargeting();session.targetingEpoch=0;session.worldRevision=0;std::cout<<"Tap trace: 3 release gestures / 3 captured positions / 0 stale-target commands / 3 exact attacks; expiry/world/menu cancellation and bounded queue PASS"<<std::endl;}

 JavaInventoryViewPreference::setStorage(true);OptionsFile().save(StringVector{"unrelated_setting:kept"});assert(JavaInventoryViewPreference::read());
 for(int id:{8,9}){session.inventory.windowId=id;++session.inventory.menuGeneration;game->screen=nullptr;(game->*field(ScheduledTag{}))=false;sent.clear();sentCalls=0;assert(showJavaInventory(game,true));auto* next=dynamic_cast<JavaStorageScreen*>(game->*field(NextScreenTag{}));assert(next&&next->window()==id&&sentCalls==0);game->screen=next;next->Screen::init(game,400,240);(game->*field(ScheduledTag{}))=false;assert(showJavaInventory(game,true));assert(!(game->*field(ScheduledTag{})));game->screen=nullptr;delete next;}
 JavaInventoryViewPreference::setStorage(false);assert(!JavaInventoryViewPreference::read());session.inventory.windowId=10;++session.inventory.menuGeneration;(game->*field(ScheduledTag{}))=false;sent.clear();sentCalls=0;assert(showJavaInventory(game,true));assert(dynamic_cast<JavaInventoryScreen*>(game->*field(NextScreenTag{}))&&sentCalls==0);
 const auto preserved=OptionsFile().getOptionStrings();auto hasPair=[&](const char* key,const char* value){for(size_t i=0;i+1<preserved.size();i+=2)if(preserved[i]==key&&preserved[i+1]==value)return true;return false;};assert(hasPair("mp_username","Fixture")&&hasPair("mp_saved_server_0_address","fixture.invalid")&&hasPair("unrelated_setting","kept"));
 std::cout<<"Sticky storage preference: actual new-window/reconnect-generation routing, explicit Menu reset, disk reload and preservation through other settings saves PASS"<<std::endl;
 std::cout<<"Native PE storage: default GUI/explicit Storage/Menu, all empty/player/hotbar slots, whole/half/move, metadata guards, no packet on view switch and nine menu geometries at mobile/desktop sizes PASS"<<std::endl;

 auto cancel=[&](Screen& ui,int key,const std::string& expected){
  (game->*field(ScheduledTag{}))=false;sent.clear();sentCalls=0;Keyboard::reset();
  Keyboard::feed(key,KeyboardAction::KEYDOWN);ui.updateEvents();
  assert(sent==expected&&sentCalls==1&&(game->*field(ScheduledTag{}))&&(game->*field(NextScreenTag{}))==nullptr);
  assert(Keyboard::isKeyDown(key));for(int frame=0;frame<120;++frame)ui.updateEvents();
  assert(sentCalls==1);Keyboard::reset();Keyboard::feed(key,KeyboardAction::KEYUP);ui.updateEvents();assert(sentCalls==1);Keyboard::reset();
 };
 for(const std::string kind:{std::string("player"),std::string("crafting"),std::string("furnace")}){
  setup(kind);game->width=400;game->height=240;Gui::GuiScale=Gui::InvGuiScale=1;
  SemanticInventoryProbe popup(session.inventory.windowId);popup.Screen::init(game,400,240);
  sent.clear();popup.buttonClicked(button(popup,-4));const std::string expected=sent;
  assert(expected.find("\"type\":\"close_container\"")!=std::string::npos);
  for(int key:{Keyboard::KEY_ESCAPE,int(ControllerKeys::MenuCancel),Keyboard::KEY_K}){game->options.keyMenuCancel.key=Keyboard::KEY_K;cancel(popup,key,expected);}
  ++session.inventory.menuGeneration;sent.clear();(game->*field(ScheduledTag{}))=false;popup.keyPressed(Keyboard::KEY_ESCAPE);assert(sent.empty()&&!(game->*field(ScheduledTag{})));
  if(kind!="furnace"){
   setup(kind);Probe ui(session.inventory.windowId);ui.Screen::init(game,400,240);
   sent.clear();ui.buttonClicked(find(ui,2));const std::string craftClose=sent;
   assert(craftClose.find("\"type\":\"close_container\"")!=std::string::npos);
   for(int key:{Keyboard::KEY_ESCAPE,int(ControllerKeys::MenuCancel),Keyboard::KEY_K})cancel(ui,key,craftClose);
   ++session.inventory.menuGeneration;sent.clear();(game->*field(ScheduledTag{}))=false;ui.keyPressed(Keyboard::KEY_ESCAPE);assert(sent.empty()&&!(game->*field(ScheduledTag{})));
  }
 }
 // Real native polling scheduler with controlled completion and clock, including
 // offscreen entries. No DNS, game input or public server traffic in this test.
 std::vector<JavaServerRow::Endpoint> endpoints;for(int i=0;i<10;++i)endpoints.push_back({"fixture-"+std::to_string(i)+".invalid",25565});endpoints.push_back(endpoints.front());
 auto refresh=[&](int ms){statusClock=ms;JavaServerRow::refreshAt(game,endpoints,std::chrono::steady_clock::time_point{}+std::chrono::milliseconds(ms));};
 refresh(0);assert(statusPending.size()==4);refresh(999);assert(statusPending.size()==4&&statusStarts.size()==4);
 for(int ms=1000;ms<=6000;ms+=50){for(auto* pending:statusPending)pending->ready=true;refresh(ms);assert(statusPending.size()<=4);}
 assert(statusStarts.size()==10);for(const auto& host:statusStarts){assert(host.second.size()>=5);for(size_t i=1;i<host.second.size();++i)assert(host.second[i]-host.second[i-1]>=1000);}
 const auto before=statusStarts;for(auto* pending:statusPending){pending->ready=true;pending->failed=true;}refresh(7000);for(int ms=7050;ms<=8200;ms+=50){for(auto* pending:statusPending)pending->ready=true;refresh(ms);}for(const auto& host:before)assert(statusStarts[host.first].size()>host.second.size());
 JavaServerRow::refreshAt(game,{},std::chrono::steady_clock::time_point{}+std::chrono::milliseconds(9000));assert(statusPending.empty()&&statusActive.empty());
 const auto stopped=statusStarts;projection();JavaServerRow::render(game,"fixture-0.invalid",25565,"Fixture",false,0,390,0);assert(statusStarts==stopped);
 std::cout<<"Server-list refresh: one-second interval, all ten offscreen/visible endpoints, duplicate suppression, four-request bound, fair scheduling, slow requests and offline retry PASS"<<std::endl;
 assert(glGetError()==GL_NO_ERROR);std::cout<<"HEADLESS EGL pbuffer: actual native 2x2/3x3 crafting selection/carry/ghost/All/resize/category/ID removal, One auto-bag, full bag, rejection, bone-block and wait/retry and server-menu/crafting Close parity with held keyboard Escape/controller/configured cancel PASS; no native window created"<<std::endl;std::_Exit(0);
}
