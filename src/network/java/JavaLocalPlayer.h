#ifndef TCPE_JAVA_LOCAL_PLAYER_H
#define TCPE_JAVA_LOCAL_PLAYER_H
#include "client/player/LocalPlayer.h"
#include "client/player/input/IMoveInput.h"
#include "client/Minecraft.h"
#include "JavaGameSession.h"
#include "JavaHealth.h"
#include "world/entity/player/Inventory.h"
// Original TCPE input and player presentation, authoritative native Java motion.
class JavaLocalPlayer:public LocalPlayer {
 Minecraft* app;
 int lastSelected=-1;
 unsigned appliedAbilitiesRevision=~0u;
public:
 JavaLocalPlayer(Minecraft* app,Level* level,User* user,int dimension):LocalPlayer(app,level,user,dimension,false),app(app){}
 bool hurt(Entity*,int) override { return false; } // Java sends authoritative health.
 void tick() override {
  if(app->javaSession) {
   if(appliedAbilitiesRevision!=app->javaSession->abilitiesRevision){abilities.mayfly=app->javaSession->canFly;abilities.flying=app->javaSession->flying;appliedAbilitiesRevision=app->javaSession->abilitiesRevision;}
   // Keep the native player available for server respawn rather than removing
   // it after the legacy death countdown.
   deathTime=0;
   int selected=inventory->selected;
   if(selected>=0 && selected<9 && selected!=lastSelected) {
    if(app->javaSession->connection.selectHotbar(selected)) lastSelected=selected;
   }
  }
  LocalPlayer::tick();
 }
 void travel(float,float)override {
  auto* session=app->javaSession;if(!session)return;
  const bool blocked=app->screen!=NULL;
  // Original input has already applied crouch/use slowdown. Java physics
  // applies its authoritative modifiers, so send the original stick strength.
  const float inputScale=input?(input->sneaking?.3f:1.f)*(isUsingItem()?.35f:1.f):1.f;
  const float forward=!blocked&&input?input->ya/inputScale:0.f;
  const float strafe=!blocked&&input?input->xa/inputScale:0.f;
  const int vertical=!blocked&&input?int(input->wantUp)-int(input->wantDown):0;
  if(session->hasAbilities)session->connection.send("{\"type\":\"flight\",\"toggle\":"+std::string(abilities.flying!=session->flying?"true":"false")+",\"flying\":"+std::string(abilities.flying?"true":"false")+",\"vertical\":"+std::to_string(vertical)+"}");
  session->connection.look(yRot,xRot);
  session->connection.walk(forward,strafe,!blocked&&input&&input->jumping,!blocked&&input&&input->sneaking,!blocked&&isSprinting());
  // Rust snapshots already include Java eye height; setPos expects the TCPE eye origin.
  if(session->hasPosition)setPos(static_cast<float>(session->position[0]),static_cast<float>(session->position[1]),static_cast<float>(session->position[2]));
  xd=yd=zd=0;health=javaNativeHealth(session->health);
 }
};
#endif
