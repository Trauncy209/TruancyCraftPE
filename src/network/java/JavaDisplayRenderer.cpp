#include "JavaDisplayRenderer.h"
#include "JavaRichText.h"
#include "JavaBlockModels.h"
#include "JavaPackItemModels.h"
#include "JavaShieldModel.h"
#include "JavaItemIcons.h"
#include "JavaArmorStandModel.h"
#include <map>
#include "client/renderer/entity/EntityRenderDispatcher.h"
#include "client/renderer/entity/ItemRenderer.h"
#include "client/renderer/Tesselator.h"
#include "client/renderer/Textures.h"
#include "client/gui/Font.h"
#include "world/item/Item.h"
#include "world/level/Level.h"
#include "world/item/ItemInstance.h"
#include <algorithm>
#include <cmath>
#include <vector>
namespace {
std::string utf8(uint32_t cp){std::string out;if(cp<128)out+=char(cp);else if(cp<2048){out+=char(0xc0|(cp>>6));out+=char(0x80|(cp&63));}else if(cp<65536){out+=char(0xe0|(cp>>12));out+=char(0x80|((cp>>6)&63));out+=char(0x80|(cp&63));}else{out+=char(0xf0|(cp>>18));out+=char(0x80|((cp>>12)&63));out+=char(0x80|((cp>>6)&63));out+=char(0x80|(cp&63));}return out;}
std::string styled(const JavaRichText::Glyph& g){return JavaRichText::encode(g);}
struct Line{std::string text;float width=0;};
std::vector<Line> lines(Font& font,const std::string& text,int width){
 std::vector<Line> out(1);const auto glyphs=JavaRichText::parse(text);size_t count=0;
 for(const auto& glyph:glyphs){if(++count>16384||out.size()>256)break;if(glyph.codepoint=='\n'){out.emplace_back();continue;}const std::string token=styled(glyph);const float advance=font.javaGlyphAdvance(glyph.codepoint,glyph.bold,glyph.font);if(width>0&&out.back().width>0&&out.back().width+advance>width)out.emplace_back();out.back().text+=token;out.back().width+=advance;}
 return out;
}
void quad(float x0,float y0,float x1,float y1,uint32_t color){
 auto& t=Tesselator::instance;glDisable2(GL_TEXTURE_2D);t.begin();t.color(int(color&0xffffff),int(color>>24));t.vertex(x0,y0,0);t.vertex(x0,y1,0);t.vertex(x1,y1,0);t.vertex(x1,y0,0);t.draw();glEnable2(GL_TEXTURE_2D);
}
using ItemContexts=std::map<std::string,std::array<float,9>>;
const std::map<std::string,ItemContexts>& standItemContexts(){static const std::map<std::string,ItemContexts> value={
#include "JavaArmorStandItemTransforms.inc"
};return value;}
bool standItem(Textures* textures,const JavaGameSession::Item& item,const char* context,float brightness){
 if(item.count<=0)return false;const std::string model=item.itemModel.empty()?item.resourceId:item.itemModel;
 if(JavaPackItemModels::has(model))return JavaPackItemModels::renderContext(textures,model,context,brightness,&item);
 if(item.resourceId=="minecraft:shield")return JavaShieldModel::renderContext(textures,context,brightness,&item);
 if(!JavaItemIcons::has(item.resourceId))return false;
 const bool left=std::string(context)=="thirdperson_lefthand";auto found=standItemContexts().find(item.resourceId);
 glPushMatrix2();if(found!=standItemContexts().end()){auto transform=found->second.find(context);if(transform==found->second.end()&&left)transform=found->second.find("thirdperson_righthand");if(transform!=found->second.end()){const auto&a=transform->second;glTranslatef2((left?-a[3]:a[3])/16,a[4]/16,a[5]/16);glRotatef2(a[0],1,0,0);glRotatef2(left?-a[1]:a[1],0,1,0);glRotatef2(left?-a[2]:a[2],0,0,1);glScalef2(a[6],a[7],a[8]);}}
 glTranslatef2(0,-.5f,0);const bool drawn=JavaItemIcons::renderWorld(textures,item.resourceId,JavaItemIcons::isBlock(item.resourceId)?2.f:1.f,brightness,&item);glPopMatrix2();return drawn;
}
std::string armorMaterial(const std::string& resource){
 const auto colon=resource.find(':');if(colon==std::string::npos||resource.substr(0,colon)!="minecraft")return "";const std::string name=resource.substr(colon+1);const auto split=name.rfind('_');if(split==std::string::npos)return "";const auto type=name.substr(split+1);if(type!="helmet"&&type!="chestplate"&&type!="leggings"&&type!="boots")return "";const auto material=name.substr(0,split);if(material=="turtle")return "turtle_scute";if(material=="golden")return "gold";if(material=="leather"||material=="chainmail"||material=="iron"||material=="diamond"||material=="netherite")return material;return "";
}
void armorStand(const JavaGameSession::Entity& snapshot,EntityRenderDispatcher& dispatcher,float x,float y,float z){
 if(!dispatcher.textures)return;static JavaArmorStandModel model;static JavaArmorStandArmorModel outer(1.f),inner(.5f);model.apply(snapshot);
 const bool cull=glIsEnabled(GL_CULL_FACE),blend=glIsEnabled(GL_BLEND),alpha=glIsEnabled(GL_ALPHA_TEST);GLint src=0,dst=0,func=0;GLfloat ref=0;glGetIntegerv(GL_BLEND_SRC,&src);glGetIntegerv(GL_BLEND_DST,&dst);glGetIntegerv(GL_ALPHA_TEST_FUNC,&func);glGetFloatv(GL_ALPHA_TEST_REF,&ref);
 glDisable2(GL_CULL_FACE);glEnable2(GL_BLEND);glBlendFunc2(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glEnable2(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.1f);
 float brightness=dispatcher.level?dispatcher.level->getBrightness(int(std::floor(snapshot.position[0])),int(std::floor(snapshot.position[1])),int(std::floor(snapshot.position[2]))):1.f;
 glPushMatrix2();glTranslatef2(x,y,z);glRotatef2(180-float(snapshot.yaw),0,1,0);const float scale=snapshot.armorStand.small?.5f:1.f;glScalef2(-scale,-scale,scale);glTranslatef2(0,-1.501f,0);
 if(!snapshot.invisible){dispatcher.textures->loadAndBindTexture("mob/java-armor-stand.png");glColor4f2(brightness,brightness,brightness,1);model.renderBody(snapshot);}
 // Vanilla invisibility hides the stand, not its worn/held equipment.
 for(int slot=2;slot<6;++slot){const auto&item=snapshot.equipment[slot];if(item.count<=0)continue;const std::string modelId=item.itemModel.empty()?item.resourceId:item.itemModel;const auto material=armorMaterial(item.resourceId);if(material.empty()||(slot==5&&JavaPackItemModels::has(modelId)))continue;
 auto&armor=slot==3?inner:outer;const std::string layer=slot==3?"-2.png":"-1.png";dispatcher.textures->loadAndBindTexture("armor/java-"+material+layer);const uint32_t dye=material=="leather"?(item.hasDyedColor?item.dyedColor:0xa06540):0xffffff;glColor4f2(((dye>>16)&255)/255.f*brightness,((dye>>8)&255)/255.f*brightness,(dye&255)/255.f*brightness,1);armor.render(snapshot,slot);if(material=="leather"){dispatcher.textures->loadAndBindTexture("armor/java-leather_overlay"+layer);glColor4f2(brightness,brightness,brightness,1);armor.render(snapshot,slot);}}
 const auto&head=snapshot.equipment[5];const std::string headModel=head.itemModel.empty()?head.resourceId:head.itemModel;
 if(head.count>0&&(armorMaterial(head.resourceId).empty()||JavaPackItemModels::has(headModel))){glPushMatrix2();model.headTransform();const bool skull=!JavaPackItemModels::has(headModel)&&(head.resourceId=="minecraft:player_head"||head.resourceId=="minecraft:zombie_head"||head.resourceId=="minecraft:skeleton_skull"||head.resourceId=="minecraft:creeper_head");if(skull){glScalef2(1.1875f,-1.1875f,-1.1875f);JavaItemIcons::renderWorld(dispatcher.textures,head.resourceId,.5f,brightness,&head);}else{glTranslatef2(0,-.25f,0);glRotatef2(180,0,1,0);glScalef2(.625f,-.625f,-.625f);standItem(dispatcher.textures,head,"head",brightness);}glPopMatrix2();}
 for(int hand=0;hand<2;++hand){if(snapshot.equipment[hand].count<=0)continue;glPushMatrix2();model.handTransform(hand==1);glRotatef2(-90,1,0,0);glRotatef2(180,0,1,0);glTranslatef2(hand==1?-.0625f:.0625f,.125f,-.625f);standItem(dispatcher.textures,snapshot.equipment[hand],hand==1?"thirdperson_lefthand":"thirdperson_righthand",brightness);glPopMatrix2();}
 glPopMatrix2();glBlendFunc2(src,dst);glAlphaFunc(func,ref);if(cull)glEnable2(GL_CULL_FACE);if(!blend)glDisable2(GL_BLEND);if(!alpha)glDisable2(GL_ALPHA_TEST);glColor4f2(1,1,1,1);
}
void quaternion(const std::array<double,4>& q){
 const double length=std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);if(!std::isfinite(length)||length<1e-8)return;
 const double w=std::max(-1.,std::min(1.,q[3]/length));const double sine=std::sqrt(std::max(0.,1-w*w));if(sine<1e-8)return;
 glRotatef2(float(2*std::acos(w)*180/3.141592653589793),float(q[0]/length/sine),float(q[1]/length/sine),float(q[2]/length/sine));
}
void billboard(unsigned mode,const JavaGameSession::Entity& entity,EntityRenderDispatcher& dispatcher){
 const float yaw=(mode==1||mode==3)?dispatcher.playerRotY:float(entity.yaw);
 const float pitch=(mode==2||mode==3)?dispatcher.playerRotX:float(entity.pitch);
 glRotatef2(-yaw,0,1,0);glRotatef2(pitch,1,0,0);
}
void text(const std::string& value,Font& font,int lineWidth,unsigned flags,unsigned opacity,uint32_t background,bool displayAnchor=false){
 if(opacity==0||value.empty())return;
 const auto wrapped=lines(font,value,lineWidth);float boxWidth=0;for(const auto& line:wrapped)boxWidth=std::max(boxWidth,line.width);
 const float height=float(wrapped.size()*Font::DefaultLineHeight);
 glEnable2(GL_BLEND);glBlendFunc2(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(false);if(flags&2)glDisable2(GL_DEPTH_TEST);glDisable2(GL_CULL_FACE);
 glScalef2(-.025f,-.025f,.025f);
 // Java text displays anchor the bottom of the line box at the entity origin.
 if(displayAnchor)glTranslatef2(0,-(height-1),0);
 if(background>>24)quad(-boxWidth*.5f-1,-1,boxWidth*.5f+1,height,background);
 for(size_t i=0;i<wrapped.size();++i){const auto& line=wrapped[i];const float x=(flags&8)?-boxWidth*.5f:(flags&16)?boxWidth*.5f-line.width:-line.width*.5f;const float y=float(i*Font::DefaultLineHeight);const uint32_t color=(std::min(opacity,255u)<<24)|0xffffff;
  if(flags&1)font.drawJava(line.text,x+1,y+1,int(color),true);font.drawJava(line.text,x,y,int(color));}
 glEnable2(GL_CULL_FACE);glEnable2(GL_DEPTH_TEST);glDepthMask(true);glDisable2(GL_BLEND);glColor4f2(1,1,1,1);
}
}
void renderJavaCustomName(const JavaGameSession::Entity& snapshot,EntityRenderDispatcher& dispatcher,float x,float y,float z,float height){
 if(!snapshot.customNameVisible||snapshot.customName.empty()||!dispatcher.getFont())return;
 if(dispatcher.distanceToSqr(float(snapshot.position[0]),float(snapshot.position[1]),float(snapshot.position[2]))>4096)return;
 glPushMatrix2();glTranslatef2(x,y+height+.25f,z);billboard(3,snapshot,dispatcher);text(snapshot.customName,*dispatcher.getFont(),0,2,255,0x40000000);glPopMatrix2();
}
float javaArmorStandNameHeight(const JavaGameSession::Entity& snapshot){
 if(snapshot.armorStandMarker)return 0.f;
 return std::isfinite(snapshot.hitbox[1])&&snapshot.hitbox[1]>0?float(snapshot.hitbox[1]):1.975f;
}
void renderJavaDisplay(const JavaGameSession::Entity& snapshot,EntityRenderDispatcher& dispatcher,float x,float y,float z){
 if(snapshot.kind=="ArmorStand"){armorStand(snapshot,dispatcher,x,y,z);renderJavaCustomName(snapshot,dispatcher,x,y,z,javaArmorStandNameHeight(snapshot));return;}
 if(snapshot.kind=="TextDisplay"&&!dispatcher.getFont())return;
 const auto& display=snapshot.display;
 for(double v:display.translation)if(!std::isfinite(v))return;for(double v:display.scale)if(!std::isfinite(v))return;
 glPushMatrix2();glTranslatef2(x,y,z);billboard(display.billboard,snapshot,dispatcher);
 glTranslatef2(float(display.translation[0]),float(display.translation[1]),float(display.translation[2]));quaternion(display.leftRotation);glScalef2(float(display.scale[0]),float(display.scale[1]),float(display.scale[2]));quaternion(display.rightRotation);
 if(snapshot.kind=="TextDisplay")text(display.text,*dispatcher.getFont(),std::max(1,display.lineWidth),display.styleFlags,display.textOpacity,(display.styleFlags&4)?0x40000000:uint32_t(display.backgroundColor),true);
 if(snapshot.kind=="BlockDisplay"){
  dispatcher.textures->loadAndBindTexture(JavaBlockModels::atlasTexture());
  const int layer=JavaBlockModels::layer(uint16_t(display.blockState));
  if(layer==2){glEnable2(GL_BLEND);glBlendFunc2(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(false);}
  glEnable2(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.1f);
  float light=dispatcher.level?dispatcher.level->getBrightness(int(std::floor(snapshot.position[0])),int(std::floor(snapshot.position[1])),int(std::floor(snapshot.position[2]))):1.f;
  if(display.brightness>=0)light=std::max((display.brightness>>4)&15,(display.brightness>>20)&15)/15.f;
  auto& t=Tesselator::instance;t.begin();JavaBlockModels::tesselateDisplay(t,uint16_t(display.blockState),light);t.draw();
  if(layer==2){glDepthMask(true);glDisable2(GL_BLEND);}
 }
 if(snapshot.kind=="ItemDisplay"&&display.item.count>0){
  const std::string modelId=display.item.itemModel.empty()?display.item.resourceId:display.item.itemModel;
  bool custom=false;
  float light=dispatcher.level?dispatcher.level->getBrightness(int(std::floor(snapshot.position[0])),int(std::floor(snapshot.position[1])),int(std::floor(snapshot.position[2]))):1.f;
  if(display.brightness>=0)light=std::max((display.brightness>>4)&15,(display.brightness>>20)&15)/15.f;
  if(JavaPackItemModels::has(modelId)){
   // Official ItemDisplayRenderer rotates Y by 180 degrees before submitting
   // the NONE-context item. Entity billboard/TRS above already wraps this.
   glPushMatrix2();glRotatef2(180,0,1,0);const char* contexts[]={"none","thirdperson_lefthand","thirdperson_righthand","firstperson_lefthand","firstperson_righthand","head","gui","ground","fixed","on_shelf"};const int index=(display.itemTransform<=9?int(display.itemTransform):0);custom=JavaPackItemModels::renderContext(dispatcher.textures,modelId,contexts[index],light,&display.item);glPopMatrix2();
  }
  if(!custom&&display.item.resourceId=="minecraft:shield"){const char* contexts[]={"none","thirdperson_lefthand","thirdperson_righthand","firstperson_lefthand","firstperson_righthand","head","gui","ground","fixed","on_shelf"};glPushMatrix2();glRotatef2(180,0,1,0);custom=JavaShieldModel::renderContext(dispatcher.textures,contexts[display.itemTransform<=9?display.itemTransform:0],light,&display.item);glPopMatrix2();}
  if(!custom&&JavaItemIcons::has(display.item.resourceId)){glPushMatrix2();glRotatef2(180,0,1,0);glTranslatef2(0,-.5f,0);const float size=JavaItemIcons::isBlock(display.item.resourceId)&&display.item.resourceId!="minecraft:player_head"&&display.item.resourceId!="minecraft:zombie_head"&&display.item.resourceId!="minecraft:skeleton_skull"&&display.item.resourceId!="minecraft:creeper_head"?2.f:1.f;custom=JavaItemIcons::renderWorld(dispatcher.textures,display.item.resourceId,size,light,&display.item);glPopMatrix2();}
  if(!custom&&!(display.item.hasProfile&&display.item.resourceId=="minecraft:player_head")&&display.item.originalId>0&&display.item.originalId<Item::MAX_ITEMS&&Item::items[display.item.originalId]){
   ItemInstance instance(display.item.originalId,display.item.count,display.item.originalData);const bool cull=glIsEnabled(GL_CULL_FACE);glDisable2(GL_CULL_FACE);glScalef2(1.f/16,-1.f/16,1.f/16);ItemRenderer::renderGuiItem(dispatcher.getFont(),dispatcher.textures,&instance,-8,-8,true);if(cull)glEnable2(GL_CULL_FACE);
  }
 }
 glPopMatrix2();glColor4f2(1,1,1,1);
}
