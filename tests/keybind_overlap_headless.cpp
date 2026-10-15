#include "headless_egl.h"
#include "client/Minecraft.h"
#include "client/gui/screens/OptionsScreen.h"
#include "client/player/LocalPlayer.h"
#include "client/player/input/KeyboardInput.h"
#include "client/player/input/touchscreen/TouchInputHolder.h"
#include "client/renderer/Textures.h"
#include "client/renderer/Tesselator.h"
#include "network/java/JavaLevel.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "util/Mth.h"
#include "AppPlatform_glfw.h"
#include "locale/I18n.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <cstdlib>
struct Draw{std::string text;int color;};static std::vector<Draw> draws;
extern "C" void __real__ZN4Font10drawShadowERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEffi(Font*,const std::string&,float,float,int);
extern "C" void __wrap__ZN4Font10drawShadowERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEffi(Font* f,const std::string& s,float x,float y,int color){draws.push_back({s,color});__real__ZN4Font10drawShadowERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEffi(f,s,x,y,color);}
class TestPlatform:public AppPlatform_glfw{public:float getPixelsPerMillimeter()override{return 10.f;}};
class Probe:public Minecraft{AppPlatform* testPlatform;public:explicit Probe(AppPlatform* p):Minecraft(),testPlatform(p){}AppPlatform* platform()override{return testPlatform;}};
class Settings:public OptionsScreen{public:using Screen::buttons;};
class Offline:public JavaLevel{public:using JavaLevel::JavaLevel;bool isJavaServerLevel()const override{return false;}};
static Button* button(Settings& ui,int id){for(auto* b:ui.buttons)if(b->id==id)return b;assert(false);return nullptr;}
static std::vector<unsigned char> render(Settings& ui,int w,int h){glViewport(0,0,400,240);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,w,h,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable2(GL_DEPTH_TEST);glDisable2(GL_LIGHTING);glDisable2(GL_FOG);glEnable2(GL_TEXTURE_2D);glEnable2(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.1);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);draws.clear();ui.render(-1,-1,0);glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> p(400*240*4);glReadPixels(0,0,400,240,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;}
static void dispatch(Probe* game){JavaChunkStore store;game->level=new Offline(new MemoryLevelStorage(),store);game->player=new LocalPlayer(game,game->level,nullptr,0,false);game->player->input=new KeyboardInput(&game->options);game->options.setKey(0,Keyboard::KEY_K);game->options.setKey(4,Keyboard::KEY_K);game->options.setKey(17,Keyboard::KEY_K);Keyboard::feed(Keyboard::KEY_K,KeyboardAction::KEYDOWN);game->tickInput();game->player->input->tick(game->player);std::cout<<"Actual tickInput overlap forward="<<game->player->input->ya<<" jump="<<game->player->input->jumping<<" perspective="<<game->options.thirdPersonView<<std::endl;assert(game->player->input->ya==1&&game->player->input->jumping&&game->options.thirdPersonView);Keyboard::feed(Keyboard::KEY_K,KeyboardAction::KEYUP);game->tickInput();game->player->input->tick(game->player);assert(game->player->input->ya==0&&!game->player->input->jumping);
 auto* holder=new TouchInputHolder(game,&game->options);game->inputHolder=holder;game->player->input=holder->getMoveInput();assert(dynamic_cast<HybridMoveInput*>(game->player->input));Keyboard::feed(Keyboard::KEY_K,KeyboardAction::KEYDOWN);game->tickInput();game->player->input->tick(game->player);assert(game->player->input->ya==1&&game->player->input->jumping);Keyboard::feed(Keyboard::KEY_K,KeyboardAction::KEYUP);game->tickInput();game->player->input->tick(game->player);assert(game->player->input->ya==0&&!game->player->input->jumping);game->options.setKey(0,0);game->options.setKey(4,0);game->player->input->setKey(0,true);game->player->input->tick(game->player);assert(game->player->input->ya==0&&!game->player->input->jumping);std::cout<<"Actual keyboard+touch hybrid overlapping press/release and unbound0 PASS"<<std::endl;}
int main(int argc,char**){std::cout.setf(std::ios::unitbuf);NativeHeadlessEGL offscreen(400,240);Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();TestPlatform platform;I18n::loadLanguage(&platform,"en_US");auto* game=new Probe(&platform);game->width=400;game->height=240;game->options.minecraft=game;game->options.initDefaultValues();game->options.useTouchScreen=true;Textures textures(&game->options,&platform);Font font(&game->options,"font/default8.png",&textures);game->textures=&textures;game->font=&font;Gui::GuiScale=Gui::InvGuiScale=1;
 if(argc>1){dispatch(game);std::_Exit(0);}Settings ui;ui.Screen::init(game,400,240);ui.selectCategory(4);ui.buttonClicked(button(ui,100));ui.keyPressed(Keyboard::KEY_SPACE);std::cout<<"UI forward binding="<<game->options.keyUp.key<<" expected overlapping jump="<<Keyboard::KEY_SPACE<<std::endl;assert(game->options.keyUp.key==Keyboard::KEY_SPACE&&!button(ui,100)->selected);auto pixels=render(ui,400,240);unsigned redLabels=0;bool warning=false;for(auto& d:draws){if((uint32_t(d.color)&0xffffff)==0xff7070)++redLabels;if(d.text.find("Warning:")==0)warning=true;}assert(redLabels==2&&warning);
 Options loaded;loaded.minecraft=game;loaded.initDefaultValues();loaded.update();assert(loaded.keyUp.key==Keyboard::KEY_SPACE&&loaded.keyJump.key==Keyboard::KEY_SPACE);
 ui.buttonClicked(button(ui,100));ui.keyPressed(Keyboard::KEY_ESCAPE);assert(game->options.keyUp.key==Keyboard::KEY_SPACE);ui.buttonClicked(button(ui,100));ui.keyPressed(Keyboard::KEY_BACKSPACE);assert(game->options.keyUp.key==0);render(ui,400,240);for(auto& d:draws)assert((uint32_t(d.color)&0xffffff)!=0xff7070);ui.buttonClicked(button(ui,122));assert(game->options.keyUp.key==Keyboard::KEY_W&&game->options.keyJump.key==Keyboard::KEY_SPACE);
 for(auto size:{std::make_pair(200,120),std::make_pair(160,96)}){Settings compact;compact.Screen::init(game,size.first,size.second);compact.selectCategory(4);compact.buttonClicked(button(compact,100));compact.keyPressed(Keyboard::KEY_SPACE);render(compact,size.first,size.second);bool highlighted=false;for(auto& d:draws)if((uint32_t(d.color)&0xffffff)==0xff7070)highlighted=true;assert(highlighted);for(auto* b:compact.buttons)if(b->visible)assert(b->x>=0&&b->y>=0&&b->x+b->width<=size.first&&b->y+b->height<=size.second);compact.buttonClicked(button(compact,122));}
 dispatch(game);std::cout<<"Headless settings duplicate save/reload, conflict colors+warning, clear/cancel/defaults, compact layouts, actual multi-action dispatch PASS"<<std::endl;std::_Exit(0);}
