#ifndef TCPE_JAVA_GAME_MODE_H
#define TCPE_JAVA_GAME_MODE_H
#include "client/gamemode/SurvivalMode.h"
#include "client/Minecraft.h"
#include "world/entity/Entity.h"
#include "JavaGameSession.h"
#include "platform/input/Keyboard.h"
#include "platform/input/Mouse.h"
#include "platform/input/ControllerActionKeys.h"
#include "client/player/input/touchscreen/TouchInputHolder.h"
// TCPE input invokes these same hooks for keyboard, mouse and touch. Java owns
// block edits, consumption and combat; never apply legacy local predictions.
class JavaGameMode : public SurvivalMode {
 bool mining=false,usingItem=false,physicalUse=false,touchUse=false;
 bool touchUseHeld()const{auto* input=minecraft->inputHolder?dynamic_cast<UnifiedTurnBuild*>(minecraft->inputHolder->getBuildInput()):nullptr;return minecraft->useTouchscreen()&&input&&input->chargedUseHeld();}
 bool useControlHeld()const{return (minecraft->options.keyUse.key&&Keyboard::isKeyDown(minecraft->options.keyUse.key))||Keyboard::isKeyDown(ControllerKeys::Use)||(!minecraft->useTouchscreen()&&Mouse::isButtonDown(MouseAction::ACTION_RIGHT));}
 JavaGameSession* session() const { return minecraft->javaSession; }
 void mine(bool held) {
  if (session()) session()->connection.send(held ? "{\"type\":\"mine\",\"held\":true}" : "{\"type\":\"mine\",\"held\":false}");
  mining=held;
 }
 bool use() { physicalUse=useControlHeld();touchUse=touchUseHeld();usingItem=session() && session()->connection.send(session()->useOffHand?"{\"type\":\"use\",\"hand\":\"off\"}":"{\"type\":\"use\"}"); return usingItem; }
public:
 explicit JavaGameMode(Minecraft* app):SurvivalMode(app){}
 void startDestroyBlock(int,int,int,int) override { mine(true); }
 // The worker may cancel mining after an entity pick or target change.
 // Reassert the physical hold; Rust keeps an existing mining target idempotent.
 void continueDestroyBlock(int,int,int,int) override { mine(true); }
 void stopDestroyBlock() override { if(mining) mine(false); destroyProgress=oDestroyProgress=0; }
 bool destroyBlock(int,int,int,int) override { mine(true); return false; }
 bool useItem(Player*,Level*,ItemInstance*) override { return use(); }
 bool useItemOn(Player*,Level*,ItemInstance*,int,int,int,int,const Vec3&) override { return use(); }
 void attack(Player*,Entity* target) override {stopDestroyBlock();if(session()&&target)session()->connection.send("{\"type\":\"attack\",\"entity_id\":"+std::to_string(target->entityId)+"}");}
 void interact(Player*,Entity*) override {use();} // Ordinary Rust pick, never the extended weapon-only target.
 void releaseUsingItem(Player*) override { if(session()) session()->connection.send("{\"type\":\"release_use\"}"); usingItem=false; }
 void tick() override {
  if(usingItem&&((physicalUse&&!useControlHeld())||(touchUse&&!touchUseHeld())))releaseUsingItem(NULL);
  if(minecraft->screen) {
   stopDestroyBlock();
   if(usingItem) releaseUsingItem(NULL);
  }
  GameMode::tick();
 }
 void handleCloseInventory(int id,Player*) override {
  if(session()) session()->connection.send("{\"type\":\"close_container\",\"window_id\":"+std::to_string(id)+"}");
 }
 ItemInstance* handleInventoryMouseClick(int id,int slot,int button,Player*) override {
  if(session() && slot>=0) session()->connection.send("{\"type\":\"click_slot\",\"window_id\":"+std::to_string(id)+",\"slot\":"+std::to_string(slot)+",\"right\":"+(button==1?"true":"false")+"}");
  return NULL;
 }
};
#endif
