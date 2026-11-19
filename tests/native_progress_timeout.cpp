#define main generatorFixtureMain
#include "native_start_game_generator.cpp"
#undef main
#include "client/gui/screens/ProgressScreen.h"
#include "client/gui/screens/DisconnectionScreen.h"
#include "platform/input/Keyboard.h"
struct ProbeProgress : ProgressScreen { void bind(Minecraft* game) { minecraft=game; } };
static int scheduled=0;
static Screen* scheduledScreen=nullptr;
extern "C" void __wrap__ZN9Minecraft9setScreenEP6Screen(Minecraft*,Screen* screen) { ++scheduled; scheduledScreen=screen; }
static double now=0;
static double clockNow() { return now; }
struct JoinWire : Wire { int disconnects=0; void disconnect() override { ++disconnects; } };
int main() {
    Mth::initMth(); Material::initMaterials(); MobCategory::initMobCategories();
    Tile::initTiles(); Item::initItems(); Biome::initBiomes();
    Storage storage; auto* game=new Game(&storage); JoinWire wire;
    game->raknetInstance=&wire;
    ClientSideNetworkHandler handler(game,&wire,clockNow);
    game->netCallback=&handler;
    const RakNet::RakNetGUID source(123456);
    ProbeProgress progress; progress.bind(game);
    // Tick rate cannot exhaust a wall-clock deadline.
    handler.beginJoin();
    for(int i=0;i<1000;++i) progress.tick();
    assert(scheduled==0);
    now=1; handler.onConnect(source);
    now=31; progress.tick(); assert(scheduled==0); // Original 30s live delay survives.
    StartGamePacket start(17,LGV_INFINITE,GameType::Survival,25,-26.5f,65.f,21.5f);
    handler.handle(source,&start); assert(game->level&&storage.deletes==1);
    handler.handle(source,&start); assert(storage.deletes==1); // No duplicate replacement.
    now=151; progress.tick();
    assert(scheduled==1&&dynamic_cast<DisconnectionScreen*>(scheduledScreen)&&wire.disconnects==1);
    handler.handle(source,&start); assert(storage.deletes==1); // Late StartGame is ignored.
    handler.onDisconnect(source); handler.onUnableToConnect();
    assert(scheduled==1); // Closing the transport does not overwrite the reason.
    game->level=nullptr;
    now=200; handler.beginJoin();
    now=230; progress.tick(); assert(scheduled==2&&wire.disconnects==2);
    handler.onConnect(source); handler.handle(source,&start);
    assert(storage.deletes==1); // A late accepted transport cannot revive cancellation.
    now=300; handler.beginJoin(); handler.onConnect(source);
    progress.keyPressed(Keyboard::KEY_ESCAPE);
    assert(scheduled==3&&wire.disconnects==3);
    handler.handle(source,&start); assert(storage.deletes==1);
    now=400; handler.beginJoin(); handler.onConnect(source);
    progress.handleBackEvent(false);
    assert(scheduled==4&&wire.disconnects==4);
    handler.handle(source,&start); assert(storage.deletes==1);
    now=500; handler.beginJoin(); handler.onConnect(source);
    handler.handle(RakNet::RakNetGUID(999),&start); assert(storage.deletes==1);
    handler.handle(source,&start); assert(storage.deletes==2&&game->level);
    std::cout << "Actual native join: delayed StartGame, bounded bootstrap, duplicate/foreign/late packet rejection, timeout/Escape/Back cancellation and retry PASS\n";
}
