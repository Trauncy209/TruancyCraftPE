#define main generatorFixtureMain
#include "native_start_game_generator.cpp"
#undef main
#include "client/gui/screens/ProgressScreen.h"
#include "client/gui/screens/DisconnectionScreen.h"
struct ProbeProgress:ProgressScreen{void bind(Minecraft*game){minecraft=game;}};
static int scheduled=0;static Screen* scheduledScreen=nullptr;
extern "C" void __wrap__ZN9Minecraft9setScreenEP6Screen(Minecraft*,Screen*screen){++scheduled;scheduledScreen=screen;}
struct ProgressGame:Game{using Game::Game;void loading(){isGeneratingLevel=true;}};
int main(){Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Storage storage;auto*game=new ProgressGame(&storage);game->loading();assert(game->getProgressStatusId()==0);game->level=new Level(new MemoryLevelStorage(),"loading",LevelSettings(17,GameType::Survival,LGV_INFINITE),LGV_INFINITE);game->level->isClientSide=true;ProbeProgress arrived;arrived.bind(game);for(int i=0;i<400;++i)arrived.tick();assert(scheduled==0&&!scheduledScreen);game->level=nullptr;ProbeProgress absent;absent.bind(game);for(int i=0;i<200;++i)absent.tick();assert(scheduled==1&&dynamic_cast<DisconnectionScreen*>(scheduledScreen));std::cout<<"Actual ProgressScreen: received native level survives locate timeout while bootstrapping; absent level still times out PASS"<<std::endl;}
