#include "../../platform/input/Keyboard.h"
#include "JavaGameSession.h"
#include "JavaSkinPixels.h"
#include "JavaPackSoundDecode.h"
#include <limits>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
namespace {bool validChatSignature(const std::string& value){return value.size()==512&&std::all_of(value.begin(),value.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});}int integer(const JavaServerConnection& connection,const std::string& path,int fallback=0){double value;if(!connection.number(path,value)||!std::isfinite(value)||value<std::numeric_limits<int>::min()||value>std::numeric_limits<int>::max()||std::floor(value)!=value)return fallback;return static_cast<int>(value);}}
namespace {
bool packScope(JavaServerConnection& c,const std::string& path,uint64_t& out){double value=0;if(!c.number(path,value)||!std::isfinite(value)||value<0||value>=18446744073709551616.0||std::floor(value)!=value)return false;out=static_cast<uint64_t>(value);return true;}
bool packInt(const JavaServerConnection& c,const std::string& path,int& out){double value=0;if(!c.number(path,value)||!std::isfinite(value)||value<std::numeric_limits<int>::min()||value>std::numeric_limits<int>::max()||std::floor(value)!=value)return false;out=static_cast<int>(value);return true;}
bool readFireworkEffect(const JavaServerConnection& c,JavaGameSession::VisualEffect& effect){
 const std::string path="visual_effect.firework";for(size_t i=0;i<3;++i)if(!c.number(path+".velocity."+std::to_string(i),effect.velocity[i])||!std::isfinite(effect.velocity[i]))return false;
 size_t count=0;if(!c.arraySize(path+".explosions",count)||count>256||(effect.kind=="firework_explosion"&&count==0)||(effect.kind=="firework_spark"&&count!=0))return false;
 for(size_t i=0;i<count;++i){JavaGameSession::FireworkExplosion e;const std::string base=path+".explosions."+std::to_string(i);if(!c.text(base+".shape",e.shape)||(e.shape!="small_ball"&&e.shape!="large_ball"&&e.shape!="star"&&e.shape!="creeper"&&e.shape!="burst"))return false;c.boolean(base+".has_trail",e.trail);c.boolean(base+".has_twinkle",e.twinkle);
  for(int fade=0;fade<2;++fade){const std::string colors=base+(fade?".fade_colors":".colors");size_t length=0;if(!c.arraySize(colors,length))continue;if(length>256)return false;auto& output=fade?e.fadeColors:e.colors;for(size_t j=0;j<length;++j){double value;if(!c.number(colors+"."+std::to_string(j),value)||!std::isfinite(value)||value<-2147483648.0||value>2147483647.0||std::floor(value)!=value)return false;output.push_back(uint32_t(int64_t(value))&0xffffff);}}
  effect.explosions.push_back(std::move(e));
 }return true;
}
JavaGameSession::GlobalTarget readGlobalTarget(const JavaServerConnection& c,const std::string& path){JavaGameSession::GlobalTarget out;if(!c.text(path+".dimension",out.dimension)||out.dimension.empty()||out.dimension.size()>1024)return {};for(int axis=0;axis<3;++axis)if(!packInt(c,path+".position."+std::to_string(axis),out.position[axis]))return {};out.valid=true;return out;}
bool packFontName(const std::string& name){size_t colon=name.find(':');if(colon==std::string::npos||colon==0||colon+1>=name.size()||name.size()>1024)return false;for(size_t i=0;i<name.size();++i){char c=name[i];if(i==colon)continue;if((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'||(i>colon&&c=='/'))continue;return false;}return true;}
std::string componentText(JavaServerConnection& connection,const std::string& path,unsigned depth=0){
 if(depth>12)return "";std::string text;if(connection.text(path,text))return text.substr(0,16384);
 size_t count=0;if(connection.arraySize(path,count)){for(size_t i=0;i<count&&i<256;i++){text+=componentText(connection,path+"."+std::to_string(i),depth+1);if(text.size()>16384){text.resize(16384);break;}}return text;}
 connection.text(path+".text",text);
 if(connection.arraySize(path+".extra",count))for(size_t i=0;i<count&&i<256;i++){text+=componentText(connection,path+".extra."+std::to_string(i),depth+1);if(text.size()>16384){text.resize(16384);break;}}
 return text;
}
std::string quoteDialogId(const std::string& text){std::string out="\"";for(unsigned char c:text){if(c=='"'||c=='\\')out+='\\';if(c<32){const char* hex="0123456789abcdef";out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}return out+"\"";}
}
bool JavaGameSession::updatePickRay(bool enabled,const std::array<double,3>& offset,const std::array<double,3>& direction){
 if(!spawned||!targetingEpoch)return false;
 if(pickRaySent&&enabled==pickRayEnabled&&offset==pickRayOffset&&direction==pickRayDirection)return true;
 double norm=0,dist=0;for(int i=0;i<3;++i){if(!std::isfinite(offset[i])||!std::isfinite(direction[i]))return false;norm+=direction[i]*direction[i];dist+=offset[i]*offset[i];}
 if(norm<.998001||norm>1.002001||dist>256)return false;
 if(pickInputRevision>=9007199254740991ULL)return false;
 ++pickInputRevision;pickRayEnabled=enabled;pickRayOffset=offset;pickRayDirection=direction;
 std::string command="{\"type\":\"pick_ray\",\"epoch\":"+std::to_string(targetingEpoch)+",\"revision\":"+std::to_string(pickInputRevision)+",\"enabled\":"+(enabled?"true":"false")+",\"offset\":[";
 for(int i=0;i<3;++i){if(i)command+=",";command+=std::to_string(offset[i]);}command+="],\"direction\":[";
 for(int i=0;i<3;++i){if(i)command+=",";command+=std::to_string(direction[i]);}command+="]}";
 pickRaySent=connection.send(command);return pickRaySent;
}
bool JavaGameSession::targetingCurrent()const{
 return spawned&&!dead&&pickRayEnabled&&pickRaySent&&targeting.epoch==targetingEpoch&&targeting.world==worldRevision&&targeting.age<=5&&targeting.inputRevision==pickInputRevision;
}
bool JavaGameSession::hasAttackTargeting()const{return targetingCurrent()&&targeting.hasAttack&&entities.count(targeting.attackEntityId)!=0;}
bool JavaGameSession::hasTargeting()const{
 if(!targetingCurrent())return false;
 if(targeting.kind=="block"){uint16_t state=0;return chunks.state(targeting.block[0],targeting.block[1],targeting.block[2],state)&&state==targeting.state;}
 return targeting.kind=="entity"&&entities.count(targeting.entityId)!=0;
}
std::string JavaGameSession::displayText(const std::string& text){return text;}
bool JavaGameSession::chooseDialogAction(size_t index){if(!dialog.pending||index>=dialog.actions.size())return false;bool sent=connection.send("{\"type\":\"dialog_click\",\"action\":"+quoteDialogId(dialog.actions[index].id)+"}");if(sent)dialog.pending=false;return sent;}
bool JavaGameSession::chooseChatAction(size_t index){if(!chatActionsPending||index>=chatActions.size())return false;const std::string& command=chatActions[index].command;if(command.empty()||command[0]!='/'||command.size()>32767)return false;const bool sent=connection.chat(command);if(sent)chatActionsPending=false;return sent;}
bool JavaGameSession::clickInventorySlot(size_t slot,bool right,bool quick){if(slot>=inventory.slots.size()||slot>65535)return false;return connection.send(std::string("{\"type\":\"")+(quick||Keyboard::isKeyDown(Keyboard::KEY_LSHIFT)?"quick_move":"click_slot")+"\",\"window_id\":"+std::to_string(inventory.windowId)+",\"menu_generation\":"+std::to_string(inventory.menuGeneration)+",\"slot\":"+std::to_string(slot)+",\"right\":"+(right?"true":"false")+"}");}
bool JavaGameSession::placeRecipe(uint32_t id,bool all){if(inventory.menuKind!="player"&&inventory.menuKind!="crafting")return false;const unsigned grid=inventory.menuKind=="player"?2:3;for(const auto& recipe:recipes)if(recipe.id==id&&recipe.placeable&&recipe.width<=grid&&recipe.height<=grid){return connection.send("{\"type\":\"place_recipe\",\"window_id\":"+std::to_string(inventory.windowId)+",\"menu_generation\":"+std::to_string(inventory.menuGeneration)+",\"recipe_id\":"+std::to_string(id)+",\"all\":"+(all?"true":"false")+"}");}return false;}
bool JavaGameSession::dropCarried(bool one){if(inventory.carried.count<=0)return false;return connection.send("{\"type\":\"drop_carried\",\"window_id\":"+std::to_string(inventory.windowId)+",\"menu_generation\":"+std::to_string(inventory.menuGeneration)+",\"right\":"+(one?"true":"false")+"}");}
bool JavaGameSession::closeInventory(){recipeGhost=RecipeGhost{};return connection.send("{\"type\":\"close_container\",\"window_id\":"+std::to_string(inventory.windowId)+",\"menu_generation\":"+std::to_string(inventory.menuGeneration)+"}");}
bool JavaGameSession::connect(const std::string& address,const std::string& identity,bool online){disconnect();static uint64_t nextChatScope=0;chatScope=++nextChatScope;error.clear();phase="Connecting";return connection.connect(address,identity,online);}
void JavaGameSession::disconnect(){hearts=HeartStatus{};clearTargeting();targetingEpoch=pickInputRevision=0;clearItemUse();recipeGhost=RecipeGhost{};recipes.clear();++recipesRevision;pendingPackFonts.clear();packFontError.clear();resourcePacks.clear();resourcePackConnection=resourcePackGeneration=0;++resourcePackRevision;scoreboard.clear();bossOverlay.clear();titleOverlay.clear(true);actionBar.clear();actionBarTicks=0;showDeathScreen=true;canFly=flying=hasAbilities=false;++abilitiesRevision;lookCorrections.clear();pendingVisualEffects.clear();pendingSounds.clear();pendingSkins.clear();cachedSkins.clear();skinTextures.clear();skinHeights.clear();connection.disconnect();phase="Disconnected";worldRevision=0;dialog=Dialog{};chatActions.clear();chatActionContext.clear();lastChat.clear();lastChatSignature.clear();chatActionSignature.clear();chatActionsPending=false;chatActionsRevision=0;chunks.clear();entities.clear();inventory=Inventory{};useOffHand=false;attackMinRange=0;attackMaxRange=3;attackHitboxMargin=.3;compass=CompassState{};spawned=false;hasPosition=false;dead=false;errorShown=false;deathMessage.clear();messages.clear();messageSignatures.clear();pendingChatDeletes.clear();deletedChatSignatures.clear();playerId=0;health=maxHealth=food=20;saturation=5;armor=0;absorption=0;experienceProgress=experienceLevel=totalExperience=0;}
void JavaGameSession::tick(){if(targeting.age<100000)++targeting.age;titleOverlay.tick();if(actionBarTicks>0)--actionBarTicks;connection.pump([&](const TcpeJavaEvent& update){event(update);});if(!connection.connected()&&!phase.empty()&&phase!="Disconnected"&&phase!="Error"&&error.empty()){error="Java connection closed";phase="Error";spawned=false;clearTargeting();clearItemUse();pendingVisualEffects.clear();pendingSounds.clear();}}
void JavaGameSession::bindItemSkin(Item& item)const{item.profileSkinTexture.clear();item.profileSkinHeight=64;auto texture=skinTextures.find(item.profileSkinUrl);auto height=skinHeights.find(item.profileSkinUrl);if(item.hasProfile&&texture!=skinTextures.end()&&height!=skinHeights.end()&&(height->second==32||height->second==64)){item.profileSkinTexture=texture->second;item.profileSkinHeight=height->second;}}
void JavaGameSession::refreshItemSkins(){for(auto& item:inventory.slots)bindItemSkin(item);for(auto& item:inventory.hotbar)bindItemSkin(item);for(auto& item:inventory.armor)bindItemSkin(item);bindItemSkin(inventory.held);bindItemSkin(inventory.carried);bindItemSkin(inventory.offHand);for(auto& pair:entities){for(auto& item:pair.second.equipment)bindItemSkin(item);bindItemSkin(pair.second.droppedItem);bindItemSkin(pair.second.display.item);}}
JavaGameSession::Item JavaGameSession::item(const std::string& path)const{
 Item out;connection.boolean(path+".profile.present",out.hasProfile);if(out.hasProfile){std::string url;if(connection.text(path+".profile.skin_url",url)){const std::string prefix="https://textures.minecraft.net/texture/";if(url.size()==prefix.size()+64&&url.compare(0,prefix.size(),prefix)==0&&std::all_of(url.begin()+prefix.size(),url.end(),[](unsigned char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');}))out.profileSkinUrl=url;}bindItemSkin(out);}connection.text(path+".kind",out.kind);connection.text(path+".resource_id",out.resourceId);connection.text(path+".charged_projectile",out.chargedProjectile);if(out.chargedProjectile!="arrow"&&out.chargedProjectile!="rocket")out.chargedProjectile.clear();connection.text(path+".name",out.name);out.name=displayText(out.name);double value;
 size_t loreCount=0;if(connection.arraySize(path+".lore",loreCount))for(size_t i=0;i<std::min(size_t(32),loreCount);++i){std::string line;if(connection.text(path+".lore."+std::to_string(i),line))out.lore.push_back(displayText(line));}
 if(connection.number(path+".count",value))out.count=static_cast<int>(value);
 out.damage=std::max(0,integer(connection,path+".damage"));out.maxDamage=std::max(0,integer(connection,path+".max_damage"));
 if(connection.number(path+".original_id",value))out.originalId=static_cast<int>(value);
 if(connection.number(path+".original_data",value))out.originalData=static_cast<int>(value);
 // Item model selectors and typed custom model data remain independent of the
 // fallback resource id. Reject a malformed component as a whole.
 std::string model;if(connection.text(path+".item_model",model)&&packFontName(model))out.itemModel=model;
 CustomModelData custom;bool valid=true,any=false;size_t stringBytes=0;
 for(const char* field:{"floats","flags","strings","colors"}){
  const std::string key=path+".custom_model_data."+field;size_t count=0;
  if(!connection.arraySize(key,count)){valid=false;break;}any=true;if(count>1024){valid=false;break;}
  for(size_t i=0;i<count&&valid;++i){const std::string at=key+"."+std::to_string(i);
   if(std::string(field)=="floats"){double value=0;if(!connection.number(at,value)||!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max()){valid=false;break;}const float parsed=static_cast<float>(value);if(!std::isfinite(parsed)){valid=false;break;}custom.floats.push_back(parsed);}
   else if(std::string(field)=="flags"){bool value=false;if(!connection.boolean(at,value)){valid=false;break;}custom.flags.push_back(value);}
   else if(std::string(field)=="strings"){std::string value;if(!connection.text(at,value)||value.size()>2048||stringBytes+value.size()>65536){valid=false;break;}stringBytes+=value.size();custom.strings.push_back(std::move(value));}
   else {int value=0;if(!packInt(connection,at,value)){valid=false;break;}custom.colors.push_back(static_cast<int32_t>(value));}
  }
 }
 if(valid&&any){custom.present=true;out.customModelData=std::move(custom);}
 if(connection.boolean(path+".lodestone_tracker.tracked",out.lodestoneTracked)){out.hasLodestoneTracker=true;out.lodestoneTarget=readGlobalTarget(connection,path+".lodestone_tracker.target");}
 int color=0;if(packInt(connection,path+".dyed_color",color)){out.hasDyedColor=true;out.dyedColor=uint32_t(color)&0xffffff;}
 if(packInt(connection,path+".potion_color",color)){out.hasPotionColor=true;out.potionColor=uint32_t(color)&0xffffff;}
 connection.boolean(path+".shield.patterned",out.shield.patterned);connection.boolean(path+".shield.complete",out.shield.complete);
 if(packInt(connection,path+".shield.base_color",color)&&color>=0&&color<16){out.shield.hasBaseColor=true;out.shield.baseColor=unsigned(color);}
 size_t layerCount=0;if(connection.arraySize(path+".shield.layers",layerCount)){
  if(layerCount>16){out.shield.complete=false;layerCount=16;}
  for(size_t i=0;i<layerCount;++i){const std::string prefix=path+".shield.layers."+std::to_string(i);ShieldLayer layer;int dye=-1;
   if(!connection.text(prefix+".asset_id",layer.assetId)||layer.assetId.empty()||layer.assetId.size()>256||!packInt(connection,prefix+".color",dye)||dye<0||dye>=16){out.shield.complete=false;continue;}
   const auto colon=layer.assetId.find(':');bool valid=colon!=std::string::npos&&colon>0&&colon+1<layer.assetId.size()&&layer.assetId.find("..") == std::string::npos;
   for(char c:layer.assetId)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'||c=='/'||c==':'))valid=false;
   if(!valid){out.shield.complete=false;continue;}layer.color=unsigned(dye);out.shield.layers.push_back(std::move(layer));
  }
 }
 return out;
}
void JavaGameSession::event(const TcpeJavaEvent& update){
 if(update.kind==TCPE_JAVA_LOGIN||update.kind==TCPE_JAVA_CONFIGURATION||update.kind==TCPE_JAVA_DISCONNECTED)hearts=HeartStatus{};
 if(update.kind==TCPE_JAVA_VITALS){
  hearts=HeartStatus{};
  connection.boolean("vitals.poisoned",hearts.poisoned);connection.boolean("vitals.withered",hearts.withered);
  connection.boolean("vitals.regenerating",hearts.regenerating);connection.boolean("vitals.hardcore",hearts.hardcore);
  double frozen=0,invulnerable=0;
  if(connection.number("vitals.frozen_ticks",frozen)&&std::isfinite(frozen)&&frozen>=0&&frozen<=2147483647&&std::floor(frozen)==frozen)hearts.frozenTicks=unsigned(frozen);
  if(connection.number("vitals.invulnerable_ticks",invulnerable)&&std::isfinite(invulnerable)&&invulnerable>=0&&invulnerable<=20&&std::floor(invulnerable)==invulnerable)hearts.invulnerableTicks=unsigned(invulnerable);
 }
 switch(update.kind){
 case TCPE_JAVA_PACK_FONTS:{
  PackFonts fonts;
  if(!packScope(connection,"pack_fonts.connection",fonts.connection)||!packScope(connection,"pack_fonts.generation",fonts.generation)){packFontError="Invalid pack font scope";break;}
  connection.text("pack_fonts.id",fonts.id);bool known=false;for(const auto& pack:resourcePacks)if(pack.id==fonts.id)known=true;
  if(!known||fonts.connection!=resourcePackConnection||fonts.generation!=resourcePackGeneration)break;
  size_t count=0,total=0;bool valid=true;
  if(connection.arraySize("pack_fonts.textures",count)&&count<=512){for(size_t i=0;i<count;++i){
   const std::string path="pack_fonts.textures."+std::to_string(i);PackFontTexture texture;std::string encoded;
   if(!packInt(connection,path+".width",texture.width)||!packInt(connection,path+".height",texture.height)||texture.width<=0||texture.height<=0||texture.width>8192||texture.height>8192||!connection.text(path+".rgba_base64",encoded)||!decodeJavaRgbaPixels(encoded,texture.width,texture.height,texture.rgba)){valid=false;break;}
   total+=texture.rgba.size();if(total>256*1024*1024){valid=false;break;}fonts.textures.push_back(std::move(texture));
  }}else valid=false;
  if(valid&&connection.arraySize("pack_fonts.fonts",count)&&count<=512){for(size_t i=0;i<count&&valid;++i){
   const std::string path="pack_fonts.fonts."+std::to_string(i);std::string name;size_t glyphs=0;
   if(!connection.text(path+".name",name)||!packFontName(name)||!connection.arraySize(path+".glyphs",glyphs)||glyphs>1048576-fonts.glyphs.size()){valid=false;break;}
   for(size_t j=0;j<glyphs&&valid;++j){
    const std::string key=path+".glyphs."+std::to_string(j);PackFontGlyph glyph;glyph.font=name;int scalar=0;
    if(!packInt(connection,key+".codepoint",scalar)||scalar<0||scalar>0x10ffff||(scalar>=0xd800&&scalar<=0xdfff)||!packInt(connection,key+".texture",glyph.texture)||!packInt(connection,key+".x",glyph.x)||!packInt(connection,key+".y",glyph.y)||!packInt(connection,key+".width",glyph.width)||!packInt(connection,key+".height",glyph.height)){valid=false;break;}
    glyph.codepoint=uint32_t(scalar);
    for(auto field:{std::make_pair("scale",&glyph.scale),std::make_pair("ascent",&glyph.ascent),std::make_pair("advance",&glyph.advance)}){double number=0;if(!connection.number(key+"."+field.first,number)||!std::isfinite(number)||std::abs(number)>65536.){valid=false;break;}*field.second=float(number);}
    if(!valid)break;
    if(glyph.texture<-1||glyph.texture>=int(fonts.textures.size())||glyph.scale<=0||glyph.scale>8192||std::abs(glyph.ascent)>8192){valid=false;break;}
    if(glyph.texture==-1){if(glyph.x!=0||glyph.y!=0||glyph.width!=0||glyph.height!=0||glyph.scale!=1||glyph.ascent!=0){valid=false;break;}}
    else{const auto& texture=fonts.textures[glyph.texture];if(glyph.x<0||glyph.y<0||glyph.width<=0||glyph.height<=0||glyph.width>texture.width||glyph.height>texture.height||glyph.x>texture.width-glyph.width||glyph.y>texture.height-glyph.height||double(glyph.height)*glyph.scale>8192||double(glyph.width)*glyph.scale>65536){valid=false;break;}}
    fonts.glyphs.push_back(glyph);
   }
  }}else valid=false;
  size_t modelCount=0,totalQuads=0;
  if(valid&&connection.arraySize("pack_fonts.models",modelCount)){
   if(modelCount>4096)valid=false;
   for(size_t m=0;m<modelCount&&valid;++m){PackItemModel model;const std::string path="pack_fonts.models."+std::to_string(m);size_t quadCount=0;
    if(!connection.text(path+".id",model.id)||!packFontName(model.id)||!connection.arraySize(path+".quads",quadCount)||quadCount>65536||totalQuads+quadCount>262144){valid=false;break;}totalQuads+=quadCount;
    for(auto field:{std::make_pair("rotation",&model.rotation),std::make_pair("translation",&model.translation),std::make_pair("scale",&model.scale)}){size_t axes=0;if(!connection.arraySize(path+".gui."+field.first,axes)||axes!=3){valid=false;break;}for(size_t axis=0;axis<3;++axis){double value=0;const double limit=std::string(field.first)=="rotation"?36000.:80.;if(!connection.number(path+".gui."+field.first+"."+std::to_string(axis),value)||!std::isfinite(value)||std::abs(value)>limit){valid=false;break;}(*field.second)[axis]=float(value);}}
    size_t displayCount=0;
    if(connection.arraySize(path+".display",displayCount)){
     if(displayCount>9)valid=false;
     for(size_t d=0;d<displayCount&&valid;++d){const std::string key=path+".display."+std::to_string(d);std::string context;PackDisplayTransform transform;
      if(!connection.text(key+".context",context)||(context!="thirdperson_righthand"&&context!="thirdperson_lefthand"&&context!="firstperson_righthand"&&context!="firstperson_lefthand"&&context!="head"&&context!="gui"&&context!="ground"&&context!="fixed"&&context!="on_shelf")||model.display.count(context)){valid=false;break;}
      for(auto field:{std::make_pair("rotation",&transform.rotation),std::make_pair("translation",&transform.translation),std::make_pair("scale",&transform.scale)}){size_t axes=0;const std::string vector=key+".transform."+field.first;if(!connection.arraySize(vector,axes)||axes!=3){valid=false;break;}for(size_t axis=0;axis<3;++axis){double value=0;const double limit=std::string(field.first)=="rotation"?36000.:(std::string(field.first)=="scale"?4.:80.);if(!connection.number(vector+"."+std::to_string(axis),value)||!std::isfinite(value)||std::abs(value)>limit){valid=false;break;}(*field.second)[axis]=float(value);}}
      if(valid)model.display.emplace(context,transform);
     }
    }
    size_t tintCount=0;if(connection.arraySize(path+".tints",tintCount)){if(tintCount>1024)valid=false;for(size_t i=0;i<tintCount&&valid;++i){PackModelTint tint;const std::string key=path+".tints."+std::to_string(i);int color=0,index=0,fallback=0;if(!connection.text(key+".kind",tint.kind)||(tint.kind!="constant"&&tint.kind!="custom_model_data"&&tint.kind!="unsupported"&&tint.kind!="dye"&&tint.kind!="potion")||!packInt(connection,key+".color",color)||color<0||color>0xffffff||!packInt(connection,key+".index",index)||index<0||index>1023||!packInt(connection,key+".default_color",fallback)||fallback<0||fallback>0xffffff){valid=false;break;}tint.color=uint32_t(color);tint.index=unsigned(index);tint.defaultColor=uint32_t(fallback);model.tints.push_back(std::move(tint));}}
    for(size_t q=0;q<quadCount&&valid;++q){PackModelQuad quad;const std::string key=path+".quads."+std::to_string(q);size_t vertices=0;
     if(!packInt(connection,key+".texture",quad.texture)||quad.texture<0||quad.texture>=int(fonts.textures.size())||!packInt(connection,key+".tint",quad.tint)||quad.tint<-1||quad.tint>1023||!connection.arraySize(key+".vertices",vertices)||vertices!=4){valid=false;break;}
     for(size_t v=0;v<4&&valid;++v){size_t fields=0;const std::string vertex=key+".vertices."+std::to_string(v);if(!connection.arraySize(vertex,fields)||fields!=5){valid=false;break;}for(size_t k=0;k<5;++k){double value=0;if(!connection.number(vertex+"."+std::to_string(k),value)||!std::isfinite(value)||std::abs(value)>(k<3?1024.:16.)){valid=false;break;}quad.vertices[v][k]=float(value);}}
     if(valid)model.quads.push_back(quad);
    }
    if(valid)fonts.models.push_back(std::move(model));
   }
  }
  size_t animationCount=0,animationBytes=0;
  if(valid&&connection.arraySize("pack_fonts.animations",animationCount)){
   std::vector<bool> pages(fonts.textures.size(),false);if(animationCount>512)valid=false;
   for(size_t i=0;i<animationCount&&valid;++i){PackAnimation animation;const std::string path="pack_fonts.animations."+std::to_string(i);size_t frames=0;
    if(!packInt(connection,path+".texture",animation.texture)||animation.texture<0||animation.texture>=int(fonts.textures.size())||pages[animation.texture]||!connection.boolean(path+".interpolate",animation.interpolate)||!connection.arraySize(path+".frames",frames)||frames==0||frames>1024){valid=false;break;}
    pages[animation.texture]=true;const auto& page=fonts.textures[animation.texture];
    for(size_t frame=0;frame<frames;++frame){const std::string key=path+".frames."+std::to_string(frame);PackAnimationFrame source;int duration=0;std::string encoded;
     if(animationBytes+page.rgba.size()>128*1024*1024||total+page.rgba.size()>256*1024*1024||!packInt(connection,key+".time",duration)||duration<=0||duration>1000000||!connection.text(key+".rgba_base64",encoded)||!decodeJavaRgbaPixels(encoded,page.width,page.height,source.rgba)){valid=false;break;}
     source.time=uint32_t(duration);animationBytes+=source.rgba.size();total+=source.rgba.size();animation.frames.push_back(std::move(source));
    }
    if(valid)fonts.animations.push_back(std::move(animation));
   }
  }
  size_t modelErrors=0,errorBytes=0;if(valid&&connection.arraySize("pack_fonts.model_errors",modelErrors)){if(modelErrors>256)valid=false;for(size_t i=0;i<modelErrors&&valid;++i){std::string message;if(!connection.text("pack_fonts.model_errors."+std::to_string(i),message)||message.size()>2048||errorBytes+message.size()>65536){valid=false;break;}errorBytes+=message.size();fonts.modelErrors.push_back(std::move(message));}}
  if(valid&&!readJavaPackSounds(connection,"pack_fonts.sounds",fonts.sounds,total))valid=false;
  size_t pendingBytes=total;for(const auto& pending:pendingPackFonts){for(const auto& sample:pending.sounds.samples)pendingBytes+=sample.pcm.size();for(const auto& texture:pending.textures)pendingBytes+=texture.rgba.size();for(const auto& animation:pending.animations)for(const auto& frame:animation.frames)pendingBytes+=frame.rgba.size();}if(pendingBytes>256*1024*1024)valid=false;
  if(valid&&pendingPackFonts.size()<128)pendingPackFonts.push_back(std::move(fonts));else packFontError="Invalid pack font snapshot";break;
 }
 case TCPE_JAVA_PACK_FONT_ERROR:{uint64_t owner=0,version=0;if(packScope(connection,"pack_font_error.connection",owner)&&packScope(connection,"pack_font_error.generation",version)&&owner==resourcePackConnection&&version==resourcePackGeneration)connection.text("pack_font_error.message",packFontError);break;}
 case TCPE_JAVA_RESOURCE_PACKS:{
  pendingPackFonts.clear();packFontError.clear();resourcePacks.clear();resourcePackConnection=resourcePackGeneration=0;++resourcePackRevision;
  if(!packScope(connection,"resource_packs.connection",resourcePackConnection)||!packScope(connection,"resource_packs.generation",resourcePackGeneration)){resourcePackConnection=resourcePackGeneration=0;packFontError="Invalid resource pack scope";break;}
  size_t count=0;if(connection.arraySize("resource_packs.requests",count))for(size_t i=0;i<std::min(size_t(128),count);++i){ResourcePack pack;const std::string path="resource_packs.requests."+std::to_string(i);connection.text(path+".id",pack.id);connection.text(path+".url",pack.url);connection.text(path+".hash",pack.hash);connection.text(path+".prompt",pack.prompt);connection.boolean(path+".required",pack.required);resourcePacks.push_back(pack);}break;
 }
 case TCPE_JAVA_SOUND:{ServerSound sound;std::string kind="play";connection.text("sound.kind",kind);bool valid=connection.text("sound.name",sound.name)&&sound.name.size()<=256;double source=0;valid=valid&&connection.number("sound.source",source)&&std::isfinite(source)&&std::floor(source)==source&&((source>=0&&source<=9)||(kind=="stop"&&source==255));sound.source=unsigned(source);if(kind=="stop"){valid=valid&&(sound.name.empty()||javaSoundName(sound.name));sound.volume=-1;}else if(kind=="play"){valid=valid&&javaSoundName(sound.name);for(size_t i=0;i<3&&valid;++i)valid=connection.number("sound.position."+std::to_string(i),sound.position[i])&&std::isfinite(sound.position[i])&&std::abs(sound.position[i])<=30000000;valid=valid&&connection.number("sound.volume",sound.volume)&&std::isfinite(sound.volume)&&sound.volume>=0&&connection.number("sound.pitch",sound.pitch)&&std::isfinite(sound.pitch)&&sound.pitch>0;double range=-1;if(connection.number("sound.fixed_range",range))valid=valid&&std::isfinite(range)&&range>=0;sound.fixedRange=range;std::string seed;if(!connection.text("sound.seed_text",seed))valid=false;else try{size_t parsed=0;sound.seed=std::stoull(seed,&parsed);valid=valid&&parsed==seed.size();}catch(...){valid=false;}}else valid=false;if(valid&&pendingSounds.size()<256)pendingSounds.push_back(std::move(sound));break;}
 case TCPE_JAVA_VISUAL_EFFECT:{VisualEffect effect;bool valid=connection.text("visual_effect.kind",effect.kind)&&(effect.kind=="gust"||effect.kind=="small_gust"||effect.kind=="gust_emitter_large"||effect.kind=="gust_emitter_small"||effect.kind=="firework_spark"||effect.kind=="firework_explosion");for(size_t i=0;i<3&&valid;++i)valid=connection.number("visual_effect.position."+std::to_string(i),effect.position[i])&&std::isfinite(effect.position[i])&&std::abs(effect.position[i])<=30000000;double count=1;if(connection.number("visual_effect.count",count)){valid=valid&&std::isfinite(count)&&count>=1&&count<=256&&std::floor(count)==count;effect.count=valid?unsigned(count):1;}for(size_t i=0;i<3&&valid;++i){double offset=0;if(connection.number("visual_effect.offset."+std::to_string(i),offset)){valid=std::isfinite(offset)&&std::abs(offset)<=1024;effect.offset[i]=offset;}}if(valid&&(effect.kind=="firework_spark"||effect.kind=="firework_explosion"))valid=readFireworkEffect(connection,effect);if(valid&&pendingVisualEffects.size()<256)pendingVisualEffects.push_back(std::move(effect));break;}
 case TCPE_JAVA_LOOK_CORRECTION:{clearTargeting();JavaLookCorrection look;if(connection.number("look.yaw",look.yaw)&&connection.number("look.pitch",look.pitch)&&connection.boolean("look.relative_yaw",look.relativeYaw)&&connection.boolean("look.relative_pitch",look.relativePitch)&&std::isfinite(look.yaw)&&std::isfinite(look.pitch))lookCorrections.push_back(look);break;}
 case TCPE_JAVA_SKIN:{Skin skin;std::string encoded;skin.width=integer(connection,"skin.width");skin.height=integer(connection,"skin.height");if(pendingSkins.size()<128&&connection.text("skin.url",skin.url)&&connection.text("skin.rgba_base64",encoded)&&decodeJavaSkinPixels(encoded,skin.width,skin.height,skin.rgba)){cachedSkins[skin.url]=skin;pendingSkins.push_back(std::move(skin));}break;}
 case TCPE_JAVA_RECIPES:{
  recipes.clear();recipeGhost=RecipeGhost{};++recipesRevision;size_t count=0;if(!connection.arraySize("recipes.entries",count)||count>4096)break;
  for(size_t i=0;i<count;++i){Recipe recipe;const std::string path="recipes.entries."+std::to_string(i);double id=0,w=0,h=0;size_t ingredients=0;
   if(!connection.number(path+".id",id)||!std::isfinite(id)||id<0||id>2147483647||id!=std::floor(id)||!connection.number(path+".width",w)||!connection.number(path+".height",h)||!std::isfinite(w)||!std::isfinite(h)||w<1||w>3||h<1||h>3||w!=std::floor(w)||h!=std::floor(h)||!connection.text(path+".kind",recipe.kind)||(recipe.kind!="shaped"&&recipe.kind!="shapeless")||!connection.arraySize(path+".ingredients",ingredients)||ingredients>9)continue;
   connection.text(path+".category",recipe.category);if(recipe.category.size()>64)recipe.category.clear();recipe.id=uint32_t(id);recipe.width=unsigned(w);recipe.height=unsigned(h);recipe.result=item(path+".result");if(recipe.result.count<=0||recipe.result.resourceId.empty())continue;
   connection.boolean(path+".placeable",recipe.placeable);connection.boolean(path+".preview_complete",recipe.previewComplete);for(size_t j=0;j<ingredients;++j)recipe.ingredients.push_back(item(path+".ingredients."+std::to_string(j)));
   bool duplicate=false;for(const auto& old:recipes)if(old.id==recipe.id){duplicate=true;break;}if(!duplicate)recipes.push_back(std::move(recipe));
  }
  int ghostWindow=-1;double gw=0,gh=0;size_t ghostIngredients=0;std::string ghostKind;
  if(packInt(connection,"recipes.ghost.container_id",ghostWindow)&&ghostWindow==inventory.windowId&&(inventory.menuKind=="player"||inventory.menuKind=="crafting")&&connection.number("recipes.ghost.width",gw)&&connection.number("recipes.ghost.height",gh)&&std::isfinite(gw)&&std::isfinite(gh)&&gw>=1&&gh>=1&&gw<=3&&gh<=3&&gw==std::floor(gw)&&gh==std::floor(gh)&&connection.text("recipes.ghost.kind",ghostKind)&&(ghostKind=="shaped"||ghostKind=="shapeless")&&connection.arraySize("recipes.ghost.ingredients",ghostIngredients)&&ghostIngredients<=9){
   const unsigned grid=inventory.menuKind=="player"?2:3;
   Recipe preview;preview.width=unsigned(gw);preview.height=unsigned(gh);preview.kind=ghostKind;preview.result=item("recipes.ghost.result");connection.boolean("recipes.ghost.preview_complete",preview.previewComplete);
   for(size_t i=0;i<ghostIngredients;++i)preview.ingredients.push_back(item("recipes.ghost.ingredients."+std::to_string(i)));
   if(preview.width<=grid&&preview.height<=grid&&preview.result.count>0&&!preview.result.resourceId.empty()){recipeGhost.present=true;recipeGhost.windowId=ghostWindow;recipeGhost.preview=std::move(preview);}
  }break;
 }
 case TCPE_JAVA_COMPASS:{compass=CompassState{};connection.text("compass.dimension",compass.dimension);if(compass.dimension.size()>1024)compass.dimension.clear();compass.defaultSpawn=readGlobalTarget(connection,"compass.default_spawn");compass.lastDeath=readGlobalTarget(connection,"compass.last_death");break;}
 case TCPE_JAVA_TARGETING:{
  Targeting next;double epoch=0,revision=0,input=0;
  auto counter=[&](const std::string& key,double& value){return connection.number("targeting."+key,value)&&std::isfinite(value)&&value>=0&&value<=9007199254740991.&&std::floor(value)==value;};
  if(!counter("epoch",epoch)||epoch==0||!counter("revision",revision)||!counter("input_revision",input)||!connection.text("targeting.kind",next.kind))break;
  next.epoch=uint64_t(epoch);next.revision=uint64_t(revision);next.inputRevision=uint64_t(input);
  if(next.epoch<targetingEpoch)break;
  if(next.kind=="clear"){if(next.epoch>targetingEpoch){clearTargeting();targetingEpoch=next.epoch;}break;}
  if(next.epoch!=targetingEpoch||next.revision<=targeting.revision||next.inputRevision!=pickInputRevision||!spawned)break;
  bool valid=true;auto vector=[&](const std::string& key,std::array<double,3>& out,double bound){size_t size=0;if(!connection.arraySize("targeting."+key,size)||size!=3)return false;for(int i=0;i<3;++i)if(!connection.number("targeting."+key+"."+std::to_string(i),out[i])||!std::isfinite(out[i])||std::abs(out[i])>bound)return false;return true;};
  valid=vector("eye",next.eye,30000064.);
  double attackId=0;if(connection.number("targeting.attack_entity",attackId)){std::array<double,3> location;next.hasAttack=packInt(connection,"targeting.attack_entity",next.attackEntityId)&&vector("attack_location",location,30000064.);valid=next.hasAttack&&valid;}
  if(next.kind=="block"||next.kind=="entity")valid=vector("location",next.location,30000064.)&&valid;
  if(next.kind=="block"){
   for(int i=0;i<3;++i)valid=packInt(connection,"targeting.block."+std::to_string(i),next.block[i])&&std::abs(double(next.block[i]))<=30000064.&&valid;
   int state=0;valid=packInt(connection,"targeting.state",state)&&state>=0&&state<=65535&&packInt(connection,"targeting.face",next.face)&&next.face>=0&&next.face<6&&valid;next.state=state;
   valid=connection.boolean("targeting.outline.complete",next.complete)&&valid;
   auto geometry=[&](const char* name,size_t limit,std::vector<std::array<double,6>>& out){size_t size=0;const std::string key=std::string("targeting.outline.")+name;if(!connection.arraySize(key,size)||size>limit)return false;for(size_t n=0;n<size;++n){size_t length=0;std::array<double,6> box{};const std::string row=key+"."+std::to_string(n);if(!connection.arraySize(row,length)||length!=6)return false;for(int j=0;j<6;++j)if(!connection.number(row+"."+std::to_string(j),box[j])||!std::isfinite(box[j])||std::abs(box[j])>1024.)return false;out.push_back(box);}return true;};
   valid=geometry("boxes",128,next.boxes)&&geometry("edges",2048,next.edges)&&valid;
   if(!next.complete&&(!next.boxes.empty()||!next.edges.empty()))valid=false;
  }else if(next.kind=="entity"){valid=packInt(connection,"targeting.entity",next.entityId)&&valid;}else if(next.kind!="miss")valid=false;
  if(valid){next.world=worldRevision;targeting=std::move(next);}break;
 }
 case TCPE_JAVA_LOGIN:clearTargeting();clearItemUse();recipeGhost=RecipeGhost{};recipes.clear();++recipesRevision;scoreboard.clear();bossOverlay.clear();titleOverlay.clear(true);actionBar.clear();actionBarTicks=0;absorption=0;experienceProgress=experienceLevel=totalExperience=0;canFly=flying=hasAbilities=false;++abilitiesRevision;lookCorrections.clear();pendingVisualEffects.clear();pendingSounds.clear();++worldRevision;chatActionsPending=false;chatActions.clear();chunks.clear();entities.clear();spawned=false;hasPosition=false;dead=false;inventory=Inventory{};compass=CompassState{};break;
 case TCPE_JAVA_SPAWN:spawned=true;dead=false;phase="Playing";break;
 case TCPE_JAVA_POSITION:{std::array<double,3> next;bool valid=true;for(int i=0;i<3;i++)valid=connection.number("position."+std::to_string(i),next[i])&&valid;if(valid){position=next;hasPosition=true;}break;}
 case TCPE_JAVA_LIGHT:if(!chunks.applyLight(update))error="Invalid Java lighting snapshot";break;
 case TCPE_JAVA_CHUNK:if(!chunks.apply(update))error="Invalid Java chunk snapshot";break;
 case TCPE_JAVA_UNLOAD_CHUNK:chunks.unload(update.x,update.z);break;
 case TCPE_JAVA_CHAT:{std::string text,signature;connection.text("signature",signature);if(!signature.empty()&&!validChatSignature(signature))break;if(!signature.empty()){signature=std::to_string(chatScope)+":"+signature;if(deletedChatSignatures.count(signature))break;}if(connection.text("text",text)){text=displayText(text);lastChat=text;lastChatSignature=signature;messages.push_back(text);messageSignatures.push_back(signature);if(messages.size()>256){messages.erase(messages.begin());messageSignatures.erase(messageSignatures.begin());}}break;}
 case TCPE_JAVA_DELETE_CHAT:{std::string signature;if(!connection.text("signature",signature)||!validChatSignature(signature))break;signature=std::to_string(chatScope)+":"+signature;if(deletedChatSignatures.size()>=256)deletedChatSignatures.erase(deletedChatSignatures.begin());deletedChatSignatures.insert(signature);if(pendingChatDeletes.size()<256)pendingChatDeletes.push_back(signature);for(size_t i=messageSignatures.size();i>0;--i)if(messageSignatures[i-1]==signature){messageSignatures.erase(messageSignatures.begin()+i-1);if(i<=messages.size())messages.erase(messages.begin()+i-1);}if(chatActionSignature==signature){chatActionsPending=false;chatActions.clear();chatActionContext.clear();chatActionSignature.clear();}break;}
 case TCPE_JAVA_CHAT_ACTIONS:{
  ++chatActionsRevision;chatActions.clear();chatActionsPending=false;chatActionContext=lastChat;chatActionSignature=lastChatSignature;size_t count=0;
  if(connection.arraySize("actions",count))for(size_t i=0;i<count&&i<64;i++){ChatAction action;const std::string path="actions."+std::to_string(i);if(connection.text(path+".0",action.label)&&connection.text(path+".1",action.command)&&!action.command.empty()&&action.command[0]=='/'&&action.command.size()<=32767){action.label=displayText(action.label);if(action.label.empty())action.label="Select";action.label=action.label.substr(0,4096);chatActions.push_back(action);}}
  chatActionsPending=!chatActions.empty();break;
 }
 case TCPE_JAVA_ERROR:clearTargeting();clearItemUse();pendingVisualEffects.clear();pendingSounds.clear();connection.text("message",error);error=displayText(error);phase="Error";spawned=false;break;
 case TCPE_JAVA_DISCONNECTED:clearTargeting();clearItemUse();pendingVisualEffects.clear();pendingSounds.clear();compass=CompassState{};connection.text("reason",error);error=displayText(error);phase="Disconnected";spawned=false;break;
 case TCPE_JAVA_CONFIGURATION:clearTargeting();recipeGhost=RecipeGhost{};recipes.clear();++recipesRevision;compass=CompassState{};connection.text("phase",phase);break;
 case TCPE_JAVA_PLAYER_ID:playerId=integer(connection,"id");break;
 case TCPE_JAVA_TIME:{double value;if(connection.number("day",value)&&value>=0&&value<18446744073709551616.0)dayTime=static_cast<uint64_t>(value);connection.boolean("ticking",dayTicking);break;}
 case TCPE_JAVA_SCOREBOARD:{
  const std::string path="scoreboard_update.";std::string op,name,owner;if(!connection.text(path+"operation",op))break;
  auto number=[&](const std::string& p){JavaScoreNumber out;connection.text(p+".kind",out.kind);connection.text(p+".text",out.text);connection.text(p+".nbt_base64",out.nbt);return out;};
  if(op=="display"){if(connection.text(path+"objective",name)){const int slot=integer(connection,path+"slot",-1);if(slot>=0&&slot<19){if(name.empty())scoreboard.displays.erase(slot);else scoreboard.displays[slot]=name;}}break;}
  if(op=="reset"){if(connection.text(path+"owner",owner)){const bool one=connection.text(path+"objective",name);scoreboard.resetScore(owner,one?&name:nullptr);}break;}
  if(op=="score"){if(connection.text(path+"objective",name)&&connection.text(path+"owner",owner)){auto objective=scoreboard.objectives.find(name);if(objective!=scoreboard.objectives.end()){JavaScoreEntry entry;entry.score=integer(connection,path+"score");entry.hasDisplay=connection.text(path+"display",entry.display);entry.number=number(path+"number_format");objective->second.scores[owner]=entry;}}break;}
  if(op=="objective_remove"){if(connection.text(path+"objective",name))scoreboard.removeObjective(name);break;}
  if(op=="objective_add"||op=="objective_change"){if(!connection.text(path+"objective",name))break;auto objective=scoreboard.objectives.find(name);if(op=="objective_change"&&objective==scoreboard.objectives.end())break;auto& value=scoreboard.objectives[name];connection.text(path+"text",value.text);value.renderType=integer(connection,path+"render_type");value.number=number(path+"number_format");break;}
  if(!connection.text(path+"team",name))break;
  if(op=="team_remove"){scoreboard.removeTeam(name);break;}
  if(op=="team_add")scoreboard.teams.emplace(name,JavaScoreTeam{});
  auto team=scoreboard.teams.find(name);if(team==scoreboard.teams.end())break;
  if(op=="team_add"||op=="team_change"){auto& t=team->second;connection.text(path+"parameters.text",t.text);connection.text(path+"parameters.prefix",t.prefix);connection.text(path+"parameters.suffix",t.suffix);t.color=integer(connection,path+"parameters.color",21);t.options=integer(connection,path+"parameters.options");t.visibility=integer(connection,path+"parameters.visibility");t.collision=integer(connection,path+"parameters.collision");}
  if(op=="team_add"||op=="team_join"||op=="team_leave"){size_t count=0;if(connection.arraySize(path+"members",count))for(size_t i=0;i<count;++i){std::string member;if(connection.text(path+"members."+std::to_string(i),member)){if(op=="team_leave")scoreboard.leave(name,member);else scoreboard.join(name,member);}}}
  break;
 }
 case TCPE_JAVA_BOSS:{
  std::string operation,id,text;const std::string path="boss_update.";
  if(!connection.text(path+"operation",operation)||!connection.text(path+"id",id)||id.empty())break;
  if(std::getenv("TCPE_JAVA_TRACE_ENTITIES"))fprintf(stderr,"Java native boss event: operation=%s id=%s known=%d\n",operation.c_str(),id.c_str(),int(bossOverlay.find(id)!=nullptr));
  if(operation=="remove"){bossOverlay.remove(id);break;}
  const double now=JavaBossState::now();
  auto style=[&](JavaBossBar& b){b.color=std::max(0,std::min(6,integer(connection,path+"color")));b.overlay=std::max(0,std::min(4,integer(connection,path+"overlay")));};
  auto properties=[&](JavaBossBar& b){connection.boolean(path+"darken_screen",b.darkenScreen);connection.boolean(path+"play_music",b.playMusic);connection.boolean(path+"create_world_fog",b.createWorldFog);};
  if(operation=="add"){JavaBossBar bar;bar.id=id;connection.text(path+"text",bar.text);connection.number(path+"progress",bar.target);style(bar);properties(bar);bossOverlay.add(bar,now);break;}
  auto* bar=bossOverlay.find(id);if(!bar)break;
  if(operation=="progress"){double value;if(connection.number(path+"progress",value))bar->retarget(value,now);}
  else if(operation=="name"&&connection.text(path+"text",text))bar->text=text;
  else if(operation=="style")style(*bar);
  else if(operation=="properties")properties(*bar);
  break;
 }
 case TCPE_JAVA_TITLE:{std::string operation,text;if(!connection.text("title_update.operation",operation))break;if(operation=="title"&&connection.text("title_update.text",text))titleOverlay.setTitle(text);else if(operation=="subtitle"&&connection.text("title_update.text",text))titleOverlay.subtitle=text;else if(operation=="times")titleOverlay.setTimes(integer(connection,"title_update.fade_in",-1),integer(connection,"title_update.stay",-1),integer(connection,"title_update.fade_out",-1));else if(operation=="clear"){bool reset=false;connection.boolean("title_update.reset_times",reset);titleOverlay.clear(reset);}break;}
 case TCPE_JAVA_ACTION_BAR:if(connection.text("text",actionBar))actionBarTicks=actionBar.empty()?0:60;break;
 case TCPE_JAVA_VITALS:{connection.boolean("vitals.attack_piercing",attackPiercing);connection.boolean("vitals.use_active",itemUseActive);connection.boolean("vitals.use_off_hand",itemUseOffHand);connection.number("vitals.use_ticks",itemUseTicks);connection.number("vitals.use_duration",itemUseDuration);itemUseChargeDuration=0;connection.number("vitals.use_charge_duration",itemUseChargeDuration);if(!std::isfinite(itemUseChargeDuration)||itemUseChargeDuration<0||itemUseChargeDuration>72000)itemUseChargeDuration=0;connection.text("vitals.use_animation",itemUseAnimation);connection.number("vitals.use_kinetic_delay",itemUseKineticDelay);connection.number("vitals.use_kinetic_dismount",itemUseKineticDismount);connection.number("vitals.use_kinetic_knockback",itemUseKineticKnockback);connection.number("vitals.use_kinetic_damage",itemUseKineticDamage);connection.number("vitals.attack_strength",attackStrength);connection.number("vitals.attack_min_charge",attackMinCharge);if(!std::isfinite(itemUseTicks)||itemUseTicks<0)itemUseTicks=0;if(!std::isfinite(itemUseDuration)||itemUseDuration<0)itemUseDuration=0;connection.number("vitals.attack_min_range",attackMinRange);connection.number("vitals.attack_max_range",attackMaxRange);connection.number("vitals.attack_hitbox_margin",attackHitboxMargin);connection.boolean("vitals.show_death_screen",showDeathScreen);bool mayFly=canFly,isFlying=flying;const bool known=connection.boolean("vitals.can_fly",mayFly)&&connection.boolean("vitals.flying",isFlying);if(known&&(!hasAbilities||mayFly!=canFly||isFlying!=flying)){hasAbilities=true;canFly=mayFly;flying=isFlying;++abilitiesRevision;}}connection.number("vitals.health",health);connection.number("vitals.max_health",maxHealth);connection.number("vitals.food",food);connection.number("vitals.saturation",saturation);connection.number("vitals.armor",armor);connection.number("vitals.absorption",absorption);connection.number("vitals.experience_progress",experienceProgress);connection.number("vitals.experience_level",experienceLevel);connection.number("vitals.total_experience",totalExperience);dead=health<=0;if(std::getenv("TCPE_JAVA_TRACE_ENTITIES"))fprintf(stderr,"Java vitals: health=%.2f armor=%.2f dead=%d\n",health,armor,int(dead));break;
 case TCPE_JAVA_DEATH:clearTargeting();recipeGhost=RecipeGhost{};connection.text("message",deathMessage);deathMessage=displayText(deathMessage);dead=true;if(std::getenv("TCPE_JAVA_TRACE_ENTITIES"))fprintf(stderr,"Java death event received\n");break;
 case TCPE_JAVA_INVENTORY:{
  ++inventoryRevision;recipeGhost=RecipeGhost{};
  inventory.windowId=integer(connection,"inventory.window_id");connection.text("inventory.title",inventory.title);inventory.title=displayText(inventory.title);inventory.selected=static_cast<unsigned>(integer(connection,"inventory.selected_hotbar"));
  size_t count=0;inventory.slots.clear();if(connection.arraySize("inventory.items",count)&&count<=4096){for(size_t i=0;i<count;i++)inventory.slots.push_back(item("inventory.items."+std::to_string(i)));}
  inventory.menuGeneration=0;packScope(connection,"inventory.menu_generation",inventory.menuGeneration);inventory.properties.clear();
  for(int id=0;id<256;++id){int value=0;if(packInt(connection,"inventory.properties."+std::to_string(id),value)&&value>=-32768&&value<=32767)inventory.properties[id]=value;}
  size_t propertyCount=0;if(connection.arraySize("inventory.property_entries",propertyCount)){
   inventory.properties.clear();if(propertyCount<=256)for(size_t i=0;i<propertyCount;++i){int id=0,value=0;const std::string path="inventory.property_entries."+std::to_string(i);
    if(packInt(connection,path+".id",id)&&id>=0&&id<=32767&&packInt(connection,path+".value",value)&&value>=-32768&&value<=32767)inventory.properties[id]=value;
   }
  }
  inventory.menuKind.clear();connection.text("inventory.menu_kind",inventory.menuKind);inventory.slotRanges.clear();
  for(const char* role:{"container","player_inventory","hotbar","crafting","ingredient","fuel","result","armor","offhand"}){
   const std::string path=std::string("inventory.slot_ranges.")+role;double start=0,end=0;
   if(connection.number(path+".start",start)&&connection.number(path+".end",end)&&std::isfinite(start)&&std::isfinite(end)&&start==std::floor(start)&&end==std::floor(end)&&start>=0&&end>start&&end<=inventory.slots.size()){
    SlotRange range;range.start=int(start);range.end=int(end);inventory.slotRanges[role]=range;
   }
  }
  for(size_t i=0;i<9;i++)inventory.hotbar[i]=item("inventory.hotbar."+std::to_string(i));for(size_t i=0;i<4;i++)inventory.armor[i]=item("inventory.armor."+std::to_string(i));inventory.held=item("inventory.held");inventory.offHand=item("inventory.offhand");if(inventory.offHand.count<=0)useOffHand=false;inventory.carried=item("inventory.carried_item");break;
 }
 case TCPE_JAVA_DIALOG:{
  const unsigned revision=dialog.revision+1;dialog=Dialog{};dialog.revision=revision;bool cleared=false;if(connection.boolean("cleared",cleared)&&cleared)break;dialog.pending=true;
  dialog.title=displayText(componentText(connection,"title"));size_t count=0;
  if(connection.arraySize("body",count))for(size_t i=0;i<count&&i<128;i++){const std::string path="body."+std::to_string(i);std::string text=componentText(connection,path+".contents");if(text.empty())text=componentText(connection,path+".description.contents");if(!text.empty()){dialog.body+=displayText(text)+"\n\n";if(dialog.body.size()>65536){dialog.body.resize(65536);break;}}}
  auto action=[&](const std::string& path){std::string type,id;if(connection.text(path+".action.type",type)&&type=="minecraft:custom"&&connection.text(path+".action.id",id)&&!id.empty()&&id.size()<=32767){DialogAction button;button.label=displayText(componentText(connection,path+".label"));if(button.label.empty())button.label="Continue";button.id=id;dialog.actions.push_back(button);}};
  action("action");if(connection.arraySize("actions",count))for(size_t i=0;i<count&&i<32;i++)action("actions."+std::to_string(i));action("exit_action");break;
 }
 case TCPE_JAVA_REMOVED_ENTITIES:{size_t count=0;if(connection.arraySize("removed_entities",count))for(size_t i=0;i<count;++i){double id;if(connection.number("removed_entities."+std::to_string(i),id)&&std::isfinite(id)&&std::floor(id)==id&&id>=std::numeric_limits<int>::min()&&id<=std::numeric_limits<int>::max())entities.erase(static_cast<int>(id));}break;}
 case TCPE_JAVA_ENTITIES:{
  size_t count=0;if(!connection.arraySize("entities",count)||count>4096)break;std::map<int,Entity> next;
  for(size_t i=0;i<count;i++){const std::string path="entities."+std::to_string(i);Entity entity{};entity.id=integer(connection,path+".id");connection.text(path+".kind",entity.kind);connection.boolean(path+".using_item",entity.usingItem);connection.boolean(path+".using_offhand",entity.usingOffhand);connection.number(path+".use_ticks",entity.useTicks);connection.number(path+".use_duration",entity.useDuration);connection.number(path+".use_charge_duration",entity.useChargeDuration);if(!std::isfinite(entity.useChargeDuration)||entity.useChargeDuration<0||entity.useChargeDuration>72000)entity.useChargeDuration=0;connection.text(path+".use_animation",entity.useAnimation);connection.number(path+".use_kinetic_delay",entity.useKineticDelay);connection.number(path+".use_kinetic_dismount",entity.useKineticDismount);connection.number(path+".use_kinetic_knockback",entity.useKineticKnockback);connection.number(path+".use_kinetic_damage",entity.useKineticDamage);bool valid=true;for(int axis=0;axis<3;axis++)valid=connection.number(path+".position."+std::to_string(axis),entity.position[axis])&&valid;if(!valid)continue;connection.number(path+".yaw",entity.yaw);connection.number(path+".pitch",entity.pitch);connection.number(path+".head_yaw",entity.headYaw);connection.boolean(path+".sneaking",entity.sneaking);for(size_t slot=0;slot<6;slot++)entity.equipment[slot]=item(path+".equipment."+std::to_string(slot));entity.droppedItem=item(path+".dropped_item");connection.boolean(path+".firework.shot_at_angle",entity.fireworkShotAtAngle);double attached=-1;if(connection.number(path+".firework.attached_to_target",attached)&&std::isfinite(attached)&&attached>=0&&attached<=2147483647.0&&std::floor(attached)==attached)entity.fireworkAttachedTo=int(attached);entity.hurtTicks=integer(connection,path+".hurt_ticks");entity.deathTicks=integer(connection,path+".death_ticks");
   int animation=0;if(packInt(connection,path+".mob_attack_ticks",animation)&&animation>=0&&animation<=10)entity.mobAttackTicks=unsigned(animation);
   if(packInt(connection,path+".mob_flower_ticks",animation)&&animation>=0&&animation<=400)entity.mobFlowerTicks=unsigned(animation);
   double livingHealth=0;if(connection.number(path+".living_health",livingHealth)&&std::isfinite(livingHealth)&&std::abs(livingHealth)<=std::numeric_limits<float>::max()){entity.livingHealth=static_cast<float>(livingHealth);entity.hasLivingHealth=std::isfinite(entity.livingHealth);}
   double maxHealth=0;if(connection.number(path+".living_max_health",maxHealth)&&std::isfinite(maxHealth)&&maxHealth>0&&maxHealth<=std::numeric_limits<float>::max()){entity.livingMaxHealth=static_cast<float>(maxHealth);entity.hasLivingMaxHealth=std::isfinite(entity.livingMaxHealth)&&entity.livingMaxHealth>0;}

   for(int axis=0;axis<2;++axis)connection.number(path+".hitbox."+std::to_string(axis),entity.hitbox[axis]);
   entity.skinLayers=unsigned(integer(connection,path+".skin_layers"))&127;connection.text(path+".profile.name",entity.profileName);connection.text(path+".profile.uuid",entity.profileUuid);connection.text(path+".profile.textures",entity.profileTextures);connection.text(path+".profile.textures_signature",entity.profileTexturesSignature);connection.text(path+".profile.decoded_textures.skin_url",entity.profileSkinUrl);connection.text(path+".profile.decoded_textures.cape_url",entity.profileCapeUrl);connection.boolean(path+".profile.decoded_textures.slim",entity.profileSkinSlim);
   connection.text(path+".custom_name",entity.customName);connection.text(path+".custom_name_json",entity.customNameJson);connection.boolean(path+".custom_name_visible",entity.customNameVisible);connection.boolean(path+".invisible",entity.invisible);connection.boolean(path+".armor_stand_marker",entity.armorStandMarker);
   const std::string stand=path+".armor_stand";connection.boolean(stand+".small",entity.armorStand.small);connection.boolean(stand+".show_arms",entity.armorStand.showArms);connection.boolean(stand+".show_base_plate",entity.armorStand.showBasePlate);
   for(int part=0;part<6;++part)for(int axis=0;axis<3;++axis){double angle=0;if(connection.number(stand+".poses."+std::to_string(part)+"."+std::to_string(axis),angle)&&std::isfinite(angle)&&std::abs(angle)<=36000)entity.armorStand.poses[part][axis]=float(angle);}
   const std::string display=path+".display";connection.text(display+".text",entity.display.text);connection.text(display+".text_json",entity.display.textJson);
   for(int axis=0;axis<3;++axis){connection.number(display+".translation."+std::to_string(axis),entity.display.translation[axis]);connection.number(display+".scale."+std::to_string(axis),entity.display.scale[axis]);}
   for(int axis=0;axis<4;++axis){connection.number(display+".left_rotation."+std::to_string(axis),entity.display.leftRotation[axis]);connection.number(display+".right_rotation."+std::to_string(axis),entity.display.rightRotation[axis]);}
   entity.display.billboard=integer(connection,display+".billboard");entity.display.textOpacity=integer(connection,display+".text_opacity",255);entity.display.styleFlags=integer(connection,display+".style_flags");entity.display.lineWidth=integer(connection,display+".line_width",200);entity.display.backgroundColor=integer(connection,display+".background_color",0x40000000);entity.display.brightness=integer(connection,display+".brightness",-1);entity.display.blockState=integer(connection,display+".block_state");entity.display.itemTransform=integer(connection,display+".item_transform");entity.display.item=item(display+".item");
   connection.number(display+".view_range",entity.display.viewRange);connection.number(display+".width",entity.display.width);connection.number(display+".height",entity.display.height);
   next[entity.id]=entity;}
  entities=std::move(next);break;
 }
 default:break;
 }
}

bool JavaGameSession::requestPackFonts(size_t index){if(index>=resourcePacks.size())return false;return connection.send("{\"type\":\"load_pack_fonts\",\"connection\":"+std::to_string(resourcePackConnection)+",\"generation\":"+std::to_string(resourcePackGeneration)+",\"id\":"+quoteDialogId(resourcePacks[index].id)+"}");}
