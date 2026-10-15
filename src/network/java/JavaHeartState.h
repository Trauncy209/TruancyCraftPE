#ifndef TCPE_JAVA_HEART_STATE_H
#define TCPE_JAVA_HEART_STATE_H
#include <algorithm>
#include <cstdint>
// Official Java health presentation, separate from server health/collision state.
struct JavaHeartState {
 bool initialized=false;unsigned world=0;int last=0,display=0,blinkUntil=0;double changed=0;
 void reset(){*this=JavaHeartState{};}
 void update(unsigned revision,int health,unsigned invulnerable,int tick,double millis){
  if(!initialized||revision!=world){initialized=true;world=revision;last=display=health;changed=millis;blinkUntil=0;return;}
  if(health<last&&invulnerable>0){changed=millis;blinkUntil=tick+20;}
  else if(health>last&&invulnerable>0){changed=millis;blinkUntil=tick+10;}
  if(millis-changed>1000){display=health;changed=millis;}
  last=health;
 }
 bool blinking(int tick)const{return blinkUntil>tick&&((blinkUntil-tick)/3)%2==1;}
 enum Kind {Normal,Poisoned,Withered,Absorbing,Frozen,Container};
 static Kind kind(bool poison,bool wither,unsigned frozen){return poison?Poisoned:wither?Withered:frozen>=140?Frozen:Normal;}
 static int atlasX(bool hardcore,bool half,bool blink){return ((hardcore?4:0)+(half?2:0)+(blink?1:0))*9;}
 static int atlasY(Kind type){return int(type)*9;}
};
#endif
