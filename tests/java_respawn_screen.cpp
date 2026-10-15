#include "client/Minecraft.h"
#include "client/gui/screens/DeathScreen.h"
#include "client/gui/components/Button.h"
#include "network/java/JavaGameSession.h"
#include "client/gui/screens/JavaInventoryScreen.h"
#include "client/gui/screens/ConsoleScreen.h"
#include "client/gui/screens/JavaDialogScreen.h"
#include "client/gui/screens/StartMenuScreen.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>
static unsigned requests=0;
static bool transport_accept=false;
static std::string command;
extern "C" TcpeJavaSession* __wrap_tcpe_java_connect(const char*,size_t,const char*,size_t,bool){return reinterpret_cast<TcpeJavaSession*>(1);}
extern "C" void __wrap_tcpe_java_close(TcpeJavaSession*){}
extern "C" bool __wrap_tcpe_java_command(TcpeJavaSession*,const char* data,size_t size){++requests;command.assign(data,size);return transport_accept;}
class DeathProbe:public DeathScreen {public:using Screen::buttons;};
class GameProbe:public Minecraft {public:GameProbe():Minecraft(){}};
int main(){
 auto* app=new GameProbe();app->javaSession=new JavaGameSession();
 assert(app->javaSession->connection.connect("fixture","NativeTest",false));
 JavaInventoryScreen inventory(0);ConsoleScreen console;JavaDialogScreen dialog(JavaGameSession::Dialog{});StartMenuScreen other;
 assert(javaDeathMayReplaceMenu(&inventory)&&javaDeathMayReplaceMenu(&console));assert(!javaDeathMayReplaceMenu(&dialog)&&!javaDeathMayReplaceMenu(&other)&&!javaDeathMayReplaceMenu(nullptr));
 DeathProbe death;assert(!javaDeathMayReplaceMenu(&death));death.Screen::init(app,400,240);app->screen=&death;
 Button* respawn=death.buttons[0];
 death.buttonClicked(respawn);assert(requests==0);
 for(int i=0;i<30;++i)death.tick();
 death.buttonClicked(respawn);assert(requests==1);assert(respawn->active);assert(app->screen==&death);
 transport_accept=true;death.buttonClicked(respawn);assert(requests==2);assert(command=="{\"type\":\"respawn\"}");
 assert(!respawn->active);assert(respawn->msg=="Respawning...");assert(app->screen==&death);
 // A duplicate callback must not send again, including after more UI ticks.
 for(int i=0;i<40;++i)death.tick();death.buttonClicked(respawn);assert(requests==2);
 assert(death.buttons[1]->active);
 std::cout<<"Native Java respawn screen: delay, send failure retry, exact command, server wait and duplicate suppression PASS"<<std::endl;
 std::_Exit(0);
}
