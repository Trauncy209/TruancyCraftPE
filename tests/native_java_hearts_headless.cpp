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
#include "headless_egl.h"

static bool pending=false;static std::string variant="normal";static double hp=20,maximum=20,gold=0,frozen=-1,invulnerable=0;
extern "C" bool tcpe_java_text(const TcpeJavaSession*,const char*,size_t,TcpeJavaText*){return false;}
extern "C" bool tcpe_java_array_size(const TcpeJavaSession*,const char*,size_t,size_t*){return false;}
extern "C" TcpeJavaSession* tcpe_java_connect(const char*,size_t,const char*,size_t,bool){return reinterpret_cast<TcpeJavaSession*>(1);}
extern "C" void tcpe_java_close(TcpeJavaSession*){}
extern "C" int tcpe_java_poll(TcpeJavaSession*,TcpeJavaEvent* e){if(!pending)return 0;pending=false;*e=TcpeJavaEvent{};e->kind=TCPE_JAVA_VITALS;return 1;}
extern "C" bool tcpe_java_boolean(const TcpeJavaSession*,const char* p,size_t n,bool* out){std::string k(p,n);if(k=="vitals.poisoned"||k=="vitals.withered"||k=="vitals.hardcore"||k=="vitals.regenerating"){*out=k=="vitals."+variant||(variant=="poison_wither"&&(k=="vitals.poisoned"||k=="vitals.withered"));return true;}return false;}
extern "C" bool tcpe_java_number(const TcpeJavaSession*,const char* p,size_t n,double* out){std::string k(p,n);if(k=="vitals.health"){*out=hp;return true;}if(k=="vitals.max_health"){*out=maximum;return true;}if(k=="vitals.food"){*out=20;return true;}if(k=="vitals.absorption"){*out=gold;return true;}if(k=="vitals.invulnerable_ticks"){*out=invulnerable;return true;}if(k=="vitals.frozen_ticks"){*out=frozen>=0?frozen:variant=="frozen"?140:0;return true;}return false;}
class GameProbe:public Minecraft{public:GameProbe():Minecraft(){}};
static std::vector<unsigned char> capture(Gui& gui){glViewport(0,0,400,240);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,400,240,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable2(GL_DEPTH_TEST);glEnable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);gui.renderJavaVitals(400,240);glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> p(400*240*4);glReadPixels(0,0,400,240,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;}
int main(){std::cout.setf(std::ios::unitbuf);NativeHeadlessEGL offscreen(400,240);Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);Font font(&options,"font/default8.png",&textures);auto* game=new GameProbe();game->textures=&textures;game->font=&font;game->javaSession=new JavaGameSession();game->level=new JavaLevel(new MemoryLevelStorage(),game->javaSession->chunks);game->player=new LocalPlayer(game,game->level,nullptr,0,false);Gui gui(game);game->javaSession->connection.connect("fixture","Fixture",false);game->javaSession->spawned=true;

 auto event=[&](){pending=true;game->javaSession->tick();};
 auto save=[&](const char* name,const std::vector<unsigned char>& pixels){std::vector<unsigned char> flipped(pixels.size());for(int r=0;r<240;++r)std::copy(pixels.begin()+r*400*4,pixels.begin()+(r+1)*400*4,flipped.begin()+(239-r)*400*4);png_image im{};im.version=PNG_IMAGE_VERSION;im.width=400;im.height=240;im.format=PNG_FORMAT_RGBA;const std::string path="/home/truancy/Documents/Codex/2026-10-13/referenced-chatgpt-conversation-this-is-an/work/heart-"+std::string(name)+".png";assert(png_image_write_to_file(&im,path.c_str(),0,flipped.data(),0,nullptr));};
 std::vector<unsigned char> normal,poison,wither;
 for(const char* state:{"normal","poisoned","withered","frozen","hardcore"}){variant=state;event();auto pixels=capture(gui);if(normal.empty())normal=pixels;else assert(pixels!=normal);if(variant=="poisoned")poison=pixels;if(variant=="withered")wither=pixels;save(state,pixels);std::cout<<state<<" real native framebuffer rendered\n";}
 variant="poison_wither";event();assert(capture(gui)==poison);variant="normal";frozen=139;event();assert(capture(gui)==normal);frozen=140;event();assert(capture(gui)!=normal);frozen=-1;
 variant="normal";gold=4;event();auto absorbing=capture(gui);variant="withered";event();auto witherGold=capture(gui);bool absorptionDiff=false;for(int y=12;y<21;++y)for(int x=2;x<19;++x)for(int c=0;c<4;++c){const size_t offset=((239-y)*400+x)*4+c;if(absorbing[offset]!=witherGold[offset])absorptionDiff=true;}assert(absorptionDiff);save("wither-absorption",witherGold);
 variant="normal";gold=0;hp=20;event();capture(gui);hp=18;invulnerable=20;event();auto damageNoFlash=capture(gui);for(int i=0;i<5;++i)gui.tick();auto damageFlash=capture(gui);assert(damageNoFlash!=damageFlash);save("damage-blink",damageFlash);
 variant="regenerating";hp=20;invulnerable=0;event();auto regen=capture(gui);variant="normal";event();auto noRegen=capture(gui);assert(regen!=noRegen);save("regeneration",regen);
 JavaHeartState timing;timing.update(1,20,0,0,0);timing.update(1,18,20,1,50);assert(timing.display==20&&timing.blinkUntil==21);assert(timing.blinking(6));timing.update(1,19,15,6,300);assert(timing.blinkUntil==16&&timing.display==20);timing.update(1,19,0,30,1301);assert(timing.display==19&&!timing.blinking(30));timing.update(2,10,20,31,1400);assert(timing.display==10&&!timing.blinking(31));timing.reset();assert(!timing.initialized);
 maximum=40;hp=40;++game->javaSession->worldRevision;event();capture(gui);maximum=20;hp=20;invulnerable=20;event();auto delayed=capture(gui);bool delayedBackground=false;for(int y=12;y<21;++y)for(int x=2;x<82;++x){const auto offset=((239-y)*400+x)*4;if(delayed[offset]||delayed[offset+1]||delayed[offset+2])delayedBackground=true;}assert(delayedBackground);save("maximum-decrease",delayed);invulnerable=0;
 game->javaSession->spawned=false;gui.renderJavaVitals(400,240);game->javaSession->spawned=true;hp=20;event();assert(capture(gui)==normal);
 game->javaSession->disconnect();assert(!game->javaSession->hearts.poisoned&&!game->javaSession->hearts.hardcore&&game->javaSession->hearts.frozenTicks==0);
 std::cout<<"HEADLESS actual native heart variants/priorities/frozen threshold/wither absorption/damage pixels/regen/timelines/reset PASS; no window\n";std::_Exit(0);}
