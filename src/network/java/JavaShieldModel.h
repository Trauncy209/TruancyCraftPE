#ifndef TCPE_JAVA_SHIELD_MODEL_H
#define TCPE_JAVA_SHIELD_MODEL_H
#include "client/model/Model.h"
#include "client/renderer/Textures.h"
#include "JavaGameSession.h"
#include <algorithm>
#include <cmath>
#include <map>
// Official1.21.11 ShieldModel plate/handle and authored component layers.
class JavaShieldModel:public Model {
 ModelPart plate,handle;
 static const char* patternTexture(const std::string& asset){static const std::map<std::string,const char*> files={
#include "JavaShieldPatterns.inc"
 };auto it=files.find(asset);return it==files.end()?nullptr:it->second;}
 static void tint(unsigned dye,float light){static const uint32_t colors[]={
#include "JavaShieldDyeColors.inc"
 };const auto color=colors[std::min(15u,dye)];glColor4f2(light*((color>>16)&255)/255.f,light*((color>>8)&255)/255.f,light*(color&255)/255.f,1);}
 void renderParts(Textures* textures,const JavaGameSession::Item* item,float light){
  const bool patterned=item&&(item->shield.patterned||item->shield.hasBaseColor||!item->shield.layers.empty());
  textures->loadAndBindTexture(patterned?"item/java-shield-patterned-base.png":"item/java-shield.png");glColor4f2(light,light,light,1);handle.renderHorrible(1.f/16);plate.renderHorrible(1.f/16);
  if(!patterned)return;
  const bool blend=glIsEnabled(GL_BLEND),alpha=glIsEnabled(GL_ALPHA_TEST);GLint src=0,dst=0,depth=0,alphaFunc=0;GLfloat alphaRef=0;GLboolean mask=GL_TRUE;glGetIntegerv(GL_BLEND_SRC,&src);glGetIntegerv(GL_BLEND_DST,&dst);glGetIntegerv(GL_DEPTH_FUNC,&depth);glGetIntegerv(GL_ALPHA_TEST_FUNC,&alphaFunc);glGetFloatv(GL_ALPHA_TEST_REF,&alphaRef);glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);
  glEnable2(GL_BLEND);glBlendFunc2(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glEnable2(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.01f);glDepthFunc(GL_LEQUAL);glDepthMask(GL_FALSE);
  textures->loadAndBindTexture("item/java-shield-patterns/base.png");tint(item->shield.hasBaseColor?item->shield.baseColor:0,light);plate.renderHorrible(1.f/16);
  for(size_t i=0;i<item->shield.layers.size()&&i<16;++i){const auto& layer=item->shield.layers[i];const auto* file=patternTexture(layer.assetId);if(!file)continue;textures->loadAndBindTexture(file);tint(layer.color,light);plate.renderHorrible(1.f/16);}
  glDepthMask(mask);glDepthFunc(depth);glBlendFunc2(src,dst);glAlphaFunc(alphaFunc,alphaRef);if(!blend)glDisable2(GL_BLEND);if(!alpha)glDisable2(GL_ALPHA_TEST);glColor4f2(light,light,light,1);
 }
public:
 JavaShieldModel(){texWidth=texHeight=64;young=false;plate.setModel(this);plate.useCustomTextureDimensions=true;plate.texOffs(0,0).addBox(-6,-11,-2,12,22,1);handle.setModel(this);handle.useCustomTextureDimensions=true;handle.texOffs(26,0).addBox(-1,-3,-1,2,6,6);}
 void renderShield(){plate.renderHorrible(1.f/16);handle.renderHorrible(1.f/16);}
 static bool renderGround(Textures* textures,float light,const JavaGameSession::Item* item=nullptr){return renderContext(textures,"ground",light,item);}
 static bool renderHeld(Textures* textures,bool firstPerson,bool left,bool blocking,float light,const JavaGameSession::Item* item=nullptr){
  if(!textures)return false;const float side=left?-1.f:1.f;float x=0,y=0,z=0,rx=0,ry=0,rz=0,scale=1;
  if(firstPerson){x=blocking?(left?5.f:-15.f):(left?10.f:-10.f);y=blocking?5.f:(left?0.f:2.f);z=blocking?-11.f:-10.f;ry=180;rz=blocking?-5.f:5.f;scale=1.25f;}
  else{x=blocking?(left?11.51f:-3.49f):10.f;y=blocking?(left?7.f:11.f):6.f;z=blocking?(left?2.5f:-2.f):(left?12.f:-4.f);rx=blocking?45.f:0;ry=blocking?155.f:90.f;}
  const bool cull=glIsEnabled(GL_CULL_FACE),texture=glIsEnabled(GL_TEXTURE_2D);glPushMatrix2();glTranslatef2(side*x/16,y/16,z/16);glRotatef2(rx,1,0,0);glRotatef2(side*ry,0,1,0);glRotatef2(side*rz,0,0,1);glScalef2(scale,-scale,-scale);glEnable2(GL_TEXTURE_2D);glDisable2(GL_CULL_FACE);static JavaShieldModel model;light=std::isfinite(light)?std::max(0.f,std::min(light,1.f)):0.f;model.renderParts(textures,item,light);glPopMatrix2();if(cull)glEnable2(GL_CULL_FACE);if(!texture)glDisable2(GL_TEXTURE_2D);return true;
 }
 static bool renderContext(Textures* textures,const std::string& context,float light,const JavaGameSession::Item* item=nullptr){
  if(context=="firstperson_lefthand"||context=="firstperson_righthand")return renderHeld(textures,true,context=="firstperson_lefthand",false,light,item);
  if(context=="thirdperson_lefthand"||context=="thirdperson_righthand")return renderHeld(textures,false,context=="thirdperson_lefthand",false,light,item);
  if(!textures)return false;float x=0,y=0,z=0,rx=0,ry=0,rz=0,scale=1;
  if(context=="ground"){x=2;y=4;z=2;scale=.25f;}else if(context=="gui"){x=2;y=3;rx=15;ry=-25;rz=-5;scale=.65f;}else if(context=="fixed"){x=-4.5f;y=4.5f;z=-5;ry=180;scale=.55f;}else if(context=="on_shelf"){x=11;y=18.5f;z=8.7f;scale=1.4f;}
  const bool cull=glIsEnabled(GL_CULL_FACE),texture=glIsEnabled(GL_TEXTURE_2D);glPushMatrix2();glTranslatef2(x/16,y/16,z/16);glRotatef2(rx,1,0,0);glRotatef2(ry,0,1,0);glRotatef2(rz,0,0,1);glScalef2(scale,-scale,-scale);glEnable2(GL_TEXTURE_2D);glDisable2(GL_CULL_FACE);static JavaShieldModel model;light=std::isfinite(light)?std::max(0.f,std::min(light,1.f)):0.f;model.renderParts(textures,item,light);glPopMatrix2();if(cull)glEnable2(GL_CULL_FACE);if(!texture)glDisable2(GL_TEXTURE_2D);return true;
 }
};
#endif
