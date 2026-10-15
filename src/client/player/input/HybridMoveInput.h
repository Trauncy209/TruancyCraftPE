#ifndef TCPE_HYBRID_MOVE_INPUT_H
#define TCPE_HYBRID_MOVE_INPUT_H
#include "KeyboardInput.h"
#include "../../../world/entity/player/Player.h"
#include <algorithm>
// Retain the original touch recognizer/layout and add independent keyboard/controller input.
class HybridMoveInput:public IMoveInput {
 IMoveInput* touch;
 KeyboardInput keyboard;
 static float bounded(float v){return std::max(-1.f,std::min(1.f,v));}
public:
 HybridMoveInput(IMoveInput* original,Options* options):touch(original),keyboard(options){}
 void setKey(int key,bool down)override{keyboard.setKey(key,down);}
 void releaseAllKeys()override{touch->releaseAllKeys();keyboard.releaseAllKeys();xa=ya=0;jumping=sneaking=wantUp=wantDown=isChangingFlightHeight=false;}
 void render(float a)override{touch->render(a);}
 void tick(Player* player)override{
  touch->tick(player);keyboard.tick(player);wasJumping=jumping;
  jumping=touch->jumping||keyboard.jumping;sneaking=touch->sneaking||keyboard.sneaking;
  float touchScale=touch->sneaking?.3f:1.f,keyScale=keyboard.sneaking?.3f:1.f,resultScale=sneaking?.3f:1.f;
  xa=bounded(touch->xa/touchScale+keyboard.xa/keyScale)*resultScale;
  ya=bounded(touch->ya/touchScale+keyboard.ya/keyScale)*resultScale;
  wantUp=touch->wantUp||keyboard.jumping;wantDown=touch->wantDown||keyboard.sneaking;
  isChangingFlightHeight=touch->isChangingFlightHeight||(player&&player->abilities.flying&&(keyboard.jumping||keyboard.sneaking));
 }
};
#endif
