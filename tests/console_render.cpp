#include "client/gui/screens/ConsoleScreen.h"
#include "client/gui/screens/ChatScreen.h"
#include "client/Minecraft.h"
#include "client/gui/Font.h"
#include "client/gui/components/Button.h"
#include "world/item/Item.h"
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
#include <thread>
#include "network/java/JavaLocalPlayer.h"
#include "network/java/JavaLevel.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
class ChatPlatformProbe:public AppPlatform_glfw{public:int shown=0,hidden=0,dialogs=0;void showKeyboard()override{++shown;AppPlatform_glfw::showKeyboard();}void hideKeyboard()override{++hidden;AppPlatform_glfw::hideKeyboard();}void showDialog(int)override{++dialogs;}};
static AppPlatform* fixturePlatform;
class GameProbe:public Minecraft{public:GameProbe():Minecraft(){}AppPlatform* platform()override{return fixturePlatform;}};
class ChatProbe:public ConsoleScreen{public:using Screen::buttons;};
static std::string sent;
extern "C" TcpeJavaSession* tcpe_java_connect(const char*,size_t,const char*,size_t,bool){return reinterpret_cast<TcpeJavaSession*>(1);}
extern "C" void tcpe_java_close(TcpeJavaSession*){}
extern "C" bool tcpe_java_command(TcpeJavaSession*,const char* text,size_t length){sent.assign(text,length);return true;}
#include "platform/input/Keyboard.h"

int main(int argc,char**argv){
 assert(argc>=4);assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
 auto* window=glfwCreateWindow(800,480,"Native chat",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();
 ChatPlatformProbe platform;fixturePlatform=&platform;Options options;Textures textures(&options,&platform);Font font(&options,"font/default8.png",&textures);font.setJavaTextEnabled(true);
 auto* game=new GameProbe();game->textures=&textures;game->font=&font;game->options.useTouchScreen=true;game->javaSession=new JavaGameSession();game->javaSession->chatActionsPending=true;game->javaSession->connection.connect("fixture","Test",false);
 game->gui.addMessage(u8"§x§2§3§e§0§f§4§lZeusPvP §rWelcome to native TCPE chat.");
 game->gui.addMessage(u8"§aOnline players: §f2 §7• §bUse /hub to switch games.");
 game->gui.addMessage(u8"§fA long server message keeps its full width and wraps naturally on a phone while preserving §x§f§f§4§4§2§2hex colors§r, Unicode 雪 and readable spacing.");
 ChatProbe chat;chat.Screen::init(game,400,240);
 auto find=[&](int id)->Button*{for(auto* b:chat.buttons)if(b->id==id)return b;return nullptr;};
 auto frame=[&](const char*name,int w,int h){chat.Screen::init(game,w,h);glViewport(0,0,800,480);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,float(w),float(h),0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable2(GL_DEPTH_TEST);glEnable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);chat.render(-1,-1,0);glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char>p(800*480*4),flipped(p.size());glReadPixels(0,0,800,480,GL_RGBA,GL_UNSIGNED_BYTE,p.data());for(int row=0;row<480;++row)std::copy(p.begin()+row*800*4,p.begin()+(row+1)*800*4,flipped.begin()+(479-row)*800*4);png_image image{};image.version=PNG_IMAGE_VERSION;image.width=800;image.height=480;image.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&image,name,0,flipped.data(),0,nullptr));return p;};
 frame(argv[1],400,240);frame(argv[2],200,160);
 sent.clear();for(char c:std::string("   "))chat.keyboardNewChar(c);chat.keyPressed(Keyboard::KEY_RETURN);assert(sent.empty());for(int i=0;i<3;++i)chat.keyPressed(Keyboard::KEY_BACKSPACE);
 for(char c:std::string(u8"hello 雪😀"))chat.keyboardNewChar(c);chat.keyPressed(Keyboard::KEY_BACKSPACE);chat.buttonClicked(find(4));assert(sent.find(u8"hello 雪")!=std::string::npos);assert(sent.find(u8"😀")==std::string::npos);
 for(int i=0;i<40;++i)game->gui.addMessage("History message "+std::to_string(i));chat.Screen::init(game,200,160);chat.render(-1,-1,0);assert(find(2)->active&&!find(3)->active);sent.clear();chat.mouseClicked(30,50,MouseAction::ACTION_LEFT);chat.mouseReleased(30,100,MouseAction::ACTION_LEFT);chat.render(-1,-1,0);assert(find(3)->active&&sent.empty());chat.buttonClicked(find(3));chat.render(-1,-1,0);assert(!find(3)->active);
 delete game->javaSession;game->javaSession=nullptr;font.setJavaTextEnabled(false);game->gui.addMessage("[red]Offline red[/red] and [green]native green[/green] chat.");const auto pixels=frame(argv[3],400,240);int red=0,green=0;for(size_t i=0;i<pixels.size();i+=4){if(pixels[i]>240&&pixels[i+1]<10&&pixels[i+2]<10)++red;if(pixels[i]<10&&pixels[i+1]>240&&pixels[i+2]<10)++green;}assert(red>10&&green>10);
 JavaChunkStore entryChunks;auto* entryLevel=new JavaLevel(new MemoryLevelStorage(),entryChunks);game->level=entryLevel;game->width=800;game->height=480;
 for(bool java:{false,true}){if(java)game->javaSession=new JavaGameSession();sent.clear();const int shown=platform.shown,hidden=platform.hidden;game->setScreen(new ChatScreen());assert(dynamic_cast<ConsoleScreen*>(game->screen));assert(platform.shown==shown+1&&platform.isKeyboardVisible());assert(platform.dialogs==0&&sent.empty());game->screen->handleBackEvent(true);assert(game->screen==nullptr&&!platform.isKeyboardVisible());assert(platform.hidden==hidden+1&&sent.empty());if(java){delete game->javaSession;game->javaSession=nullptr;}}
 game->level=nullptr;delete entryLevel;
 std::cout<<"Native chat GPU PASS: rich Java/offline color, mobile/desktop layout, Unicode send/backspace, swipe/history/Latest, no gesture command"<<std::endl;std::_Exit(0);
}
