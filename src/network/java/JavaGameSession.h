#ifndef TCPE_JAVA_GAME_SESSION_H
#define TCPE_JAVA_GAME_SESSION_H
#include "../JavaServerConnection.h"
#include "JavaChunkStore.h"
#include "JavaLookCorrection.h"
#include "JavaTitleState.h"
#include "JavaBossState.h"
#include "JavaScoreboardState.h"
#include "JavaPackSounds.h"
#include <array>
#include <vector>
#include <map>
#include <set>
// Authoritative server state awaiting application to native TCPE objects.
class JavaGameSession {
public:
 struct Targeting {
  std::string kind="miss";uint64_t epoch=0,revision=0,inputRevision=0;unsigned world=0,age=0;
  std::array<double,3> eye{{0,0,0}},location{{0,0,0}};std::array<int,3> block{{0,0,0}};
  unsigned state=0;int face=0,entityId=0,attackEntityId=0;bool complete=false,hasAttack=false;
  std::vector<std::array<double,6>> boxes,edges;
 } targeting;
 uint64_t targetingEpoch=0,pickInputRevision=0;
 std::array<double,3> pickRayOffset{{0,0,0}},pickRayDirection{{0,0,1}};bool pickRayEnabled=false,pickRaySent=false;
 void clearTargeting(){targeting=Targeting{};pickRaySent=false;}
 bool updatePickRay(bool enabled,const std::array<double,3>& offset,const std::array<double,3>& direction);
 bool hasTargeting()const;
 bool targetingCurrent()const;
 bool hasAttackTargeting()const;
 struct FireworkExplosion {std::string shape;std::vector<uint32_t> colors,fadeColors;bool trail=false,twinkle=false;};
 struct VisualEffect {std::string kind;std::array<double,3> position{{0,0,0}},offset{{0,0,0}},velocity{{0,0,0}};unsigned count=1;std::vector<FireworkExplosion> explosions;};
 std::vector<VisualEffect> pendingVisualEffects;
 struct ServerSound {std::string name;std::array<double,3> position{{0,0,0}};double volume=1,pitch=1,fixedRange=-1;uint64_t seed=0;unsigned source=0;};
 std::vector<ServerSound> pendingSounds;
 struct PackFontTexture {int width=0,height=0;std::vector<unsigned char> rgba;};
 struct PackFontGlyph {std::string font;uint32_t codepoint=0;int texture=-1,x=0,y=0,width=0,height=0;float scale=1,ascent=0,advance=0;};
 struct PackModelQuad {int texture=-1,tint=-1;std::array<std::array<float,5>,4> vertices{};};
 struct PackModelTint {std::string kind="unsupported";uint32_t color=0xffffff,index=0,defaultColor=0xffffff;};
 struct PackDisplayTransform {std::array<float,3> rotation{{0,0,0}},translation{{0,0,0}},scale{{1,1,1}};};
 struct PackItemModel {std::string id;std::vector<PackModelQuad> quads;std::vector<PackModelTint> tints;std::map<std::string,PackDisplayTransform> display;std::array<float,3> rotation{{0,0,0}},translation{{0,0,0}},scale{{1,1,1}};};
 struct PackAnimationFrame {std::vector<unsigned char> rgba;uint32_t time=1;};
 struct PackAnimation {int texture=-1;std::vector<PackAnimationFrame> frames;bool interpolate=false;};
 struct PackFonts {uint64_t connection=0,generation=0;std::string id;std::vector<PackFontTexture> textures;std::vector<PackFontGlyph> glyphs;std::vector<PackItemModel> models;std::vector<std::string> modelErrors;std::vector<PackAnimation> animations;JavaPackSounds sounds;};
 std::vector<PackFonts> pendingPackFonts;std::string packFontError;
 bool requestPackFonts(size_t index);
 struct ResourcePack {std::string id,url,hash,prompt;bool required=false;};
 std::vector<ResourcePack> resourcePacks;uint64_t resourcePackConnection=0,resourcePackGeneration=0;unsigned resourcePackRevision=0;
 struct CustomModelData {std::vector<float> floats;std::vector<bool> flags;std::vector<std::string> strings;std::vector<int32_t> colors;bool present=false;};
 struct GlobalTarget {bool valid=false;std::string dimension;std::array<int,3> position{{0,0,0}};};
 struct CompassState {std::string dimension;GlobalTarget defaultSpawn,lastDeath;};
 struct ShieldLayer {std::string assetId;unsigned color=0;};
 struct Shield {bool patterned=false,complete=true;bool hasBaseColor=false;unsigned baseColor=0;std::vector<ShieldLayer> layers;};
 struct Item {Shield shield;bool hasProfile=false;std::string profileSkinUrl,profileSkinTexture;int profileSkinHeight=64;std::string kind,name,resourceId,itemModel,chargedProjectile;CustomModelData customModelData;std::vector<std::string> lore;int count=0,damage=0,maxDamage=0,originalId=-1,originalData=0;bool hasLodestoneTracker=false,lodestoneTracked=false;GlobalTarget lodestoneTarget;bool hasDyedColor=false;uint32_t dyedColor=0;bool hasPotionColor=false;uint32_t potionColor=0;};
 struct Recipe {uint32_t id=0;unsigned width=0,height=0;std::string kind,category;Item result;std::vector<Item> ingredients;bool placeable=false,previewComplete=false;};
 struct RecipeGhost {bool present=false;int windowId=-1;Recipe preview;};
 RecipeGhost recipeGhost;
 std::vector<Recipe> recipes;unsigned recipesRevision=0;
 bool placeRecipe(uint32_t id,bool all=false);
 struct Display {
  std::string text,textJson;
  std::array<double,3> translation{{0,0,0}},scale{{1,1,1}};
  std::array<double,4> leftRotation{{0,0,0,1}},rightRotation{{0,0,0,1}};
  unsigned billboard=0,textOpacity=255,styleFlags=0,blockState=0,itemTransform=0;
  int lineWidth=200,backgroundColor=0x40000000,brightness=-1;
  double viewRange=1,width=0,height=0;Item item;
 };
 struct ArmorStand {bool small=false,showArms=false,showBasePlate=true;std::array<std::array<float,3>,6> poses{{{{0,0,0}},{{0,0,0}},{{-10,0,-10}},{{-15,0,10}},{{-1,0,-1}},{{1,0,1}}}};};
 struct Entity {bool usingItem=false,usingOffhand=false;double useTicks=0,useDuration=0,useChargeDuration=0,useKineticDelay=0,useKineticDismount=0,useKineticKnockback=0,useKineticDamage=0;std::string useAnimation;int id;std::string kind;std::array<double,3> position;std::array<double,2> hitbox{{0,0}};double yaw,pitch,headYaw;bool sneaking;std::array<Item,6> equipment;Item droppedItem;bool fireworkShotAtAngle=false;int fireworkAttachedTo=-1;unsigned hurtTicks,deathTicks;unsigned mobAttackTicks=0,mobFlowerTicks=0;float livingHealth=0,livingMaxHealth=0;bool hasLivingHealth=false,hasLivingMaxHealth=false;std::string skinTexture,customName,customNameJson,profileName,profileUuid,profileTextures,profileTexturesSignature,profileSkinUrl,profileCapeUrl;unsigned skinLayers=0;bool skinModern=false,profileSkinSlim=false;bool customNameVisible=false,invisible=false,armorStandMarker=false;ArmorStand armorStand;Display display;};
 struct SlotRange {int start=-1,end=-1;bool valid()const{return start>=0&&end>start;}};
 CompassState compass;
 struct Inventory {uint64_t menuGeneration=0;std::map<int,int> properties;std::string menuKind;std::map<std::string,SlotRange> slotRanges;int windowId=0;std::string title;std::vector<Item> slots;std::array<Item,9> hotbar;std::array<Item,4> armor;Item held,carried,offHand;unsigned selected=0;};
 struct DialogAction {std::string label,id;};
 struct Dialog {std::string title,body;std::vector<DialogAction> actions;bool pending=false;unsigned revision=0;};
 struct ChatAction {std::string label,command;};
 std::vector<ChatAction> chatActions;std::string chatActionContext,lastChat,lastChatSignature,chatActionSignature;unsigned chatActionsRevision=0;bool chatActionsPending=false;
 bool chooseChatAction(size_t index);
 Dialog dialog;
 bool chooseDialogAction(size_t index);
 // Retains UTF8 and Java styling for the Java-only native font renderer.
 static std::string displayText(const std::string& text);
 JavaServerConnection connection;JavaChunkStore chunks;
 std::vector<JavaLookCorrection> lookCorrections;
 // Local snapshot position is eye height; Entity snapshots below use feet.
 std::array<double,3> position{{0,0,0}};
 struct Skin {std::string url;int width=0,height=0;std::vector<unsigned char> rgba;};
 std::map<std::string,Skin> cachedSkins;
 std::vector<Skin> pendingSkins;std::map<std::string,std::string> skinTextures;std::map<std::string,int> skinHeights;
 std::map<int,Entity> entities;
 bool useOffHand=false;
 bool itemUseActive=false,itemUseOffHand=false,attackPiercing=false;
 double itemUseTicks=0,itemUseDuration=0,itemUseChargeDuration=0,attackStrength=1,attackMinCharge=0;
 double itemUseKineticDelay=0,itemUseKineticDismount=0,itemUseKineticKnockback=0,itemUseKineticDamage=0;
 std::string itemUseAnimation;
 void clearItemUse(){itemUseActive=itemUseOffHand=attackPiercing=false;itemUseTicks=itemUseDuration=itemUseChargeDuration=0;itemUseAnimation.clear();itemUseKineticDelay=itemUseKineticDismount=itemUseKineticKnockback=itemUseKineticDamage=0;attackStrength=1;attackMinCharge=0;}
 double attackMinRange=0,attackMaxRange=3,attackHitboxMargin=.3;
 Inventory inventory;unsigned inventoryRevision=0,worldRevision=0;
 bool clickInventorySlot(size_t slot,bool right,bool quick=false);
 bool closeInventory();
 bool dropCarried(bool one);
 bool canFly=false,flying=false,hasAbilities=false,showDeathScreen=true;unsigned abilitiesRevision=0;
 double health=20,maxHealth=20,food=20,saturation=5,armor=0,absorption=0;
 struct HeartStatus {bool poisoned=false,withered=false,regenerating=false,hardcore=false;unsigned frozenTicks=0,invulnerableTicks=0;} hearts;
 JavaTitleState titleOverlay;
 JavaBossState bossOverlay;
 JavaScoreboardState scoreboard;
 std::string actionBar;int actionBarTicks=0;
 double experienceProgress=0,experienceLevel=0,totalExperience=0;
 int playerId=0;uint64_t dayTime=0;bool dayTicking=true,spawned=false,hasPosition=false,dead=false,errorShown=false;
 std::string phase,error,deathMessage;std::vector<std::string> messages;
 std::vector<std::string> messageSignatures,pendingChatDeletes;std::set<std::string> deletedChatSignatures;uint64_t chatScope=0;
 bool connect(const std::string& address,const std::string& identity,bool online);
 void disconnect();void tick();void refreshItemSkins();
private:
 void bindItemSkin(Item& item)const;
 Item item(const std::string& path)const;
 void event(const TcpeJavaEvent& event);
};
#endif
