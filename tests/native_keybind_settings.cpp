#include "network/java/JavaPackFontRuntime.h"
#include "network/java/JavaPackItemModels.h"
#include "network/java/JavaGameMode.h"
#include "client/gui/screens/JavaInventoryScreen.h"
#include "world/level/tile/Tile.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "util/Mth.h"
#include "AppPlatform_glfw.h"
#include <cassert>
#include <png.h>
#include <iostream>
#include "client/gui/screens/OptionsScreen.h"
#include "client/gui/screens/touch/TouchStartMenuScreen.h"
#include "client/player/input/KeyboardInput.h"
#include "platform/input/Keyboard.h"
#include "locale/I18n.h"
#include "network/java/JavaLocalPlayer.h"
#include "network/java/JavaLevel.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "client/player/input/touchscreen/TouchInputHolder.h"
#include "platform/input/AndroidKeyAdapter.h"
#include "client/gui/components/TextBox.h"
#include <fstream>
#include <unistd.h>
class OfflineFlagLevel:public JavaLevel {public:using JavaLevel::JavaLevel;bool isJavaServerLevel()const override{return false;}};
class PackGameProbe: public Minecraft {};
class ControlPlayer:public JavaLocalPlayer {public:using JavaLocalPlayer::JavaLocalPlayer;int crafts=0;void startCrafting(int,int,int,int)override{++crafts;}};
class FieldScreen:public Screen {public:using Screen::textBoxes;using Screen::keyPressed;using Screen::keyboardNewChar;};
static std::string sent;
extern "C" TcpeJavaSession* tcpe_java_connect(const char*,size_t,const char*,size_t,bool){return reinterpret_cast<TcpeJavaSession*>(1);}
extern "C" void tcpe_java_close(TcpeJavaSession*){}
extern "C" bool tcpe_java_account_status(TcpeJavaAccountStatus* out){if(out)*out=TcpeJavaAccountStatus{};return out!=nullptr;}
extern "C" bool tcpe_java_account_action(uint32_t){return false;}
extern "C" bool tcpe_java_uses_account(const TcpeJavaSession*){return false;}
extern "C" bool tcpe_java_command(TcpeJavaSession*,const char* data,size_t length){sent.assign(data,length);return true;}
class SettingsProbe: public OptionsScreen { public: SettingsProbe():OptionsScreen(false){} using Screen::buttons;using Screen::tabButtons;using Screen::tabButtonIndex; };
static Button* button(SettingsProbe& ui,int id){for(auto* b:ui.buttons)if(b->id==id)return b;assert(false);return nullptr;}
static void projection(int w,int h){glViewport(0,0,400,240);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,w,h,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable2(GL_LIGHTING);glDisable2(GL_FOG);glDisable2(GL_CULL_FACE);glDisable2(GL_DEPTH_TEST);glEnable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);}
static void save(const char* file){std::vector<unsigned char> p(400*240*4),flip(p.size());glFinish();glReadPixels(0,0,400,240,GL_RGBA,GL_UNSIGNED_BYTE,p.data());for(int y=0;y<240;++y)std::copy(p.begin()+y*400*4,p.begin()+(y+1)*400*4,flip.begin()+(239-y)*400*4);png_image im{};im.version=PNG_IMAGE_VERSION;im.width=400;im.height=240;im.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&im,file,0,flip.data(),0,nullptr));}
int main(){
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_TRUE);glfwWindowHint(GLFW_FOCUSED,GLFW_FALSE);glfwWindowHint(GLFW_FOCUS_ON_SHOW,GLFW_FALSE);
#ifdef TCPE_GLES3
 glfwWindowHint(GLFW_CLIENT_API,GLFW_OPENGL_ES_API);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,0);
#else
 glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
#endif
auto* window=glfwCreateWindow(400,240,"Native keyboard settings",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);
#ifdef TCPE_GLES3
 assert(TCPEGL3::initialize());
#else
 assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();
#endif
Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();
 AppPlatform_glfw platform;I18n::loadLanguage(&platform,"en_US");
