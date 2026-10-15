#ifndef TCPE_ANDROID_KEY_ADAPTER_H
#define TCPE_ANDROID_KEY_ADAPTER_H
#include <array>
#include "ControllerActionKeys.h"
// Portable Android key/source adapter; no Android SDK dependency in mapping tests.
struct AndroidControllerBindings {
 int forward=87,back=83,left=65,right=68,jump=32,craft=67,attack=88,use=85;
 int menuPrevious=38,menuNext=40,menuSelect=13,menuCancel=8,pause=27;
};
template<class OptionsT> inline AndroidControllerBindings androidControllerBindings(const OptionsT& o) {
 AndroidControllerBindings b;b.forward=o.keyUp.key;b.back=o.keyDown.key;b.left=o.keyLeft.key;b.right=o.keyRight.key;
 b.jump=o.keyJump.key;b.craft=o.keyCraft.key;b.attack=o.keyDestroy.key;b.use=o.keyUse.key;
 b.menuPrevious=o.keyMenuPrevious.key;b.menuNext=o.keyMenuNext.key;b.menuSelect=o.keyMenuOk.key;b.menuCancel=o.keyMenuCancel.key;b.pause=o.keyPause.key;return b;
}
inline bool androidControllerSource(int source) {
 return (source&0x401)==0x401 || (source&0x1000010)==0x1000010 || ((source&0x201)==0x201 && (source&0x101)!=0x101);
}
inline bool androidControllerEvent(int raw,int source){return androidControllerSource(source)||(raw>=96&&raw<=110);}
inline int androidKeyboardKey(int raw) {
 if(raw>=29&&raw<=54)return raw-29+65;
 if(raw>=7&&raw<=16)return raw-7+48;
 if(raw>=144&&raw<=153)return raw-144+48;
 if(raw>=131&&raw<=142)return raw-131+112;
 switch(raw){
 case 19:return 38;case 20:return 40;case 21:return 37;case 22:return 39;case 23:return 13;
 case 154:return 111;case 155:return 106;case 156:return 109;case 157:return 107;case 158:return 110;case 159:return 188;case 161:return 187;
 case 61:return 9;case 62:return 32;case 66:case 160:return 13;case 67:return 8;
 case 59:case 60:return 10;case 113:case 114:return 17;case 57:case 58:return 18;
 case 111:case 82:return 27;case 112:return 46;case 122:return 36;case 123:return 35;case 92:return 33;case 93:return 34;
 case 55:return 188;case 56:return 190;case 68:return 192;case 69:return 189;case 70:return 187;
 case 71:return 219;case 72:return 221;case 73:return 220;case 74:return 186;case 75:return 222;case 76:return 191;
 // Android Back and volume remain handled by the original app/system path.
 default:return 0;
 }
}
inline int androidControllerKey(int raw,bool menu,const AndroidControllerBindings&) {
 switch(raw){
 case 19:return menu?ControllerKeys::MenuPrevious:ControllerKeys::Forward;
 case 20:return menu?ControllerKeys::MenuNext:ControllerKeys::Back;
 case 21:return menu?0:ControllerKeys::Left;case 22:return menu?0:ControllerKeys::Right;
 case 23:case 96:return menu?ControllerKeys::MenuSelect:ControllerKeys::Jump;
 case 97:return menu?ControllerKeys::MenuCancel:0;
 case 102:return ControllerKeys::Attack;case 103:return ControllerKeys::Use;case 109:return ControllerKeys::Craft;
 case 82:case 108:return menu?ControllerKeys::MenuCancel:ControllerKeys::Pause;
 case 99:case 100:return menu?0:raw;
 default:return 0;
 }
}

class AndroidKeyState {
 std::array<int,512> pressed{};
public:
 int translate(int raw,int source,bool menu,const AndroidControllerBindings& b,bool down) {
  if(raw<0||raw>=256)return 0;
  bool controller=androidControllerEvent(raw,source);unsigned index=raw+(controller?256:0);
  if(!down&&pressed[index]){int key=pressed[index];pressed[index]=0;return key;}
  int key=controller?androidControllerKey(raw,menu,b):androidKeyboardKey(raw);
  if(down)pressed[index]=key;
  return key;
 }
};
#endif
