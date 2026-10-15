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
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_TRUE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);auto* window=glfwCreateWindow(400,240,"TCPE custom head rendering verification",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);Font font(&options,"font/default8.png",&textures);font.setJavaTextEnabled(true);

 JavaGameSession session;JavaGameSession::Item head;head.resourceId="minecraft:player_head";head.count=1;head.hasProfile=true;head.profileSkinUrl="https://textures.minecraft.net/texture/"+std::string(64,'a');
 auto image=[&](int h,bool hat){TextureData data;data.w=64;data.h=h;data.numBytes=64*h*4;data.transparent=true;data.data=new unsigned char[data.numBytes]();for(int y=0;y<16;++y)for(int x=0;x<32;++x){auto i=(y*64+x)*4;data.data[i]=255;data.data[i+3]=255;}if(hat)for(int y=0;y<16;++y)for(int x=32;x<64;++x){auto i=(y*64+x)*4;data.data[i+2]=255;data.data[i+3]=255;}std::string key="headfixture/"+std::to_string(h)+(hat?"hat":"base");textures.assignTexture(key,data);return key;};
 auto frame=[&](bool hostileDepth=false){projection();if(hostileDepth){glClearDepthf(0);glClear(GL_DEPTH_BUFFER_BIT);glClearDepthf(1);}glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,400,240,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glEnable2(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,1.f);bool drawn=JavaItemIcons::render(&textures,head.resourceId,120,40,120,&head);GLfloat ref=0;glGetFloatv(GL_ALPHA_TEST_REF,&ref);assert(ref==1.f);auto p=pixels();glfwSwapBuffers(window);glfwPollEvents();return std::make_pair(drawn,p);};
 assert(!frame().first);session.inventory.held=head;session.inventory.hotbar[8]=head;session.inventory.slots.push_back(head);session.inventory.carried=head;JavaGameSession::Entity entity{};entity.droppedItem=head;entity.equipment[0]=head;entity.display.item=head;session.entities.emplace(1,entity);
 for(int h:{32,64}){session.skinTextures[head.profileSkinUrl]=image(h,false);session.skinHeights[head.profileSkinUrl]=h;session.refreshItemSkins();head=session.inventory.held;assert(head.profileSkinHeight==h&&head.profileSkinTexture==session.skinTextures[head.profileSkinUrl]);assert(session.inventory.hotbar[8].profileSkinTexture==head.profileSkinTexture&&session.inventory.carried.profileSkinTexture==head.profileSkinTexture&&session.entities[1].droppedItem.profileSkinTexture==head.profileSkinTexture&&session.entities[1].equipment[0].profileSkinTexture==head.profileSkinTexture&&session.entities[1].display.item.profileSkinTexture==head.profileSkinTexture);auto base=frame();assert(base.first);auto staleDepth=frame(true);assert(staleDepth.first);assert(base.second==staleDepth.second);size_t reds=0;for(size_t i=0;i<base.second.size();i+=4)if(base.second[i]>200&&base.second[i+2]<10)++reds;assert(reds>500);session.skinTextures[head.profileSkinUrl]=image(h,true);session.refreshItemSkins();head=session.inventory.held;auto outer=frame();assert(outer.first&&outer.second!=base.second);size_t blues=0;for(size_t i=0;i<outer.second.size();i+=4)if(outer.second[i+2]>200&&outer.second[i]<10)++blues;assert(blues>500);saveHotbar(outer.second,"/home/truancy/Documents/Codex/2026-10-13/referenced-chatgpt-conversation-this-is-an/work/custom-head-"+std::to_string(h)+".png");}
 session.skinTextures.clear();session.skinHeights.clear();session.refreshItemSkins();head=session.inventory.held;assert(head.profileSkinTexture.empty()&&!frame().first);head.hasProfile=false;assert(frame().first);std::cout<<"Visible custom head GPU: 64x32/64x64 base and hat, unresolved profile safe, authoritative cache updates all item contexts, alpha restore PASS"<<std::endl;glfwDestroyWindow(window);std::_Exit(0);}