#ifdef PLATFORM_DESKTOP
 {std::ofstream f("options.txt");f<<"ctrl_usetouchscreen:0\n";}
 auto* initializedDesktop=new PackGameProbe();AppContext actualContext{};actualContext.platform=&platform;actualContext.doRender=false;initializedDesktop->App::init(actualContext);
 assert(platform.supportsTouchscreen());assert(initializedDesktop->supportNonTouchScreen());assert(!initializedDesktop->options.useTouchScreen&&!initializedDesktop->useTouchscreen());
 assert(dynamic_cast<Touch::StartMenuScreen*>(initializedDesktop->screenChooser.createScreen(SCREEN_STARTMENU)));
 initializedDesktop->options.useTouchScreen=true;assert(initializedDesktop->useTouchscreen());
#endif
 Options options;Textures textures(&options,&platform);Font font(&options,"font/default8.png",&textures);font.setJavaTextEnabled(true);auto* game=new PackGameProbe();game->textures=&textures;game->font=&font;game->options.minecraft=game;game->options.initDefaultValues();game->options.useTouchScreen=true;

 for(auto size:std::vector<std::pair<int,int>>{{400,160},{200,120}}){
   SettingsProbe ui;ui.Screen::init(game,size.first,size.second);ui.selectCategory(0);
   auto* previous=button(ui,120);auto* next=button(ui,121);
   assert(previous->visible&&next->visible&&!previous->active&&next->active);
   ui.buttonClicked(next);assert(previous->active);
   while(next->active)ui.buttonClicked(next);
   assert(!next->active&&previous->active);
   ui.buttonClicked(previous);assert(next->active);
   ui.selectCategory(3);assert(!previous->visible&&!next->visible);
 }
 for(auto size:std::vector<std::pair<int,int>>{{400,240},{200,160},{160,96}}){
   SettingsProbe ui;ui.Screen::init(game,size.first,size.second);ui.selectCategory(4);
   for(auto* b:ui.buttons)if(b->visible){assert(b->x>=0&&b->y>=0&&b->x+b->width<=size.first&&b->y+b->height<=size.second);}
   projection(size.first,size.second);game->width=400;game->height=240;ui.render(-1,-1,0);save(size.first==400?"native-keybind-400.png":size.first==200?"native-keybind-200.png":"native-keybind-160.png");glfwSwapBuffers(window);
   game->options.useTouchScreen=false;
   for(int step=0;step<30;++step){ui.keyPressed(game->options.keyMenuNext.key);assert(ui.tabButtons[ui.tabButtonIndex]->visible&&ui.tabButtons[ui.tabButtonIndex]->active);}
   game->options.useTouchScreen=true;
   int old=game->options.keyUp.key;
   ui.buttonClicked(button(ui,100));ui.keyPressed(Keyboard::KEY_A);assert(game->options.keyUp.key==Keyboard::KEY_A); // intentional overlap accepted
   ui.buttonClicked(button(ui,100));ui.keyPressed(Keyboard::KEY_R);assert(game->options.keyUp.key==Keyboard::KEY_R);
   KeyboardInput movement(&game->options);movement.setKey(Keyboard::KEY_W,true);movement.tick(nullptr);assert(movement.ya==0);movement.setKey(Keyboard::KEY_R,true);movement.tick(nullptr);assert(movement.ya==1);
   ui.buttonClicked(button(ui,100));ui.keyPressed(Keyboard::KEY_ESCAPE);assert(game->options.keyUp.key==Keyboard::KEY_R);
   ui.buttonClicked(button(ui,100));ui.keyPressed(Keyboard::KEY_BACKSPACE);assert(game->options.keyUp.key==0);
   ui.buttonClicked(button(ui,122));assert(game->options.keyUp.key==Keyboard::KEY_W&&game->options.useTouchScreen);
   int pages=0;while(button(ui,121)->active){ui.buttonClicked(button(ui,121));assert(++pages<Options::KeyMappingCount);}assert(pages>0);ui.buttonClicked(button(ui,120));assert(button(ui,121)->active);
 }
 JavaGameSession session;assert(session.connection.connect("fixture.invalid","fixture",false));game->javaSession=&session;
 std::vector<uint16_t> air(4096,0);std::vector<uint8_t> sunlight(2048,255);uint8_t flag=1;TcpeJavaEvent chunk{};chunk.kind=TCPE_JAVA_CHUNK;chunk.x=chunk.z=0;chunk.height=16;chunk.states=air.data();chunk.states_len=air.size();chunk.sky_light=sunlight.data();chunk.sky_light_len=sunlight.size();chunk.light_sections=&flag;chunk.light_sections_len=1;assert(session.chunks.apply(chunk));auto* level=new JavaLevel(new MemoryLevelStorage(),session.chunks);game->level=level;auto* player=new ControlPlayer(game,level,nullptr,0);game->player=player;player->input=new KeyboardInput(&game->options);player->moveTo(2,4,2,0,0);
 Keyboard::feed(17,KeyboardAction::KEYUP);sent.clear();game->gui.handleKeyPressed(game->options.keyDrop.key);assert(sent=="{\"type\":\"drop_selected\",\"all\":false}");
 Keyboard::feed(17,KeyboardAction::KEYDOWN);sent.clear();game->gui.handleKeyPressed(game->options.keyDrop.key);assert(sent=="{\"type\":\"drop_selected\",\"all\":true}");Keyboard::feed(17,KeyboardAction::KEYUP);Keyboard::reset();
 Keyboard::feed(game->options.keySwapOffhand.key,KeyboardAction::KEYDOWN);sent.clear();game->tickInput();assert(sent=="{\"type\":\"swap_offhand\"}");Keyboard::feed(game->options.keySwapOffhand.key,KeyboardAction::KEYUP);Keyboard::reset();
 player->input->setKey(game->options.keyUp.key,true);Keyboard::feed(game->options.keySprint.key,KeyboardAction::KEYDOWN);player->aiStep();assert(player->isSprinting());player->travel(0,0);assert(sent.find("\"sprint\":true")!=std::string::npos);
 player->input->setKey(game->options.keyUp.key,false);Keyboard::feed(game->options.keySprint.key,KeyboardAction::KEYUP);player->aiStep();assert(!player->isSprinting());player->travel(0,0);assert(sent.find("\"sprint\":false")!=std::string::npos);
 game->width=400;game->height=240;game->options.useTouchScreen=true;
 auto* holder=new TouchInputHolder(game,&game->options);game->inputHolder=holder;player->input=holder->getMoveInput();assert(dynamic_cast<HybridMoveInput*>(player->input));
 AndroidKeyState android;AndroidControllerBindings bindings=androidControllerBindings(game->options);
 Keyboard::reset();Keyboard::feed(android.translate(51,0x101,false,bindings,true),KeyboardAction::KEYDOWN);game->tickInput();player->input->tick(player);assert(player->input->ya==1.f);
 Keyboard::feed(android.translate(51,0x101,false,bindings,false),KeyboardAction::KEYUP);game->tickInput();player->input->tick(player);assert(player->input->ya==0.f);
 game->options.keyUp.key=0;player->input->setKey(ControllerKeys::Forward,true);player->input->tick(player);assert(player->input->ya==1.f);player->input->setKey(ControllerKeys::Forward,false);game->options.keyUp.key=Keyboard::KEY_W;
 player->input->setKey(game->options.keyJump.key,true);player->input->setKey(game->options.keySneak.key,true);player->input->tick(player);assert(player->input->jumping&&player->input->sneaking&&player->input->wantUp&&player->input->wantDown);player->input->releaseAllKeys();player->input->tick(player);assert(!player->input->jumping&&!player->input->sneaking&&player->input->ya==0);
 int crafts=player->crafts;player->input->setKey(game->options.keyCraft.key,true);player->input->tick(player);assert(player->crafts==crafts+1);player->input->releaseAllKeys();
 game->options.keyCraft.key=0;player->input->setKey(ControllerKeys::Craft,true);player->input->tick(player);assert(player->crafts==crafts+2);player->input->releaseAllKeys();game->options.keyCraft.key=Keyboard::KEY_C;
 // Actual Java use sends release when the configured physical input is released.
 JavaGameMode charged(game);Keyboard::reset();Keyboard::feed(game->options.keyUse.key,KeyboardAction::KEYDOWN);assert(charged.useItem(player,game->level,nullptr));assert(sent.find("\"type\":\"use\"")!=std::string::npos);charged.tick();assert(sent.find("\"type\":\"use\"")!=std::string::npos);Keyboard::feed(game->options.keyUse.key,KeyboardAction::KEYUP);charged.tick();assert(sent=="{\"type\":\"release_use\"}");
 Keyboard::feed(ControllerKeys::Use,KeyboardAction::KEYDOWN);assert(charged.useItem(player,game->level,nullptr));Keyboard::feed(ControllerKeys::Use,KeyboardAction::KEYUP);charged.tick();assert(sent=="{\"type\":\"release_use\"}");Keyboard::reset();
 // Original long touch gesture becomes Java weapon use and releases on lift.
 auto* turn=dynamic_cast<UnifiedTurnBuild*>(holder->getBuildInput());assert(turn);const auto savedHeld=game->javaSession->inventory.held;game->javaSession->inventory.held.resourceId="minecraft:iron_spear";game->javaSession->inventory.held.count=1;
 BuildActionIntention touchAction;turn->tickBuild(player,&touchAction);int ux=-1,uy=-1;for(int y=40;y<int(turn->screenArea._y1)-40&&ux<0;y+=10)for(int x=int(turn->screenArea._x1)/2;x<int(turn->screenArea._x1)-40;x+=10)if(turn->isInsideArea(x,y)){ux=x;uy=y;break;}assert(ux>=0);
 Multitouch::feed(1,MouseAction::DATA_DOWN,ux,uy,1);Multitouch::commit();turn->getTurnDelta();turn->tickBuild(player,&touchAction);usleep(650000);turn->getTurnDelta();assert(turn->tickBuild(player,&touchAction)&&touchAction.isInteract()&&!touchAction.isRemove()&&turn->chargedUseHeld());assert(charged.useItem(player,game->level,nullptr));
 Multitouch::feed(1,MouseAction::DATA_UP,ux,uy,1);Multitouch::commit();turn->getTurnDelta();turn->tickBuild(player,&touchAction);assert(!turn->chargedUseHeld());charged.tick();assert(sent=="{\"type\":\"release_use\"}");Multitouch::reset();Multitouch::resetThisUpdate();Multitouch::commit();game->javaSession->inventory.held=savedHeld;
 // Stock touch movement and jump still feed through the same recognizer.
 session.inventory.held.resourceId="minecraft:iron_spear";session.inventory.held.count=1;session.itemUseActive=false;
 Multitouch::feed(1,MouseAction::DATA_DOWN,300,100,0);Multitouch::commit();holder->getTurnInput()->getTurnDelta();BuildActionIntention chargedAction;holder->getBuildInput()->tickBuild(player,&chargedAction);
 usleep(450000);holder->getTurnInput()->getTurnDelta();assert(holder->getBuildInput()->tickBuild(player,&chargedAction));
 auto* chargedTouch=dynamic_cast<UnifiedTurnBuild*>(holder->getBuildInput());assert(chargedTouch&&chargedTouch->chargedUseHeld());
 session.itemUseActive=true;assert(!holder->getBuildInput()->tickBuild(player,&chargedAction));assert(chargedTouch->chargedUseHeld());
 session.itemUseActive=false;assert(holder->getBuildInput()->tickBuild(player,&chargedAction));
 Multitouch::feed(1,MouseAction::DATA_UP,300,100,0);Multitouch::commit();holder->getTurnInput()->getTurnDelta();holder->getBuildInput()->tickBuild(player,&chargedAction);assert(!chargedTouch->chargedUseHeld());Multitouch::reset();Multitouch::resetThisUpdate();Multitouch::commit();
 // A retained network session must not turn offline mining into charged use.
 auto* offlineLevel=new OfflineFlagLevel(new MemoryLevelStorage(),session.chunks);game->level=offlineLevel;session.inventory.held.resourceId="minecraft:shield";session.itemUseActive=true;
 Multitouch::feed(1,MouseAction::DATA_DOWN,300,100,0);Multitouch::commit();holder->getTurnInput()->getTurnDelta();holder->getBuildInput()->tickBuild(player,&chargedAction);usleep(450000);holder->getTurnInput()->getTurnDelta();assert(holder->getBuildInput()->tickBuild(player,&chargedAction)&&chargedAction.isRemove()&&!chargedTouch->chargedUseHeld());
 Multitouch::feed(1,MouseAction::DATA_UP,300,100,0);Multitouch::commit();holder->getTurnInput()->getTurnDelta();holder->getBuildInput()->tickBuild(player,&chargedAction);Multitouch::reset();Multitouch::resetThisUpdate();Multitouch::commit();game->level=level;session.itemUseActive=false;
 TouchscreenInput_TestFps originalTouch(game,&game->options);auto area=originalTouch.getMovementRectangleArea();int tx=int(area.centerX()),ty=int(area._y0+2);
 Multitouch::feed(1,MouseAction::DATA_DOWN,tx,ty,0);Multitouch::commit();originalTouch.tick(player);float touchY=originalTouch.ya;assert(touchY>0);player->input->tick(player);assert(player->input->ya==touchY);
 Multitouch::feed(1,MouseAction::DATA_UP,tx,ty,0);Multitouch::commit();player->input->tick(player);Multitouch::reset();Multitouch::resetThisUpdate();Multitouch::commit();
 FieldScreen fieldScreen;fieldScreen.Screen::init(game,400,240);TextBox field(20,"Name");field.focused=true;fieldScreen.textBoxes.push_back(&field);game->screen=&fieldScreen;
 Keyboard::reset();for(uint32_t scalar:{uint32_t('A'),0xe9u,0x4e2du,0x1f600u})assert(Keyboard::feedCodepoint(scalar));while(Keyboard::nextTextChar())fieldScreen.keyboardNewChar(Keyboard::getChar());assert(field.text==std::string("A\xc3\xa9\xe4\xb8\xad\xf0\x9f\x98\x80"));fieldScreen.keyPressed(Keyboard::KEY_BACKSPACE);assert(game->screen==&fieldScreen&&field.text==std::string("A\xc3\xa9\xe4\xb8\xad"));game->screen=nullptr;fieldScreen.textBoxes.clear();
 game->options.setKey(0,Keyboard::KEY_R);game->options.setKey(28,Keyboard::KEY_N);game->options.setKey(29,Keyboard::KEY_V);game->options.setKey(30,Keyboard::KEY_H);
 std::ifstream saved("options.txt");std::string line,keys;while(std::getline(saved,line))if(line.find("key_")==0)keys+=line+"\n";saved.close();assert(keys.find("key_key.forward:82")!=std::string::npos);assert(keys.find("key_key.hotbar.9:78")!=std::string::npos);
 {std::ofstream f("options.txt");f<<keys<<"key_key.left:999\nkey_key.back:bad\n";}
 Options reloaded;reloaded.minecraft=game;reloaded.initDefaultValues();reloaded.update();assert(reloaded.keyUp.key==Keyboard::KEY_R&&reloaded.keyHotbar[8].key==Keyboard::KEY_N&&reloaded.keyLeft.key==Keyboard::KEY_A&&reloaded.keyDown.key==Keyboard::KEY_S&&reloaded.keySprint.key==Keyboard::KEY_V&&reloaded.keySwapOffhand.key==Keyboard::KEY_H);
 reloaded.resetKeys();assert(reloaded.keyUp.key==Keyboard::KEY_W&&reloaded.keyHotbar[8].key=='9');
 std::cout<<"Native Keyboard settings PASS: 400x240/200x160/160x96 visible layouts; overlap accepted, rebind, cancel, clear, defaults, paging, real movement remap, nativeQ/CtrlQ/swap commands, held sprint/stopJavawalk, actual touch-holder hybrid routing/controller unbinding/crafting/original gestures and native Unicode fields and save/reload/invalid values."<<std::endl;std::_Exit(0);
}
