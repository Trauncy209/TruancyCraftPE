#include "client/Minecraft.h"
#include "network/java/JavaGameMode.h"
#include "network/java/JavaLevel.h"
#include "client/player/LocalPlayer.h"
#include "client/player/input/KeyboardInput.h"
#include "client/particle/ParticleEngine.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "util/Mth.h"
#include <cassert>
#include <iostream>
#include <cstdlib>
static std::vector<std::string> sent;
// CPU fixture suppresses only particle texture upload; mining and particles remain real.
extern "C" int __wrap__ZN8Textures11loadTextureERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEb(void*,const std::string&,bool){return 0;}
extern "C" TcpeJavaSession* __wrap_tcpe_java_connect(const char*,size_t,const char*,size_t,bool){return reinterpret_cast<TcpeJavaSession*>(1);}
extern "C" void __wrap_tcpe_java_close(TcpeJavaSession*){}
extern "C" bool __wrap_tcpe_java_command(TcpeJavaSession*,const char* p,size_t n){sent.emplace_back(p,n);return sent.back().find("\"use\"")==std::string::npos;}
class Probe:public Minecraft{public:Probe():Minecraft(){}};
class FixtureLevel:public JavaLevel{public:FixtureLevel(JavaGameSession& s):JavaLevel(new MemoryLevelStorage(),s.chunks){}int getTile(int,int,int)override{return 1;}int getData(int,int,int)override{return 0;}};
class Intent:public IBuildInput{public:int flags=0;bool tickBuild(Player*,BuildActionIntention* out)override{*out=BuildActionIntention(flags);return flags!=0;}};
static void expect(std::initializer_list<const char*> commands){assert(sent.size()==commands.size());size_t i=0;for(auto c:commands)assert(sent[i++]==c);sent.clear();}
int main(){Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();auto* g=new Probe;g->javaSession=new JavaGameSession;assert(g->javaSession->connection.connect("fixture","fixture",false));g->level=new FixtureLevel(*g->javaSession);g->player=new LocalPlayer(g,g->level,nullptr,0,false);g->player->input=new KeyboardInput(&g->options);g->gameMode=new JavaGameMode(g);g->particleEngine=new ParticleEngine(g->level,nullptr);g->hitResult=HitResult(0,64,0,1,Vec3(0,64,0));g->options.keyDestroy.key=Keyboard::KEY_K;auto* touch=new Intent;g->inputHolder=new CustomInputHolder(nullptr,nullptr,touch);assert(g->useTouchscreen());
 for(int key:{Keyboard::KEY_K,int(ControllerKeys::Attack)}){Keyboard::feed(key,KeyboardAction::KEYDOWN);g->tickInput();g->tickInput();expect({"{\"type\":\"mine\",\"held\":true}"});Keyboard::feed(key,KeyboardAction::KEYUP);g->tickInput();g->tickInput();expect({"{\"type\":\"mine\",\"held\":false}"});}
 touch->flags=BuildActionIntention::BAI_FIRSTREMOVE|BuildActionIntention::BAI_INTERACT;g->tickInput();touch->flags=BuildActionIntention::BAI_REMOVE|BuildActionIntention::BAI_INTERACT;g->tickInput();for(auto& c:sent)std::cout<<c<<std::endl;expect({"{\"type\":\"mine\",\"held\":true}"});touch->flags=0;g->tickInput();g->tickInput();expect({"{\"type\":\"mine\",\"held\":false}"});
 // One control releasing must not cancel another control that remains held.
 touch->flags=BuildActionIntention::BAI_FIRSTREMOVE|BuildActionIntention::BAI_INTERACT;g->tickInput();expect({"{\"type\":\"mine\",\"held\":true}"});Keyboard::feed(Keyboard::KEY_K,KeyboardAction::KEYDOWN);touch->flags=0;g->tickInput();expect({"{\"type\":\"mine\",\"held\":true}"});g->tickInput();expect({});Keyboard::feed(Keyboard::KEY_K,KeyboardAction::KEYUP);g->tickInput();expect({"{\"type\":\"mine\",\"held\":false}"});
 touch->flags=BuildActionIntention::BAI_INTERACT;g->tickInput();expect({"{\"type\":\"use\"}","{\"type\":\"use\"}"});touch->flags=0;g->tickInput();expect({});touch->flags=BuildActionIntention::BAI_BUILD|BuildActionIntention::BAI_INTERACT;g->tickInput();expect({"{\"type\":\"use\"}","{\"type\":\"use\"}"});g->hitResult=HitResult(g->player);touch->flags=BuildActionIntention::BAI_ATTACK;g->tickInput();assert(sent.size()==1&&sent[0].find("\"type\":\"attack\"")!=std::string::npos);sent.clear();touch->flags=BuildActionIntention::BAI_INTERACT;g->tickInput();assert(sent.size()==2&&sent[0].find("\"type\":\"interact\"")!=std::string::npos&&sent[1]=="{\"type\":\"use\"}");sent.clear();touch->flags=BuildActionIntention::BAI_REMOVE|BuildActionIntention::BAI_INTERACT;g->tickInput();expect({});std::cout<<"CPU actual Minecraft.tickInput -> JavaGameMode -> captured C ABI: keyboard/controller hold-release, touch remove/interact and force-use boundary PASS"<<std::endl;std::_Exit(0);}
