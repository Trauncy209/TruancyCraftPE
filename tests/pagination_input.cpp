#include "../original-tcpe/src/client/gui/screens/JavaInventoryScreen.h"
#include "../original-tcpe/src/client/gui/screens/JavaDialogScreen.h"
#include "../original-tcpe/src/client/gui/screens/JavaChatActionsScreen.h"
#include <cassert>
#include <iostream>
#include <cstdlib>
static unsigned commands=0;
extern "C" TcpeJavaSession* __wrap_tcpe_java_connect(const char*,size_t,const char*,size_t,bool){return reinterpret_cast<TcpeJavaSession*>(1);}
extern "C" void __wrap_tcpe_java_close(TcpeJavaSession*){}
extern "C" bool __wrap_tcpe_java_command(TcpeJavaSession*,const char*,size_t){++commands;return true;}
class GameProbe:public Minecraft{public:GameProbe():Minecraft(){}};
class InventoryProbe:public JavaInventoryScreen{public:InventoryProbe():JavaInventoryScreen(7){}using Screen::buttons;};
class DialogProbe:public JavaDialogScreen{public:explicit DialogProbe(const JavaGameSession::Dialog& d):JavaDialogScreen(d){}using Screen::buttons;};
class ChatProbe:public JavaChatActionsScreen{public:explicit ChatProbe(const JavaGameSession& s):JavaChatActionsScreen(s){}using Screen::buttons;};
int main(){
 GameProbe* owner=new GameProbe();GameProbe& game=*owner;game.javaSession=new JavaGameSession();game.javaSession->connection.connect("fixture","NativeTest",false);game.javaSession->inventory.windowId=7;game.javaSession->inventory.slots.resize(90);
 InventoryProbe inv;inv.Screen::init(&game,400,240);Button* next=inv.buttons[1];const int x=next->x+next->width/2,y=next->y+next->height/2;unsigned hits=0;
 for(Button* b:inv.buttons)if(b->clicked(&game,x,y)){assert(b==next);++hits;inv.buttonClicked(b);}assert(hits==1);assert(commands==0);
 Button* firstSlot=inv.buttons[4];assert(inv.describedSlotAt(firstSlot->x+1,firstSlot->y+1)==45);assert(commands==0);assert(inv.describedSlotAt(x,y)==-1);assert(commands==0);
 unsigned hidden=0;for(Button* b:inv.buttons)if(!b->visible){assert(!b->active);inv.buttonClicked(b);++hidden;}assert(hidden>0&&commands==0);
 // Visible item actions still route, and page movement itself does not dispatch.
 inv.buttonClicked(inv.buttons[4]);assert(commands==1);
 JavaGameSession::Dialog dialog;dialog.pending=true;dialog.revision=1;for(int i=0;i<8;i++)dialog.actions.push_back({"","fixture:action"});game.javaSession->dialog=dialog;DialogProbe ds(dialog);ds.Screen::init(&game,400,240);for(Button* b:ds.buttons)if(!b->visible){assert(!b->active);ds.buttonClicked(b);}assert(commands==1);
 game.javaSession->chatActionsPending=true;game.javaSession->chatActionsRevision=1;for(int i=0;i<12;i++)game.javaSession->chatActions.push_back({"","/fixture"});ChatProbe cs(*game.javaSession);cs.Screen::init(&game,400,240);for(Button* b:cs.buttons)if(!b->visible){assert(!b->active);cs.buttonClicked(b);}assert(commands==1);
 game.javaSession->inventory.windowId=7;game.javaSession->inventory.carried.count=3;inv.setupPositions();Button* drop=inv.buttons[67];assert(drop->visible&&drop->active);unsigned before=commands;inv.buttonClicked(drop);assert(commands==before+1);assert(game.javaSession->inventory.carried.count==3);
 game.javaSession->inventory.carried.count=0;inv.setupPositions();assert(!drop->visible&&!drop->active);inv.buttonClicked(drop);assert(commands==before+1);
 game.javaSession->inventory.carried.count=3;inv.setupPositions();game.javaSession->inventory.windowId=8;inv.buttonClicked(drop);assert(commands==before+1);
 std::cout<<"Actual native Button hit testing: Next only, no ghost slot commands, hidden dialog/chat actions disabled PASS"<<std::endl;
 std::_Exit(0);
}
