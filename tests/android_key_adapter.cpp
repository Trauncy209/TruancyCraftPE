#include "platform/input/AndroidKeyAdapter.h"
#include <cassert>
#include <iostream>
int main(){
 constexpr int keyboard=0x101,gamepad=0x401,dpad=0x201,joystick=0x1000010;
 for(int i=0;i<26;++i)assert(androidKeyboardKey(29+i)=='A'+i);
 for(int i=0;i<10;++i){assert(androidKeyboardKey(7+i)=='0'+i);assert(androidKeyboardKey(144+i)=='0'+i);}
 for(int i=0;i<12;++i)assert(androidKeyboardKey(131+i)==112+i);
 assert(androidKeyboardKey(51)=='W'&&androidKeyboardKey(51)!='3');
 assert(androidKeyboardKey(19)==38&&androidKeyboardKey(20)==40&&androidKeyboardKey(21)==37&&androidKeyboardKey(22)==39);
 assert(androidKeyboardKey(59)==10&&androidKeyboardKey(60)==10&&androidKeyboardKey(113)==17&&androidKeyboardKey(114)==17&&androidKeyboardKey(57)==18&&androidKeyboardKey(58)==18);
 assert(androidKeyboardKey(61)==9&&androidKeyboardKey(62)==32&&androidKeyboardKey(66)==13&&androidKeyboardKey(67)==8&&androidKeyboardKey(111)==27&&androidKeyboardKey(112)==46);
 assert(androidKeyboardKey(4)==0&&androidKeyboardKey(24)==0&&androidKeyboardKey(25)==0&&androidKeyboardKey(999)==0);
 assert(androidKeyboardKey(154)==111&&androidKeyboardKey(155)==106&&androidKeyboardKey(156)==109&&androidKeyboardKey(157)==107&&androidKeyboardKey(158)==110&&androidKeyboardKey(160)==13&&androidKeyboardKey(161)==187);
 assert(androidKeyboardKey(56)==190&&androidKeyboardKey(112)!=androidKeyboardKey(56));
 assert(!androidControllerSource(keyboard)&&androidControllerSource(gamepad)&&androidControllerSource(dpad)&&androidControllerSource(joystick)&&androidControllerSource(keyboard|gamepad));

 AndroidControllerBindings b;
 assert(androidControllerKey(19,false,b)==ControllerKeys::Forward&&androidControllerKey(20,false,b)==ControllerKeys::Back&&androidControllerKey(21,false,b)==ControllerKeys::Left&&androidControllerKey(22,false,b)==ControllerKeys::Right);
 assert(androidControllerKey(19,true,b)==ControllerKeys::MenuPrevious&&androidControllerKey(20,true,b)==ControllerKeys::MenuNext&&androidControllerKey(23,true,b)==ControllerKeys::MenuSelect&&androidControllerKey(97,true,b)==ControllerKeys::MenuCancel);
 assert(androidControllerKey(23,false,b)==ControllerKeys::Jump&&androidControllerKey(102,false,b)==ControllerKeys::Attack&&androidControllerKey(103,false,b)==ControllerKeys::Use&&androidControllerKey(109,false,b)==ControllerKeys::Craft);
 assert(androidControllerKey(99,false,b)==99&&androidControllerKey(100,false,b)==100&&androidControllerKey(4,false,b)==0&&androidControllerKey(82,false,b)==ControllerKeys::Pause);
 b.forward=0;b.menuPrevious=0;b.use=0;assert(androidControllerKey(19,false,b)==ControllerKeys::Forward&&androidControllerKey(19,true,b)==ControllerKeys::MenuPrevious&&androidControllerKey(103,false,b)==ControllerKeys::Use);
 assert(androidControllerEvent(100,keyboard)&&!androidControllerEvent(51,keyboard));
 AndroidKeyState state;assert(state.translate(19,gamepad,false,b,true)==ControllerKeys::Forward);assert(state.translate(19,gamepad,true,b,false)==ControllerKeys::Forward);
 assert(state.translate(51,keyboard,false,b,true)=='W');assert(state.translate(51,gamepad,false,b,true)==0);assert(state.translate(51,keyboard,true,b,false)=='W');
 assert(state.translate(100,keyboard,false,b,true)==100);assert(state.translate(100,keyboard,true,b,false)==100);
 assert(state.translate(-1,keyboard,false,b,true)==0&&state.translate(256,keyboard,false,b,true)==0);
 std::cout<<"Android keyboard/controller mapping PASS: alphabet/numbers/functions/arrows/modifiers/menu, no W-to-3 truncation, separate controller actions survive keyboard rebinding/unbinding, matching releases and system Back/volume preservation."<<std::endl;
}
