#include "JavaEntityAdapter.h"
#include "JavaGameSession.h"
#include "JavaDisplayRenderer.h"
#include "JavaItemIcons.h"
#include "JavaShieldModel.h"
#include "JavaDroppedCompass.h"
#include "JavaFlyingMobModel.h"
#include "JavaIronGolemModel.h"
#include "JavaWindChargeModel.h"
#include "JavaPackItemModels.h"
#include "client/renderer/Tesselator.h"
#include "util/Random.h"
#include "util/Mth.h"
#include "client/renderer/entity/EntityRenderDispatcher.h"
#include "world/level/Level.h"
#include "world/level/dimension/Dimension.h"
#include "client/player/RemotePlayer.h"
#include "world/entity/monster/Zombie.h"
#include "world/entity/monster/PigZombie.h"
#include "world/entity/monster/Skeleton.h"
#include "world/entity/monster/Creeper.h"
#include "world/entity/monster/Spider.h"
#include "world/entity/animal/Pig.h"
#include "world/entity/animal/Cow.h"
#include "world/entity/animal/Chicken.h"
#include "world/entity/animal/Sheep.h"
#include "world/item/ItemInstance.h"
#include "world/entity/item/ItemEntity.h"
#include "world/entity/projectile/Arrow.h"
#include "JavaTridentModel.h"
#include "world/item/Item.h"
#include <cmath>
#include <set>
// Only these wrappers skip autonomous legacy AI/physics; offline entities keep
// their original ticks. Unsupported modern types deliberately use the zombie
// mesh as an explicit temporary visual fallback, with server ID/bounds retained.
struct JavaPresentation {
 std::string serverKind;
 JavaGameSession::Entity snapshot;
 virtual ~JavaPresentation(){}
 virtual void apply(const JavaGameSession::Entity& snapshot)=0;
};
const JavaGameSession::Item* javaCarriedItem(Entity* entity,bool offHand){auto* p=dynamic_cast<JavaPresentation*>(entity);return p?&p->snapshot.equipment[offHand?1:0]:nullptr;}
const JavaGameSession::Entity* javaPresentationSnapshot(Entity* entity){auto* p=dynamic_cast<JavaPresentation*>(entity);return p?&p->snapshot:nullptr;}
unsigned javaPlayerSkinLayers(Entity* entity){auto* p=dynamic_cast<JavaPresentation*>(entity);return p?p->snapshot.skinLayers:127;}
bool javaPlayerSkinModel(Entity* entity,bool& slim){auto* p=dynamic_cast<JavaPresentation*>(entity);if(!p||!entity->isPlayer()||!p->snapshot.skinModern||p->snapshot.skinTexture.empty())return false;slim=p->snapshot.profileSkinSlim;return true;}
static ItemInstance item(const JavaGameSession::Item& value) {
 if(value.count<=0 || value.originalId<=0 || value.originalId>=Item::MAX_ITEMS || !Item::items[value.originalId]) return ItemInstance();
 return ItemInstance(value.originalId,value.count,value.originalData);
}
template<class Base> class JavaEntity : public Base, public JavaPresentation {
 std::array<ItemInstance,6> gear;
 bool crouch=false;
public:
 template<class... Args> JavaEntity(Level* level,Args... args):Base(level,args...) {this->noPhysics=true;this->reallyRemoveIfPlayer=true;}
 void tick() override {++this->tickCount;} // Snapshot drives every pose.
 void aiStep() override {}
 bool hurt(Entity*,int) override {return false;}
 bool isSneaking() override {return crouch;}
 ItemInstance* getCarriedItem() override {return gear[0].isNull()?NULL:&gear[0];}
 void apply(const JavaGameSession::Entity& snapshot) override {
  JavaPresentation::snapshot=snapshot;
  this->xo=this->x;this->yo=this->y;this->zo=this->z;
  this->xOld=this->x;this->yOld=this->y;this->zOld=this->z;
  this->yRotO=this->yRot;this->xRotO=this->xRot;this->yBodyRotO=this->yBodyRot;
  const float dx=float(snapshot.position[0])-this->x,dz=float(snapshot.position[2])-this->z;
  this->walkAnimSpeedO=this->walkAnimSpeed;this->walkAnimSpeed=std::min(1.f,std::sqrt(dx*dx+dz*dz)*4.f);this->walkAnimPos+=this->walkAnimSpeed;
  // Java snapshots use feet positions. TCPE player origin includes heightOffset.
  this->setPos(float(snapshot.position[0]),float(snapshot.position[1])+this->heightOffset,float(snapshot.position[2]));
  this->yBodyRot=float(snapshot.yaw);this->yRot=float(snapshot.headYaw);this->xRot=float(snapshot.pitch);
  this->xd=this->yd=this->zd=0;this->hurtTime=int(snapshot.hurtTicks);this->hurtDuration=10;this->deathTime=int(snapshot.deathTicks);
  crouch=snapshot.sneaking;this->setSneaking(crouch);
  for(size_t i=0;i<gear.size();++i)gear[i]=item(snapshot.equipment[i]);
  // The native arm renderer needs a valid item to enter its held-item path.
  // Its visual is always taken from the authoritative Java stack below.
  const auto& held=snapshot.equipment[0];const std::string model=held.itemModel.empty()?held.resourceId:held.itemModel;
  if(gear[0].isNull()&&held.count>0&&Item::stick&&(JavaPackItemModels::has(model)||JavaItemIcons::has(held.resourceId)))gear[0]=ItemInstance(Item::stick->id,held.count,0);

  if(this->isPlayer()) {
   Player* player=Player::asPlayer(this);player->name=snapshot.profileName;this->textureName=snapshot.skinTexture.empty()?"mob/char.png":snapshot.skinTexture;
   // Snapshot gear order: main hand, off hand, boots, legs, chest, helmet.
   for(int armor=0;armor<4;++armor)player->setArmor(armor,&gear[5-armor]);
  }
 }
};
class JavaDroppedItem : public ItemEntity,public JavaPresentation {
public:
 JavaGameSession::CompassState compass;
 JavaDroppedCompass compassNeedle;
 explicit JavaDroppedItem(Level* level):ItemEntity(level){noPhysics=true;reallyRemoveIfPlayer=true;}
 void tick()override{++age;++Entity::tickCount;}
 void playerTouch(Player*)override{} // Pickup is decided by the Java server.
 bool hurt(Entity*,int)override{return false;}
 void apply(const JavaGameSession::Entity& value)override{
  snapshot=value;xo=x;yo=y;zo=z;xOld=x;yOld=y;zOld=z;yRotO=yRot;xRotO=xRot;
  setPos(float(value.position[0]),float(value.position[1])+heightOffset,float(value.position[2]));
  yRot=float(value.yaw);xRot=float(value.pitch);xd=yd=zd=0;
  item=::item(value.droppedItem);
 }
};
class JavaFlyingEntity : public Entity,public JavaPresentation {
public:
 explicit JavaFlyingEntity(Level* level):Entity(level){noPhysics=true;reallyRemoveIfPlayer=true;}
 int getEntityTypeId()const override{return 126;}
 bool isPickable()override{return true;}
 bool isShootable()override{return true;}
 void readAdditionalSaveData(CompoundTag*)override{}
 void addAdditonalSaveData(CompoundTag*)override{}
 void tick()override{++tickCount;}
 bool hurt(Entity*,int)override{return false;}
 void apply(const JavaGameSession::Entity& value)override{
  snapshot=value;xo=x;yo=y;zo=z;xOld=x;yOld=y;zOld=z;yRotO=yRot;xRotO=xRot;
  const float width=serverKind=="Bee"?.7f:.35f;setSize(width,.6f);
  setPos(float(value.position[0]),float(value.position[1]),float(value.position[2]));yRot=float(value.yaw);xRot=float(value.pitch);
 }
 void render(EntityRenderDispatcher& dispatcher,float x,float y,float z,float partial){
  if(snapshot.invisible||!dispatcher.textures)return;
  static JavaFlyingMobModel bee(true),allay(false);auto& model=serverKind=="Bee"?bee:allay;
  const bool cull=glIsEnabled(GL_CULL_FACE),blend=glIsEnabled(GL_BLEND),alpha=glIsEnabled(GL_ALPHA_TEST);GLint alphaFunction=0;GLfloat alphaRef=0;glGetIntegerv(GL_ALPHA_TEST_FUNC,&alphaFunction);glGetFloatv(GL_ALPHA_TEST_REF,&alphaRef);
  glDisable2(GL_CULL_FACE);glEnable2(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.1f);glEnable2(GL_BLEND);glBlendFunc2(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  dispatcher.textures->loadAndBindTexture(JavaFlyingMobModel::texture(serverKind));
  // AllayRenderer forces block light15. Native combined lighting is
  // max(block,sky), so use its ramp at15; Bee keeps world lighting.
  const float brightness=serverKind=="Allay"?level->dimension->brightnessRamp[15]:getBrightness(partial);glColor4f2(brightness,brightness,brightness,1);
  glPushMatrix2();glTranslatef2(x,y,z);glRotatef2(180-float(snapshot.yaw),0,1,0);glScalef2(-1,-1,1);glTranslatef2(0,-1.5f,0);
  model.renderFlying(tickCount+partial,float(snapshot.headYaw-snapshot.yaw),float(snapshot.pitch));glPopMatrix2();
  if(cull)glEnable2(GL_CULL_FACE);if(!blend)glDisable2(GL_BLEND);if(!alpha)glDisable2(GL_ALPHA_TEST);glAlphaFunc(alphaFunction,alphaRef);glColor4f2(1,1,1,1);
 }
};
class JavaWindChargeEntity : public Entity,public JavaPresentation {
public:
 explicit JavaWindChargeEntity(Level* level):Entity(level){noPhysics=true;reallyRemoveIfPlayer=true;setSize(.3125f,.3125f);}
 int getEntityTypeId()const override{return 126;}
 bool isPickable()override{return true;}
 bool isShootable()override{return true;}
 bool hurt(Entity*,int)override{return false;}
 void readAdditionalSaveData(CompoundTag*)override{}
 void addAdditonalSaveData(CompoundTag*)override{}
 void tick()override{++tickCount;}
 void apply(const JavaGameSession::Entity& value)override{
  snapshot=value;xo=x;yo=y;zo=z;xOld=x;yOld=y;zOld=z;
  setPos(float(value.position[0]),float(value.position[1]),float(value.position[2]));
 }
 void render(EntityRenderDispatcher& dispatcher,float x,float y,float z,float partial){
  if(snapshot.invisible||!dispatcher.textures)return;
  static JavaWindChargeModel model;
  GLint mode,source,destination;glGetIntegerv(GL_MATRIX_MODE,&mode);glGetIntegerv(GL_BLEND_SRC,&source);glGetIntegerv(GL_BLEND_DST,&destination);
  const bool blend=glIsEnabled(GL_BLEND),cull=glIsEnabled(GL_CULL_FACE),lighting=glIsEnabled(GL_LIGHTING),alpha=glIsEnabled(GL_ALPHA_TEST),textured=glIsEnabled(GL_TEXTURE_2D);
  GLfloat color[4];glGetFloatv(GL_CURRENT_COLOR,color);
  dispatcher.textures->loadAndBindTexture("mob/java-wind-charge.png");
  glEnable2(GL_TEXTURE_2D);
  glEnable2(GL_BLEND);glBlendFunc2(GL_ONE,GL_ONE);glDisable2(GL_CULL_FACE);glDisable2(GL_LIGHTING);glDisable2(GL_ALPHA_TEST);
  const float age=tickCount+(std::isfinite(partial)?partial:0);
  glMatrixMode(GL_TEXTURE);glPushMatrix();glTranslatef(JavaWindChargeModel::scroll(age),0,0);
  glMatrixMode(GL_MODELVIEW);glPushMatrix2();glTranslatef2(x,y,z);glScalef2(-1,-1,1);glColor4f2(1,1,1,1);model.renderAge(age);glPopMatrix2();
  glMatrixMode(GL_TEXTURE);glPopMatrix();glMatrixMode(mode);
  glBlendFunc2(source,destination);if(!blend)glDisable2(GL_BLEND);if(cull)glEnable2(GL_CULL_FACE);if(lighting)glEnable2(GL_LIGHTING);if(alpha)glEnable2(GL_ALPHA_TEST);if(!textured)glDisable2(GL_TEXTURE_2D);glColor4f2(color[0],color[1],color[2],color[3]);
 }
};
class JavaIronGolemEntity : public Entity,public JavaPresentation {
 float walkPosition=0,walkSpeed=0,oldWalkPosition=0,oldWalkSpeed=0,targetSpeed=0;int lastMovementTick=-100;bool positioned=false;
public:
 explicit JavaIronGolemEntity(Level* level):Entity(level){noPhysics=true;reallyRemoveIfPlayer=true;setSize(1.4f,2.7f);}
 int getEntityTypeId()const override{return 125;}
 bool isPickable()override{return true;}
 bool isShootable()override{return true;}
 void readAdditionalSaveData(CompoundTag*)override{}
 void addAdditonalSaveData(CompoundTag*)override{}
 bool hurt(Entity*,int)override{return false;}
 void tick()override{
  ++tickCount;oldWalkSpeed=walkSpeed;oldWalkPosition=walkPosition;
  if(tickCount-lastMovementTick>2)targetSpeed=0;
  walkSpeed+=(targetSpeed-walkSpeed)*.4f;walkPosition+=walkSpeed;
 }
 void apply(const JavaGameSession::Entity& value)override{
  if(positioned){const float dx=float(value.position[0])-x,dz=float(value.position[2])-z;const float distance=std::sqrt(dx*dx+dz*dz);if(distance>0){const int elapsed=std::max(1,std::min(2,tickCount-lastMovementTick));targetSpeed=std::min(1.f,distance*4/elapsed);lastMovementTick=tickCount;}}
  else positioned=true;
  snapshot=value;xo=x;yo=y;zo=z;xOld=x;yOld=y;zOld=z;yRotO=yRot;xRotO=xRot;
  setPos(float(value.position[0]),float(value.position[1]),float(value.position[2]));yRot=float(value.yaw);xRot=float(value.pitch);
 }
 void render(EntityRenderDispatcher& dispatcher,float x,float y,float z,float partial){
  if(snapshot.invisible||!dispatcher.textures)return;
  static JavaIronGolemModel model;
  const bool cull=glIsEnabled(GL_CULL_FACE),blend=glIsEnabled(GL_BLEND),alpha=glIsEnabled(GL_ALPHA_TEST);GLint alphaFunction=0;GLfloat alphaRef=0;glGetIntegerv(GL_ALPHA_TEST_FUNC,&alphaFunction);glGetFloatv(GL_ALPHA_TEST_REF,&alphaRef);
  glDisable2(GL_CULL_FACE);glEnable2(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.1f);glEnable2(GL_BLEND);glBlendFunc2(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  dispatcher.textures->loadAndBindTexture(JavaIronGolemModel::texture());const float brightness=getBrightness(partial);glColor4f2(brightness,brightness,brightness,1);
  const float speed=oldWalkSpeed+(walkSpeed-oldWalkSpeed)*partial,position=oldWalkPosition+(walkPosition-oldWalkPosition)*partial;
  glPushMatrix2();glTranslatef2(x,y,z);glRotatef2(180-float(snapshot.yaw),0,1,0);
  if(speed>=.01f)glRotatef2(6.5f*JavaIronGolemModel::triangle(position+6,13),0,0,1);
  glScalef2(-1,-1,1);glTranslatef2(0,-1.5f,0);
  const float attack=std::max(0.f,float(snapshot.mobAttackTicks)-partial);const int flower=int(snapshot.mobFlowerTicks);
  model.renderPose(position,speed,float(snapshot.headYaw-snapshot.yaw),float(snapshot.pitch),attack,flower);
  if(snapshot.hasLivingHealth){const char* cracks=JavaIronGolemModel::crackTexture(snapshot.livingHealth,snapshot.hasLivingMaxHealth?snapshot.livingMaxHealth:100.f);if(cracks){GLint depth=0;glGetIntegerv(GL_DEPTH_FUNC,&depth);glDepthFunc(GL_EQUAL);dispatcher.textures->loadAndBindTexture(cracks);model.renderPose(position,speed,float(snapshot.headYaw-snapshot.yaw),float(snapshot.pitch),attack,flower);glDepthFunc(depth);}}
  if(flower>0)model.renderFlower(dispatcher.textures,brightness);
  glPopMatrix2();
  if(cull)glEnable2(GL_CULL_FACE);if(!blend)glDisable2(GL_BLEND);if(!alpha)glDisable2(GL_ALPHA_TEST);glAlphaFunc(alphaFunction,alphaRef);glColor4f2(1,1,1,1);
 }
};
class JavaDisplayEntity : public Entity,public JavaPresentation {
public:
 explicit JavaDisplayEntity(Level* level):Entity(level){noPhysics=true;reallyRemoveIfPlayer=true;setSize(.5f,.5f);}
 int getEntityTypeId()const override{return 127;} // Client-only display; never serialized.
 void readAdditionalSaveData(CompoundTag*)override{}
 void addAdditonalSaveData(CompoundTag*)override{}
 void tick()override{++tickCount;}
 bool isPickable()override{return (serverKind=="Interaction"||(serverKind=="ArmorStand"&&!snapshot.armorStandMarker))&&bbWidth>0.f&&bbHeight>0.f;}
 bool isShootable()override{return serverKind=="ArmorStand"&&!snapshot.armorStandMarker;}
 bool hurt(Entity*,int)override{return false;}
 bool shouldRenderAtSqrDistance(float distance)override{const float range=64.f*float(snapshot.display.viewRange);return distance<=range*range;}
 void apply(const JavaGameSession::Entity& value)override{
  snapshot=value;xo=x;yo=y;zo=z;xOld=x;yOld=y;zOld=z;yRotO=yRot;xRotO=xRot;
  if(serverKind=="Marker"||(serverKind=="ArmorStand"&&value.armorStandMarker))setSize(0.f,0.f);
  else if(serverKind=="Interaction"||serverKind=="ArmorStand")setSize(std::isfinite(value.hitbox[0])&&value.hitbox[0]>=0?float(value.hitbox[0]):0.f,std::isfinite(value.hitbox[1])&&value.hitbox[1]>=0?float(value.hitbox[1]):0.f);
  else setSize(float(std::max(.5,value.display.width)),float(std::max(.5,value.display.height)));
  setPos(float(value.position[0]),float(value.position[1]),float(value.position[2]));yRot=float(value.yaw);xRot=float(value.pitch);
 }
};
class JavaTridentEntity : public Entity,public JavaPresentation {
public:
 explicit JavaTridentEntity(Level* level):Entity(level){noPhysics=true;reallyRemoveIfPlayer=true;setSize(.5f,.5f);}
 int getEntityTypeId()const override{return 124;}
 void readAdditionalSaveData(CompoundTag*)override{}
 void addAdditonalSaveData(CompoundTag*)override{}
 void tick()override{++tickCount;}
 bool hurt(Entity*,int)override{return false;}
 void apply(const JavaGameSession::Entity& value)override{snapshot=value;xo=x;yo=y;zo=z;xOld=x;yOld=y;zOld=z;yRotO=yRot;xRotO=xRot;setPos(float(value.position[0]),float(value.position[1]),float(value.position[2]));yRot=float(value.yaw);xRot=float(value.pitch);xd=yd=zd=0;}
 void render(EntityRenderDispatcher& dispatcher,float x,float y,float z,float partial){
  if(snapshot.invisible||!dispatcher.textures)return;static JavaTridentModel model;
  GLint mode,alphaFunction;GLfloat color[4],alphaRef;glGetIntegerv(GL_MATRIX_MODE,&mode);glGetIntegerv(GL_ALPHA_TEST_FUNC,&alphaFunction);glGetFloatv(GL_ALPHA_TEST_REF,&alphaRef);glGetFloatv(GL_CURRENT_COLOR,color);
  const bool cull=glIsEnabled(GL_CULL_FACE),alpha=glIsEnabled(GL_ALPHA_TEST),textured=glIsEnabled(GL_TEXTURE_2D);
  glDisable2(GL_CULL_FACE);glEnable2(GL_TEXTURE_2D);glEnable2(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.1f);dispatcher.textures->loadAndBindTexture("item/java-trident.png");const float brightness=getBrightness(partial);glColor4f2(brightness,brightness,brightness,1);
  glMatrixMode(GL_MODELVIEW);glPushMatrix2();glTranslatef2(x,y,z);glRotatef2(yRotO+(yRot-yRotO)*partial-90,0,1,0);glRotatef2(xRotO+(xRot-xRotO)*partial+90,0,0,1);model.draw();glPopMatrix2();glMatrixMode(mode);
  glAlphaFunc(alphaFunction,alphaRef);if(cull)glEnable2(GL_CULL_FACE);else glDisable2(GL_CULL_FACE);if(alpha)glEnable2(GL_ALPHA_TEST);else glDisable2(GL_ALPHA_TEST);if(textured)glEnable2(GL_TEXTURE_2D);else glDisable2(GL_TEXTURE_2D);glColor4f2(color[0],color[1],color[2],color[3]);
 }
};
class JavaThrownItemEntity : public Entity,public JavaPresentation {
public:
 explicit JavaThrownItemEntity(Level* level):Entity(level){noPhysics=true;reallyRemoveIfPlayer=true;setSize(.25f,.25f);}
 int getEntityTypeId()const override{return 125;}
 void readAdditionalSaveData(CompoundTag*)override{}
 void addAdditonalSaveData(CompoundTag*)override{}
 void tick()override{++tickCount;}
 bool hurt(Entity*,int)override{return false;}
 bool shouldRenderAtSqrDistance(float distance)override{if(serverKind=="FireworkRocket")return snapshot.fireworkAttachedTo<0&&distance<4096.f;return Entity::shouldRenderAtSqrDistance(distance);}
 void apply(const JavaGameSession::Entity& value)override{
  snapshot=value;xo=x;yo=y;zo=z;xOld=x;yOld=y;zOld=z;yRotO=yRot;xRotO=xRot;
  setPos(float(value.position[0]),float(value.position[1]),float(value.position[2]));yRot=float(value.yaw);xRot=float(value.pitch);xd=yd=zd=0;
 }
 void render(EntityRenderDispatcher& dispatcher,float x,float y,float z,float partial){
  if(snapshot.invisible||!dispatcher.textures||(serverKind=="FireworkRocket"&&snapshot.fireworkAttachedTo>=0))return;
  auto appearance=snapshot.droppedItem;
  // Explicit Empty metadata remains invisible; the protocol default is a real
  // rocket stack. An attached boost rocket is hidden by vanilla's render gate.
  if(serverKind=="FireworkRocket"&&appearance.count<=0)return;
  if(appearance.count<=0){appearance.count=1;appearance.originalId=-1;appearance.resourceId=serverKind=="Snowball"?"minecraft:snowball":serverKind=="Egg"?"minecraft:egg":serverKind=="EnderPearl"?"minecraft:ender_pearl":serverKind=="SplashPotion"?"minecraft:splash_potion":serverKind=="LingeringPotion"?"minecraft:lingering_potion":"minecraft:experience_bottle";}
  const std::string model=appearance.itemModel.empty()?appearance.resourceId:appearance.itemModel;
  GLint mode,source,destination,alphaFunction;GLfloat color[4],alphaRef;glGetIntegerv(GL_MATRIX_MODE,&mode);glGetIntegerv(GL_BLEND_SRC,&source);glGetIntegerv(GL_BLEND_DST,&destination);glGetIntegerv(GL_ALPHA_TEST_FUNC,&alphaFunction);glGetFloatv(GL_ALPHA_TEST_REF,&alphaRef);glGetFloatv(GL_CURRENT_COLOR,color);
  const bool blend=glIsEnabled(GL_BLEND),cull=glIsEnabled(GL_CULL_FACE),lighting=glIsEnabled(GL_LIGHTING),alpha=glIsEnabled(GL_ALPHA_TEST),textured=glIsEnabled(GL_TEXTURE_2D);
  glDisable2(GL_CULL_FACE);glDisable2(GL_LIGHTING);glEnable2(GL_TEXTURE_2D);glEnable2(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.1f);glEnable2(GL_BLEND);glBlendFunc2(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  glMatrixMode(GL_MODELVIEW);glPushMatrix2();glTranslatef2(x,y,z);glRotatef2(180-dispatcher.playerRotY,0,1,0);glRotatef2(-dispatcher.playerRotX,1,0,0);
  if(serverKind=="FireworkRocket"&&snapshot.fireworkShotAtAngle){glRotatef2(180,0,0,1);glRotatef2(180,0,1,0);glRotatef2(90,1,0,0);}
  const float brightness=getBrightness(partial);
  if(JavaPackItemModels::has(model))JavaPackItemModels::renderContext(dispatcher.textures,model,"ground",brightness,&appearance);
  else {glTranslatef2(0,-.125f,.015625f);JavaItemIcons::renderWorld(dispatcher.textures,appearance.resourceId,.5f,brightness,&appearance);}
  glPopMatrix2();glMatrixMode(mode);glBlendFunc2(source,destination);glAlphaFunc(alphaFunction,alphaRef);
  if(blend)glEnable2(GL_BLEND);else glDisable2(GL_BLEND);if(cull)glEnable2(GL_CULL_FACE);else glDisable2(GL_CULL_FACE);if(lighting)glEnable2(GL_LIGHTING);else glDisable2(GL_LIGHTING);if(alpha)glEnable2(GL_ALPHA_TEST);else glDisable2(GL_ALPHA_TEST);if(textured)glEnable2(GL_TEXTURE_2D);else glDisable2(GL_TEXTURE_2D);glColor4f2(color[0],color[1],color[2],color[3]);
 }
};
bool renderJavaPresentation(Entity* entity,EntityRenderDispatcher& dispatcher,float x,float y,float z,float partial){
 auto* presentation=dynamic_cast<JavaPresentation*>(entity);if(!presentation)return false;
 const auto& snapshot=presentation->snapshot;
 if(auto* trident=dynamic_cast<JavaTridentEntity*>(entity)){trident->render(dispatcher,x,y,z,partial);return true;}
 if(auto* thrown=dynamic_cast<JavaThrownItemEntity*>(entity)){thrown->render(dispatcher,x,y,z,partial);return true;}
 if(auto* charge=dynamic_cast<JavaWindChargeEntity*>(entity)){charge->render(dispatcher,x,y,z,partial);return true;}
 if(auto* golem=dynamic_cast<JavaIronGolemEntity*>(entity)){golem->render(dispatcher,x,y,z,partial);renderJavaCustomName(snapshot,dispatcher,x,y,z,entity->bbHeight);return true;}
 if(auto* flying=dynamic_cast<JavaFlyingEntity*>(entity)){flying->render(dispatcher,x,y,z,partial);renderJavaCustomName(snapshot,dispatcher,x,y,z,entity->bbHeight);return true;}
 if(auto* dropped=dynamic_cast<JavaDroppedItem*>(entity)){
  if(snapshot.invisible||snapshot.droppedItem.count<=0)return true;
  const std::string modelId=snapshot.droppedItem.itemModel.empty()?snapshot.droppedItem.resourceId:snapshot.droppedItem.itemModel;
  const bool customModel=JavaPackItemModels::has(modelId);
  if(dispatcher.textures&&(customModel||JavaItemIcons::has(snapshot.droppedItem.resourceId))){
   const bool shield=snapshot.droppedItem.resourceId=="minecraft:shield";
   const bool block=customModel||shield||JavaItemIcons::isBlock(snapshot.droppedItem.resourceId);
   const std::string icon=customModel?snapshot.droppedItem.resourceId:dropped->compassNeedle.resource(dropped->compass,snapshot.droppedItem,dropped->x,dropped->z,float(dropped->age),dropped->bobOffs,static_cast<unsigned long long>(dropped->age),dropped->entityId);
   const int count=snapshot.droppedItem.count>20?4:snapshot.droppedItem.count>5?3:snapshot.droppedItem.count>1?2:1;
   Random random(187);partial=std::isfinite(partial)?std::max(0.f,std::min(1.f,partial)):0.f;const float age=dropped->age+partial;const float brightness=dropped->getBrightness(partial);
   glPushMatrix2();glTranslatef2(x,y+Mth::sin(age/10.f+dropped->bobOffs)*.1f+.1f,z);
   if(block)glRotatef2((age/20.f+dropped->bobOffs)*Mth::RADDEG,0,1,0);
   for(int i=0;i<count;++i){glPushMatrix2();if(i>0){const float spread=block?.2f:.15f;const float dx=(random.nextFloat()*2-1)*spread,dy=(random.nextFloat()*2-1)*spread,dz=(random.nextFloat()*2-1)*spread;glTranslatef2(dx,dy,dz);}if(!block)glRotatef2(180-dispatcher.playerRotY,0,1,0);
    if(customModel)JavaPackItemModels::renderGround(dispatcher.textures,modelId,brightness,&snapshot.droppedItem);else if(shield)JavaShieldModel::renderGround(dispatcher.textures,brightness,&snapshot.droppedItem);else JavaItemIcons::renderWorld(dispatcher.textures,icon,.5f,brightness,&snapshot.droppedItem);glPopMatrix2();}
   glPopMatrix2();return true;
  }
  if(dropped->item.isNull())return true;
 }
 if(snapshot.kind=="Marker"||snapshot.kind=="Interaction")return true; // No visual/name/mesh.
 if(dynamic_cast<JavaDisplayEntity*>(entity)){renderJavaDisplay(snapshot,dispatcher,x,y,z);return true;}
 renderJavaCustomName(snapshot,dispatcher,x,y,z,entity->bbHeight);
 return snapshot.invisible;
}
// Java arrows retain TCPE's arrow mesh while server snapshots own motion.
// No legacy collision, pickup or projectile simulation runs on these actors.
class JavaArrowEntity : public Arrow,public JavaPresentation {
public:
 explicit JavaArrowEntity(Level* level):Arrow(level){noPhysics=true;reallyRemoveIfPlayer=true;shakeTime=0;}
 void tick()override{++tickCount;}
 void playerTouch(Player*)override{}
 bool hurt(Entity*,int)override{return false;}
 void apply(const JavaGameSession::Entity& value)override{
  snapshot=value;xo=x;yo=y;zo=z;xOld=x;yOld=y;zOld=z;yRotO=yRot;xRotO=xRot;
  setSize(float(value.hitbox[0]),float(value.hitbox[1]));setPos(float(value.position[0]),float(value.position[1]),float(value.position[2]));
  yRot=float(value.yaw);xRot=float(value.pitch);xd=yd=zd=0;shakeTime=0;
 }
};
static Entity* create(Level* level,const std::string& kind) {
 if(kind=="Trident")return new JavaTridentEntity(level);
 if(kind=="FireworkRocket"||kind=="Snowball"||kind=="Egg"||kind=="EnderPearl"||kind=="ExperienceBottle"||kind=="SplashPotion"||kind=="LingeringPotion")return new JavaThrownItemEntity(level);
 if(kind=="Arrow"||kind=="SpectralArrow")return new JavaArrowEntity(level);
 if(kind=="WindCharge"||kind=="BreezeWindCharge")return new JavaWindChargeEntity(level);
 if(kind=="IronGolem")return new JavaIronGolemEntity(level);
 if(JavaFlyingMobModel::supports(kind))return new JavaFlyingEntity(level);
 if(kind=="Item")return new JavaDroppedItem(level);
 if(kind=="TextDisplay"||kind=="BlockDisplay"||kind=="ItemDisplay"||kind=="ArmorStand"||kind=="Marker"||kind=="Interaction")return new JavaDisplayEntity(level);
 if(kind=="Player"||kind=="Mannequin")return new JavaEntity<RemotePlayer>(level,false);
 if(kind=="Zombie")return new JavaEntity<Zombie>(level);
 if(kind=="ZombifiedPiglin")return new JavaEntity<PigZombie>(level);
 if(kind=="Skeleton")return new JavaEntity<Skeleton>(level);
 if(kind=="Creeper")return new JavaEntity<Creeper>(level);
 if(kind=="Spider")return new JavaEntity<Spider>(level);
 if(kind=="Pig")return new JavaEntity<Pig>(level);
 if(kind=="Cow")return new JavaEntity<Cow>(level);
 if(kind=="Chicken")return new JavaEntity<Chicken>(level);
 if(kind=="Sheep")return new JavaEntity<Sheep>(level);
 return new JavaEntity<Zombie>(level);
}
void synchronizeJavaEntities(Level* level,const JavaGameSession& session) {
 if(!level || !level->isJavaServerLevel())return;
 for(Entity* entity:level->entities) {
  auto* presentation=dynamic_cast<JavaPresentation*>(entity);
  if(!presentation || entity->removed)continue;
  auto snapshot=session.entities.find(entity->entityId);
  if(snapshot==session.entities.end() || snapshot->second.kind!=presentation->serverKind)level->removeEntity(entity);
 }
 for(const auto& entry:session.entities) {
  auto snapshot=entry.second;auto skin=session.skinTextures.find(snapshot.profileSkinUrl);if(skin!=session.skinTextures.end())snapshot.skinTexture=skin->second;auto height=session.skinHeights.find(snapshot.profileSkinUrl);snapshot.skinModern=height!=session.skinHeights.end()&&height->second==64;
  if(snapshot.id==session.playerId)continue;
  if(!std::isfinite(snapshot.position[0])||!std::isfinite(snapshot.position[1])||!std::isfinite(snapshot.position[2]))continue;
  Entity* entity=level->getEntity(snapshot.id);
  // Let original Level retire old IDs before adding a replacement. Its deferred
  // removal erases lookup IDs, so immediate reuse would erase the new object.
  if(entity && entity->removed)continue;
  if(!entity) {
   entity=create(level,snapshot.kind);entity->entityId=snapshot.id;
   auto* presentation=dynamic_cast<JavaPresentation*>(entity);presentation->serverKind=snapshot.kind;presentation->apply(snapshot);
   entity->xo=entity->x;entity->yo=entity->y;entity->zo=entity->z;
   entity->yRotO=entity->yRot;entity->xRotO=entity->xRot;if(entity->isMob())static_cast<Mob*>(entity)->yBodyRotO=static_cast<Mob*>(entity)->yBodyRot;
   if(!level->addEntity(entity)){delete entity;continue;}
  } else if(auto* presentation=dynamic_cast<JavaPresentation*>(entity))presentation->apply(snapshot);
  if(auto* dropped=dynamic_cast<JavaDroppedItem*>(entity))dropped->compass=session.compass;
 }
}
