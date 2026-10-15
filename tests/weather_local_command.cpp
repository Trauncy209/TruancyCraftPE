#include "client/NativeWeatherCommand.h"
#include "client/gui/screens/ConsoleScreen.h"
#include "client/gui/screens/ChatScreen.h"
#include "client/gui/components/Button.h"
#include "client/player/LocalPlayer.h"
#include "AppPlatform.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/level/tile/Tile.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include <cassert>
#include <iostream>
struct Platform:AppPlatform{std::string text;int getUserInputStatus()override{return 1;}StringVector getUserInput()override{return {text};}};
struct GameProbe:Minecraft{Platform fake;GameProbe():Minecraft(){}AppPlatform* platform()override{return &fake;}};
struct Storage:MemoryLevelStorage{int saves=0;WeatherState saved;void saveLevelData(LevelData& data,std::vector<Player*>*)override{++saves;saved=data.weather;}};
struct World:Level{using Level::Level;bool java=false;bool isJavaServerLevel()const override{return java;}};
struct Wire:IRakNetInstance{bool hosted=false;int chats=0,snapshots=0;std::string raw;bool isServer()override{return hosted;}void send(Packet& p)override{if(auto* chat=dynamic_cast<ChatPacket*>(&p)){++chats;raw=chat->message;}if(dynamic_cast<WeatherPacket*>(&p))++snapshots;}};
int main(){
 using namespace NativeWeatherCommand;
 assert(!parse("hello").recognized&&!parse("/weatherish rain").recognized);
 for(const char* text:{"/weather","/weather snow","/weather rain 0","/weather rain -1","/weather rain 50001","/weather rain 2.5","/weather rain 10s","/weather rain 99999999999999999999999","/weather rain 1 extra"}){auto p=parse(text);assert(p.recognized&&!p.valid);}
 assert(parse("/weather rain").valid&&parse("/weather rain").ticks==6000);
 assert(parse("  /weather thunder 50000 ").valid&&parse("/weather thunder 50000").ticks==1000000);
 assert(parse("/weather clear +12").valid&&parse("/weather clear +12").ticks==240);
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
 auto* game=new GameProbe();auto* storage=new Storage();auto* level=new World(storage,"local-weather",LevelSettings(123,GameType::Survival,LGV_ORIGINAL),LGV_ORIGINAL);game->level=level;Wire wire;game->raknetInstance=&wire;
 auto& weather=level->getLevelData()->weather;weather.rain=.4f;
 assert(dispatch(*game,"/weather rain"));assert(weather.raining&&weather.rainTicks==6000&&weather.rain==.4f&&storage->saves==1&&storage->saved.raining&&wire.snapshots==0&&wire.chats==0);
 assert(dispatch(*game,"/weather rain 0"));assert(weather.rainTicks==6000&&storage->saves==1&&wire.snapshots==0);
 wire.hosted=true;assert(dispatch(*game,"/weather thunder 15"));assert(weather.raining&&weather.thundering&&weather.rainTicks==300&&storage->saves==2&&wire.snapshots==1);
 level->isClientSide=true;assert(dispatch(*game,"/weather clear 20"));assert(weather.raining&&weather.thundering&&weather.rainTicks==300&&storage->saves==2&&wire.snapshots==1&&wire.chats==1&&wire.raw=="/weather clear 20");
 assert(dispatch(*game,"/weather nonsense 0"));assert(wire.chats==2&&wire.raw=="/weather nonsense 0"&&storage->saves==2);
 level->java=true;assert(!dispatch(*game,"/weather clear")&&wire.chats==2&&weather.raining);
 level->java=false;level->isClientSide=false;game->javaSession=reinterpret_cast<JavaGameSession*>(uintptr_t(1));assert(!dispatch(*game,"/weather clear")&&weather.raining);game->javaSession=nullptr;
 assert(!dispatch(*game,"plain chat")&&!dispatch(*game,"/time set day"));
 assert(dispatch(*game,"/weather clear 1"));assert(!weather.raining&&!weather.thundering&&weather.rainTicks==20&&storage->saves==3&&wire.snapshots==2);
 // Exercise both real chat submission paths without a renderer/window.
 game->player=new LocalPlayer(game,level,nullptr,0,false);
 ConsoleScreen console;static_cast<Screen&>(console).init(game,854,480);Button send(4,"Send");
 for(char c:std::string("/weather rain 7"))console.keyboardNewChar(c);console.buttonClicked(&send);
 assert(weather.raining&&!weather.thundering&&weather.rainTicks==140&&storage->saves==4&&wire.snapshots==3);
 game->fake.text="/weather thunder 9";ChatScreen dialog;static_cast<Screen&>(dialog).init(game,854,480);dialog.render(0,0,0);
 assert(weather.thundering&&weather.rainTicks==180&&storage->saves==5&&wire.snapshots==4);
 level->isClientSide=true;
 for(char c:std::string("/weather clear 8"))console.keyboardNewChar(c);console.buttonClicked(&send);
 assert(weather.thundering&&weather.rainTicks==180&&wire.chats==3&&wire.raw=="/weather clear 8"&&storage->saves==5);
 game->fake.text="/weather clear 8";dialog.render(0,0,0);
 assert(weather.thundering&&weather.rainTicks==180&&wire.chats==4&&wire.raw=="/weather clear 8"&&storage->saves==5);
 std::cout<<"Actual ConsoleScreen and dialog ChatScreen submissions: owner executes+saves/broadcasts; remote forwards raw without mutation PASS\n";
 std::cout<<"Native local weather: strict parser/defaults/bounds; owner save+smooth transition; host broadcast; native remote raw forwarding/no mutation; Java and other commands untouched PASS\n";
}
