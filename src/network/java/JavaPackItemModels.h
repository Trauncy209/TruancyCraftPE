#ifndef TCPE_JAVA_PACK_ITEM_MODELS_H
#define TCPE_JAVA_PACK_ITEM_MODELS_H
#include "JavaGameSession.h"
#include "../../client/renderer/Textures.h"
#include "../../client/renderer/Tesselator.h"
#include <cmath>
#include <limits>
namespace JavaPackItemModels {
struct Quad {TextureId texture=0;int tint=-1;std::array<std::array<float,5>,4> vertices{};};
using Tint=JavaGameSession::PackModelTint;
struct Model {std::vector<Quad> quads;std::vector<Tint> tints;std::map<std::string,JavaGameSession::PackDisplayTransform> display;std::array<float,3> rotation{{0,0,0}},translation{{0,0,0}},scale{{1,1,1}};};
inline std::map<std::string,Model>& models(){static std::map<std::string,Model> value;return value;}
inline void clear(){models().clear();}
inline bool has(const std::string& id){auto found=models().find(id);return found!=models().end()&&!found->second.quads.empty();}
inline void install(const JavaGameSession::PackItemModel& source,const std::vector<TextureId>& textures){
 Model model;model.tints=source.tints;model.display=source.display;model.rotation=source.rotation;model.translation=source.translation;model.scale=source.scale;
 for(const auto& q:source.quads){if(q.texture<0||size_t(q.texture)>=textures.size())continue;Quad quad;quad.texture=textures[q.texture];quad.tint=q.tint;quad.vertices=q.vertices;model.quads.push_back(quad);}models()[source.id]=std::move(model);
}
inline uint32_t color(const Model& model,int index,const JavaGameSession::Item* item=nullptr){
 if(index<0||size_t(index)>=model.tints.size())return 0xffffff;
 const auto& tint=model.tints[index];if(tint.kind=="constant")return tint.color&0xffffff;
 if(tint.kind=="dye"&&item&&item->hasDyedColor)return item->dyedColor&0xffffff;
 if(tint.kind=="potion"&&item&&item->hasPotionColor)return item->potionColor&0xffffff;
 if(tint.kind=="custom_model_data"&&item&&item->customModelData.present&&tint.index<item->customModelData.colors.size())return uint32_t(item->customModelData.colors[tint.index])&0xffffff;
 return tint.defaultColor&0xffffff;
}
// Preserve authored face order and alpha layering, batching only contiguous
// texture runs. Bound batches below the original tesselator's vertex capacity.
inline void drawGeometry(Textures* textures,const Model& model,const JavaGameSession::Item* item,float brightness,bool exactGuiColor){
 auto& t=Tesselator::instance;bool active=false;TextureId bound=0;size_t quads=0;
 for(const auto& q:model.quads){
  if(!active||q.texture!=bound||quads>=4096){if(active)t.draw();textures->bind(q.texture);t.begin();bound=q.texture;quads=0;active=true;}
  const uint32_t rgb=color(model,q.tint,item);
  if(exactGuiColor)t.color(int((rgb>>16)&255),int((rgb>>8)&255),int(rgb&255),255);
  else t.color(((rgb>>16)&255)/255.f*brightness,((rgb>>8)&255)/255.f*brightness,(rgb&255)/255.f*brightness);
  for(const auto& v:q.vertices)t.vertexUV(v[0]/16.f,v[1]/16.f,v[2]/16.f,v[3],v[4]);++quads;
 }
 if(active)t.draw();
}
inline void restore(GLenum state,bool enabled){if(enabled)glEnable2(state);else glDisable2(state);}
// Separate GUI projection; clears depth only inside the icon's clipped bounds.
inline bool render(Textures* textures,const std::string& id,float x,float y,float size=16,const JavaGameSession::Item* item=nullptr){
 auto found=models().find(id);if(!textures||found==models().end()||found->second.quads.empty()||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(size)||size<=0)return false;
 const auto& model=found->second;
 bool blend=glIsEnabled(GL_BLEND),alpha=glIsEnabled(GL_ALPHA_TEST),depth=glIsEnabled(GL_DEPTH_TEST),cull=glIsEnabled(GL_CULL_FACE),scissor=glIsEnabled(GL_SCISSOR_TEST),lighting=glIsEnabled(GL_LIGHTING),fog=glIsEnabled(GL_FOG);
 GLint oldScissor[4],viewport[4],depthFunc=0,blendSrc=0,blendDst=0,alphaFunc=0;GLboolean depthMask;GLfloat alphaRef=0,clearDepth=1,mv[16],pr[16];glGetIntegerv(GL_SCISSOR_BOX,oldScissor);glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_DEPTH_FUNC,&depthFunc);glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);glGetIntegerv(GL_BLEND_SRC,&blendSrc);glGetIntegerv(GL_BLEND_DST,&blendDst);glGetIntegerv(GL_ALPHA_TEST_FUNC,&alphaFunc);glGetFloatv(GL_ALPHA_TEST_REF,&alphaRef);glGetFloatv(GL_DEPTH_CLEAR_VALUE,&clearDepth);glGetFloatv(GL_MODELVIEW_MATRIX,mv);glGetFloatv(GL_PROJECTION_MATRIX,pr);
 auto project=[&](float px,float py){float view[4]={},clip[4]={},point[4]={px,py,0,1};for(int r=0;r<4;++r)for(int k=0;k<4;++k)view[r]+=mv[k*4+r]*point[k];for(int r=0;r<4;++r)for(int k=0;k<4;++k)clip[r]+=pr[k*4+r]*view[k];return std::array<float,2>{{viewport[0]+(clip[0]/clip[3]+1)*viewport[2]*.5f,viewport[1]+(clip[1]/clip[3]+1)*viewport[3]*.5f}};};
 auto a=project(x,y),b=project(x+size,y+size);if(!std::isfinite(a[0])||!std::isfinite(a[1])||!std::isfinite(b[0])||!std::isfinite(b[1])||std::abs(a[0])>10000000||std::abs(a[1])>10000000||std::abs(b[0])>10000000||std::abs(b[1])>10000000)return false;int left=int(std::floor(std::min(a[0],b[0]))),bottom=int(std::floor(std::min(a[1],b[1]))),right=int(std::ceil(std::max(a[0],b[0]))),top=int(std::ceil(std::max(a[1],b[1])));
 if(scissor){left=std::max(left,oldScissor[0]);bottom=std::max(bottom,oldScissor[1]);right=std::min(right,oldScissor[0]+oldScissor[2]);top=std::min(top,oldScissor[1]+oldScissor[3]);}
 glEnable2(GL_SCISSOR_TEST);glScissor(left,bottom,std::max(0,right-left),std::max(0,top-bottom));glDepthMask(GL_TRUE);glClearDepthf(1);glClear(GL_DEPTH_BUFFER_BIT);glClearDepthf(clearDepth);glEnable2(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glEnable2(GL_BLEND);glBlendFunc2(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glEnable2(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.01f);glDisable2(GL_CULL_FACE);glDisable2(GL_LIGHTING);glDisable2(GL_FOG);
 // Native GUI origin lies on its near plane; retain screen coordinates while
 // providing depth room for authored item geometry. World projections are untouched.
 GLint matrixMode=GL_MODELVIEW;glGetIntegerv(GL_MATRIX_MODE,&matrixMode);GLfloat guiProjection[16];std::copy(pr,pr+16,guiProjection);for(int i: {2,6,10,14})guiProjection[i]*=.001f;
 glMatrixMode(GL_PROJECTION);glPushMatrix2();glLoadMatrixf(guiProjection);glMatrixMode(GL_MODELVIEW);
 glPushMatrix2();glTranslatef2(x+size*.5f,y+size*.5f,0);glScalef2(size,-size,size);glTranslatef2(model.translation[0]/16.f,model.translation[1]/16.f,model.translation[2]/16.f);
 // Java GUI item transformations use XYZ rotation and authored GUI scale.
 glRotatef2(model.rotation[0],1,0,0);glRotatef2(model.rotation[1],0,1,0);glRotatef2(model.rotation[2],0,0,1);glScalef2(model.scale[0],model.scale[1],model.scale[2]);glTranslatef2(-.5f,-.5f,-.5f);
 drawGeometry(textures,model,item,1.f,true);glPopMatrix2();glMatrixMode(GL_PROJECTION);glPopMatrix2();glMatrixMode(matrixMode);
 glDepthFunc(depthFunc);glDepthMask(depthMask);glBlendFunc2(blendSrc,blendDst);glAlphaFunc(alphaFunc,alphaRef);glScissor(oldScissor[0],oldScissor[1],oldScissor[2],oldScissor[3]);restore(GL_BLEND,blend);restore(GL_ALPHA_TEST,alpha);restore(GL_DEPTH_TEST,depth);restore(GL_CULL_FACE,cull);restore(GL_SCISSOR_TEST,scissor);restore(GL_LIGHTING,lighting);restore(GL_FOG,fog);glColor4f2(1,1,1,1);return true;
}

// World draw deliberately retains terrain depth and fog. Physical unit-cube
// size/origin match JavaItemIcons::renderWorld's block path. Authored ground
// transformations are not yet part of the decoded bundle.
inline bool renderWorld(Textures* textures,const std::string& id,float size=.5f,float brightness=1.f,const JavaGameSession::Item* item=nullptr){
 auto found=models().find(id);if(!textures||found==models().end()||found->second.quads.empty()||!std::isfinite(size)||size<=0)return false;
 brightness=std::isfinite(brightness)?std::max(0.f,std::min(1.f,brightness)):0.f;
 const bool blend=glIsEnabled(GL_BLEND),alpha=glIsEnabled(GL_ALPHA_TEST),cull=glIsEnabled(GL_CULL_FACE),lighting=glIsEnabled(GL_LIGHTING);
 GLint sourceBlend=0,destinationBlend=0,alphaFunction=0,depthFunction=0;GLfloat alphaReference=0;glGetIntegerv(GL_BLEND_SRC,&sourceBlend);glGetIntegerv(GL_BLEND_DST,&destinationBlend);glGetIntegerv(GL_ALPHA_TEST_FUNC,&alphaFunction);glGetFloatv(GL_ALPHA_TEST_REF,&alphaReference);glGetIntegerv(GL_DEPTH_FUNC,&depthFunction);
 glEnable2(GL_BLEND);glBlendFunc2(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glEnable2(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.01f);glDisable2(GL_CULL_FACE);glDisable2(GL_LIGHTING);glDepthFunc(GL_LEQUAL);
 glPushMatrix2();glScalef2(size*.5f,size*.5f,size*.5f);glTranslatef2(-.5f,0,-.5f);
 drawGeometry(textures,found->second,item,brightness,false);
 glPopMatrix2();glDepthFunc(depthFunction);glBlendFunc2(sourceBlend,destinationBlend);glAlphaFunc(alphaFunction,alphaReference);restore(GL_BLEND,blend);restore(GL_ALPHA_TEST,alpha);restore(GL_CULL_FACE,cull);restore(GL_LIGHTING,lighting);glColor4f2(1,1,1,1);return true;
}

// ItemDisplay NONE is a full unit model centred on all three axes, unlike a
// small dropped block. Verified official 1.21.11 ItemTransform NO_TRANSFORM.
inline bool renderDisplay(Textures* textures,const std::string& id,float brightness=1.f,const JavaGameSession::Item* item=nullptr){
 if(!textures||!has(id))return false;
 glPushMatrix2();glTranslatef2(0,-.5f,0);const bool drawn=renderWorld(textures,id,2.f,brightness,item);glPopMatrix2();return drawn;
}
// Match official ItemTransform: translation, XYZ rotation, scale, cube centre.
// Missing left-hand contexts inherit the corresponding right-hand transform.
inline bool renderContext(Textures* textures,const std::string& id,const std::string& context,float brightness=1.f,const JavaGameSession::Item* item=nullptr){
 auto found=models().find(id);if(!textures||found==models().end()||found->second.quads.empty())return false;
 const auto& entries=found->second.display;auto transform=entries.find(context);
 const bool left=context=="firstperson_lefthand"||context=="thirdperson_lefthand";
 if(transform==entries.end()&&left)transform=entries.find(context=="firstperson_lefthand"?"firstperson_righthand":"thirdperson_righthand");
 glPushMatrix2();
 if(transform!=entries.end()){
  const auto& value=transform->second;const float sign=left?-1.f:1.f;
  glTranslatef2(sign*value.translation[0]/16.f,value.translation[1]/16.f,value.translation[2]/16.f);
  glRotatef2(value.rotation[0],1,0,0);glRotatef2(sign*value.rotation[1],0,1,0);glRotatef2(sign*value.rotation[2],0,0,1);
  glScalef2(value.scale[0],value.scale[1],value.scale[2]);
 }
 const bool drawn=renderDisplay(textures,id,brightness,item);glPopMatrix2();return drawn;
}

inline float groundMinimumY(const Model& model){
 auto found=model.display.find("ground");if(found==model.display.end())return 0;
 const auto& a=found->second;const float radians=3.14159265358979323846f/180.f;
 const float sx=std::sin(a.rotation[0]*radians),cx=std::cos(a.rotation[0]*radians),sy=std::sin(a.rotation[1]*radians),cy=std::cos(a.rotation[1]*radians),sz=std::sin(a.rotation[2]*radians),cz=std::cos(a.rotation[2]*radians);
 float minimum=std::numeric_limits<float>::infinity();
 for(const auto& q:model.quads)for(const auto& v:q.vertices){const float x=(v[0]/16.f-.5f)*a.scale[0],y=(v[1]/16.f-.5f)*a.scale[1],z=(v[2]/16.f-.5f)*a.scale[2];const float rzX=cz*x-sz*y,rzY=sz*x+cz*y,ryZ=-sy*rzX+cy*z;minimum=std::min(minimum,cx*rzY-sx*ryZ+a.translation[1]/16.f);}
 return std::isfinite(minimum)?minimum:0;
}
inline bool renderGround(Textures* textures,const std::string& id,float brightness=1.f,const JavaGameSession::Item* item=nullptr){
 auto found=models().find(id);if(found==models().end())return false;
 if(!found->second.display.count("ground"))return renderWorld(textures,id,.5f,brightness,item);
 // 1.21.11 positions the transformed mesh bottom 1/16 above the bob origin.
 glPushMatrix2();glTranslatef2(0,.0625f-groundMinimumY(found->second),0);const bool drawn=renderContext(textures,id,"ground",brightness,item);glPopMatrix2();return drawn;
}

}
#endif
