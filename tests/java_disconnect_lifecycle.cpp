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
#include "client/gui/screens/DisconnectionScreen.h"
#include "client/renderer/LevelRenderer.h"
#include "client/particle/ParticleEngine.h"
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
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_TRUE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);auto* window=glfwCreateWindow(400,240,"TCPE disconnect lifecycle verification",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);Font font(&options,"font/default8.png",&textures);font.setJavaTextEnabled(true);


 auto* game=new HeldGameProbe();game->textures=&textures;game->font=&font;game->width=400;game->height=240;game->levelRenderer=new LevelRenderer(game);game->particleEngine=new ParticleEngine(nullptr,&textures);game->javaSession=new JavaGameSession();game->javaSession->error="Login expired";game->javaSession->phase="Disconnected";game->javaSession->errorShown=true;game->level=new JavaLevel(new MemoryLevelStorage(),game->javaSession->chunks);game->player=new LocalPlayer(game,game->level,nullptr,0,false);game->updateJavaServer();assert(dynamic_cast<DisconnectionScreen*>(game->screen));auto render=[&](){projection();glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,400,240,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();game->screen->render(0,0,0);glfwSwapBuffers(window);glfwPollEvents();};render();game->setScreen(nullptr);game->updateJavaServer();assert(dynamic_cast<DisconnectionScreen*>(game->screen));render();dynamic_cast<DisconnectionScreen*>(game->screen)->keyPressed(Keyboard::KEY_ESCAPE);assert(!game->javaSession&&!game->level&&!game->player&&game->screen&&!dynamic_cast<DisconnectionScreen*>(game->screen));render();JavaChunkStore offlineChunks;game->level=new JavaLevel(new MemoryLevelStorage(),offlineChunks);game->setScreen(new DisconnectionScreen("Offline error"));render();dynamic_cast<DisconnectionScreen*>(game->screen)->keyPressed(Keyboard::KEY_ESCAPE);assert(!game->screen);std::cout<<"Visible disconnect lifecycle: Java error restores removed screen, Escape clears session/world/player to title, offline Escape unchanged PASS"<<std::endl;glfwDestroyWindow(window);std::_Exit(0);}
