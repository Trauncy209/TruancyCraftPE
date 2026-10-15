#include "network/java/JavaPackItemModels.h"
#include "network/java/JavaItemIcons.h"
#include "client/Minecraft.h"
#include "client/gui/Gui.h"
#include "client/gui/screens/JavaInventoryScreen.h"
#include "client/gamemode/SurvivalMode.h"
#include <png.h>
#include "client/player/LocalPlayer.h"
#include "client/renderer/TileRenderer.h"
#include "client/renderer/RenderChunk.h"
#define private public
#include "client/renderer/ItemInHandRenderer.h"
#undef private
class HeldGameProbe:public Minecraft{public:HeldGameProbe():Minecraft(){}};
#include "network/java/JavaEntityAdapter.h"
#include "network/java/JavaLevel.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "client/renderer/entity/EntityRenderDispatcher.h"
#include "world/level/tile/Tile.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "util/Mth.h"
#include "AppPlatform_glfw.h"
#include <cassert>
#include <iostream>
#include <set>
static std::string sent;
extern "C" TcpeJavaSession* tcpe_java_connect(const char*,size_t,const char*,size_t,bool){return reinterpret_cast<TcpeJavaSession*>(1);}
extern "C" void tcpe_java_close(TcpeJavaSession*){}
extern "C" bool tcpe_java_command(TcpeJavaSession*,const char* data,size_t len){sent.assign(data,len);return true;}
static JavaGameSession::PackModelQuad quad(int texture,int tint=-1){JavaGameSession::PackModelQuad q;q.texture=texture;q.tint=tint;q.vertices={{{{0,0,8,0,1}},{{16,0,8,1,1}},{{16,16,8,1,0}},{{0,16,8,0,0}}}};return q;}
static void projection(){glViewport(0,0,400,240);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(-.5f,.5f,-.125f,.475f,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable2(GL_LIGHTING);glDisable2(GL_FOG);glDisable2(GL_CULL_FACE);glDisable2(GL_SCISSOR_TEST);glEnable2(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_TRUE);glEnable2(GL_TEXTURE_2D);glDisable2(GL_ALPHA_TEST);glDisable2(GL_BLEND);glClearColor(0,0,0,1);glClearDepthf(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);}
static std::vector<unsigned char> pixels(){glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> p(400*240*4);glReadPixels(0,0,400,240,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;}
static std::array<unsigned char,3> color(const std::vector<unsigned char>& p,int x,int y){size_t i=((239-y)*400+x)*4;return {{p[i],p[i+1],p[i+2]}};}
static void saveHotbar(const std::vector<unsigned char>& p,const std::string& file){std::vector<unsigned char> flipped(p.size());for(int y=0;y<240;++y)std::copy(p.begin()+y*400*4,p.begin()+(y+1)*400*4,flipped.begin()+(239-y)*400*4);png_image image{};image.version=PNG_IMAGE_VERSION;image.width=400;image.height=240;image.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&image,file.c_str(),0,flipped.data(),0,nullptr));}
int main(){
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_TRUE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);auto* window=glfwCreateWindow(400,240,"Full native Java hotbar",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);Font font(&options,"font/default8.png",&textures);font.setJavaTextEnabled(true);
 JavaGameSession session;auto* level=new JavaLevel(new MemoryLevelStorage(),session.chunks);auto* game=new HeldGameProbe();game->textures=&textures;game->font=&font;game->level=level;game->player=new LocalPlayer(game,level,nullptr,0,false);game->gameMode=new SurvivalMode(game);game->options.useTouchScreen=true;game->javaSession=nullptr;Gui gui(game);int offlineCount=gui.getNumSlots(),offlineSelection=gui.getItemBarSelectionSize();game->javaSession=&session;session.phase="Game";session.inventory.slots.resize(46);assert(session.connection.connect("fixture.invalid","fixture",false));Gui::GuiScale=Gui::InvGuiScale=1;
 JavaGameSession::Item compass;compass.resourceId="minecraft:compass";session.compass.dimension="minecraft:overworld";session.compass.defaultSpawn.valid=true;session.compass.defaultSpawn.dimension=session.compass.dimension;session.compass.defaultSpawn.position={{100,64,0}};
 auto needle=[&](double yaw,unsigned long long tick){return JavaItemIcons::animatedResource(session,compass,.5,.5,yaw,tick);};
 std::string east;for(unsigned long long tick=1;tick<200;++tick)east=needle(0,tick);assert(east=="minecraft:compass_08");assert(needle(90,199)==east);std::string turned;for(unsigned long long tick=200;tick<400;++tick)turned=needle(90,tick);assert(turned=="minecraft:compass_00"&&turned!=east);assert(JavaItemIcons::has(turned));
 compass.hasLodestoneTracker=true;compass.lodestoneTarget=session.compass.defaultSpawn;compass.lodestoneTarget.position={{-100,64,0}};auto lodestone=needle(90,399);assert(lodestone!=turned);compass.lodestoneTarget.dimension="minecraft:the_nether";std::set<std::string> spinning;for(unsigned long long tick=400;tick<500;++tick)spinning.insert(needle(90,tick));assert(spinning.size()>3);
 JavaGameSession::Item recovery;recovery.resourceId="minecraft:recovery_compass";session.compass.lastDeath=session.compass.defaultSpawn;std::string recoveryFrame;for(unsigned long long tick=1;tick<200;++tick)recoveryFrame=JavaItemIcons::animatedResource(session,recovery,.5,.5,0,tick);assert(recoveryFrame=="minecraft:recovery_compass_08"&&JavaItemIcons::has(recoveryFrame));
 const std::vector<std::string> resources={"minecraft:diamond","minecraft:iron_ingot","minecraft:gold_ingot","minecraft:apple","minecraft:stick","minecraft:emerald","minecraft:name_tag","minecraft:comparator","minecraft:ender_eye"};
 auto fill=[&](bool populated){for(int i=0;i<9;++i){session.inventory.hotbar[i]={};if(populated){session.inventory.hotbar[i].resourceId=resources[i];session.inventory.hotbar[i].count=1;}}};
 auto frame=[&](int w,bool hostile){game->width=w;game->height=240;projection();glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,w,240,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glEnable2(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,hostile?1.f:.1f);gui.render(0,false,-1,-1);GLint func=0;GLfloat ref=0;glGetIntegerv(GL_ALPHA_TEST_FUNC,&func);glGetFloatv(GL_ALPHA_TEST_REF,&ref);assert(func==GL_GREATER&&std::abs(ref-(hostile?1.f:.1f))<.001f);auto snapshot=pixels();glfwSwapBuffers(window);glfwPollEvents();return snapshot;};
 for(bool touch:{false,true})for(int w:{400,200,160}){game->options.useTouchScreen=touch;assert(gui.getNumSlots()==9&&gui.getItemBarSelectionSize()==9);fill(false);auto empty=frame(w,true);fill(true);auto shown=frame(w,true);saveHotbar(frame(w,false),"/home/truancy/Documents/Codex/2026-10-13/referenced-chatgpt-conversation-this-is-an/work/hotbar-"+std::to_string(w)+(touch?"-touch":"-desktop")+".png");
  for(int slot=0;slot<9;++slot){int x=0,y=0,nextX=0,unused=0;gui.getSlotPos(slot,x,y);gui.getSlotPos(slot+1,nextX,unused);const int pitch=slot==8?std::min(20,std::max(1,(w-(touch?28:8))/9)):nextX-x;assert(x>=0&&x+pitch<=w);const int center=x+pitch/2;assert(gui.getSlotIdAt(center,y+10)==slot);size_t changed=0;for(int py=y;py<std::min(240,y+20);++py)for(int px=x;px<x+pitch;++px){int physicalX=px*400/w;auto a=color(empty,physicalX,py),b=color(shown,physicalX,py);if(a!=b)++changed;}assert(changed>2);sent.clear();gui.handleClick(1,center,y+10);assert(game->player->inventory->selected==slot&&sent=="{\"type\":\"select_hotbar\",\"slot\":"+std::to_string(slot)+"}");}
  if(touch){int x=0,y=0;gui.getSlotPos(9,x,y);assert(x>=0&&x+20<=w&&gui.getSlotIdAt(x+10,y+10)==9);auto normal=frame(w,false);size_t whiteGlyph=0;for(int py=y+5;py<y+12;++py)for(int px=x+3;px<x+17;++px){auto c=color(normal,px*400/w,py);if(c[0]>180&&c[1]>180&&c[2]>180)++whiteGlyph;}assert(whiteGlyph>=5);gui.handleClick(1,x+10,y+10);assert(dynamic_cast<JavaPlayerInventoryScreen*>(game->screen));game->setScreen(nullptr);}
  fill(true);auto count1=frame(w,false);session.inventory.hotbar[8].count=64;auto count64=frame(w,false);if(count1==count64){std::cerr<<"Authoritative modern item count overlay missing width="<<w<<" touch="<<touch<<std::endl;}assert(count1!=count64);
  session.inventory.hotbar[8].count=1;session.inventory.hotbar[8].maxDamage=100;session.inventory.hotbar[8].damage=0;auto pristine=frame(w,false);session.inventory.hotbar[8].damage=50;auto damaged=frame(w,false);assert(pristine!=damaged);session.inventory.hotbar[8].maxDamage=0;auto noDurability=frame(w,false);assert(noDurability==pristine);
 }
 game->javaSession=nullptr;assert(gui.getNumSlots()==offlineCount&&gui.getItemBarSelectionSize()==offlineSelection);
 std::cout<<"Full Gui hotbar GPU: nine icons/selection including slots6-8, separate touch inventory action, compact160/200 widths, authoritative counts, hostile alpha restoration and offline slot count preserved PASS"<<std::endl;std::_Exit(0);
}
