#include "JavaCrossbowAnimation.h"
#include "JavaItemIcons.h"
#include "JavaShieldModel.h"
#include "JavaCompassFrames.h"
#include "JavaBlockModels.h"
#include "client/model/geom/ModelPart.h"
#include "client/model/geom/Cube.h"
#include "client/renderer/Tesselator.h"
#include "client/renderer/Textures.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
struct JavaItemIcon {const char* name;float uv[4];int state;};
static const JavaItemIcon icons[]={
#include "JavaItemIcons.inc"
};
namespace {
const JavaItemIcon* find(const std::string& name){std::string id=name.find(':')==std::string::npos?"minecraft:"+name:name;auto* end=icons+sizeof(icons)/sizeof(*icons);auto* icon=std::lower_bound(icons,end,id,[](const JavaItemIcon& icon,const std::string& id){return icon.name<id;});return icon!=end&&id==icon->name?icon:nullptr;}

const char* headTexture(const std::string& id){
 if(id=="minecraft:zombie_head")return "mob/zombie.png";
 if(id=="minecraft:skeleton_skull")return "mob/skeleton.png";
 if(id=="minecraft:creeper_head")return "mob/creeper.png";
 if(id=="minecraft:player_head")return "mob/char.png";
 return nullptr;
}
void headGeometry(const std::string& id,int height=32){
 ModelPart part;part.useCustomTextureDimensions=true;part.setTexSize(64,height);Cube base(&part,0,0,-4,-4,-4,8,8,8,0);auto& t=Tesselator::instance;t.begin();base.compile(t,1.f/8.f);t.draw();
 if(id=="minecraft:player_head"){Cube outer(&part,32,0,-4,-4,-4,8,8,8,.25f);t.begin();outer.compile(t,1.f/8.f);t.draw();}
}

#include "JavaPotionSpriteAlpha.inc"
bool isPotion(const std::string& id){return id=="minecraft:potion"||id=="minecraft:splash_potion"||id=="minecraft:lingering_potion";}
const unsigned char* bottleAlpha(const std::string& id){return id=="minecraft:splash_potion"?splash_potionAlpha:id=="minecraft:lingering_potion"?lingering_potionAlpha:potionAlpha;}
std::string bottleTexture(const std::string& id){return "java-items/"+id.substr(id.find(':')+1)+".png";}
void potionColor(Tesselator& t,const JavaGameSession::Item* item,bool overlay,float brightness){
 const unsigned rgb=overlay?(item&&item->hasPotionColor?item->potionColor:0x385dc6u):0xffffffu;
 t.color(((rgb>>16)&255)/255.f*brightness,((rgb>>8)&255)/255.f*brightness,(rgb&255)/255.f*brightness);
}
void potionGui(Textures* textures,const std::string& id,const JavaGameSession::Item* item,float x,float y,float size){
 auto& t=Tesselator::instance;
 for(int layer=0;layer<2;++layer){textures->loadAndBindTexture(layer?bottleTexture(id):"java-items/potion_overlay.png");t.begin();potionColor(t,item,layer==0,1);t.vertexUV(x,y+size,0,0,1);t.vertexUV(x+size,y+size,0,1,1);t.vertexUV(x+size,y,0,1,0);t.vertexUV(x,y,0,0,0);t.draw();}
}
void potionWorld(Textures* textures,const std::string& id,const JavaGameSession::Item* item,float size,float brightness){
 auto& t=Tesselator::instance;
 for(int layer=0;layer<2;++layer){
  const auto* alpha=layer?bottleAlpha(id):potion_overlayAlpha;
  textures->loadAndBindTexture(layer?bottleTexture(id):"java-items/potion_overlay.png");
  const float depth=size/32.f+layer*size*.0001f;
  t.begin();potionColor(t,item,layer==0,brightness);
  t.vertexUV(-size*.5f,0,depth,0,1);t.vertexUV(size*.5f,0,depth,1,1);t.vertexUV(size*.5f,size,depth,1,0);t.vertexUV(-size*.5f,size,depth,0,0);
  t.vertexUV(size*.5f,0,-depth,1,1);t.vertexUV(-size*.5f,0,-depth,0,1);t.vertexUV(-size*.5f,size,-depth,0,0);t.vertexUV(size*.5f,size,-depth,1,0);
  // Extrude alpha boundaries, preserving bottle holes and each colored layer.
  for(int y=0;y<16;++y)for(int x=0;x<16;++x)if(alpha[y*16+x]){
   const float x0=(x/16.f-.5f)*size,x1=((x+1)/16.f-.5f)*size,y0=(1-(y+1)/16.f)*size,y1=(1-y/16.f)*size,u=(x+.5f)/16.f,v=(y+.5f)/16.f;
   auto edge=[&](float ax,float ay,float bx,float by){t.vertexUV(ax,ay,-depth,u,v);t.vertexUV(bx,by,-depth,u,v);t.vertexUV(bx,by,depth,u,v);t.vertexUV(ax,ay,depth,u,v);};
   if(x==0||!alpha[y*16+x-1])edge(x0,y0,x0,y1);
   if(x==15||!alpha[y*16+x+1])edge(x1,y1,x1,y0);
   if(y==0||!alpha[(y-1)*16+x])edge(x0,y1,x1,y1);
   if(y==15||!alpha[(y+1)*16+x])edge(x1,y0,x0,y0);
  }
  t.draw();
 }
}
void restore(GLenum state,bool enabled){if(enabled)glEnable2(state);else glDisable2(state);}
}
bool JavaItemIcons::isBlock(const std::string& id){const auto* icon=find(id);return icon&&icon->state>=0;}
bool JavaItemIcons::has(const std::string& id){const auto* icon=find(id);return icon&&(headTexture(id)||icon->state<0||(icon->uv[2]>icon->uv[0]&&icon->uv[3]>icon->uv[1])||JavaBlockModels::layer(static_cast<uint16_t>(icon->state))>=0);}
bool JavaItemIcons::render(Textures* textures,const std::string& id,float x,float y,float size,const JavaGameSession::Item* item){
 const auto* icon=find(id);if(!textures||!icon||!std::isfinite(size)||size<=0)return false;
 const bool bakedGui=icon->state>=0&&icon->uv[2]>icon->uv[0]&&icon->uv[3]>icon->uv[1];
 if(id=="minecraft:player_head"&&(!item||(item->hasProfile&&(item->profileSkinTexture.empty()||(item->profileSkinHeight!=32&&item->profileSkinHeight!=64)))))return false;
 if(icon->state>=0&&!bakedGui&&!headTexture(id)&&JavaBlockModels::layer(static_cast<uint16_t>(icon->state))<0)return false;
 const bool shield=id=="minecraft:shield";
 const bool geometryGui=shield||headTexture(id)||(icon->state>=0&&!bakedGui);GLint matrixMode=GL_MODELVIEW;glGetIntegerv(GL_MATRIX_MODE,&matrixMode);
 bool blend=glIsEnabled(GL_BLEND),alpha=glIsEnabled(GL_ALPHA_TEST),depth=glIsEnabled(GL_DEPTH_TEST),cull=glIsEnabled(GL_CULL_FACE),scissor=glIsEnabled(GL_SCISSOR_TEST),lighting=glIsEnabled(GL_LIGHTING),fog=glIsEnabled(GL_FOG);
 GLfloat threshold=0;GLint function=0,src=0,dst=0;glGetFloatv(GL_ALPHA_TEST_REF,&threshold);glGetIntegerv(GL_ALPHA_TEST_FUNC,&function);glGetIntegerv(GL_BLEND_SRC,&src);glGetIntegerv(GL_BLEND_DST,&dst);
 GLint oldScissor[4],viewport[4],depthFunc=0;GLboolean depthMask;GLfloat clearDepth=1,mv[16],pr[16];glGetIntegerv(GL_SCISSOR_BOX,oldScissor);glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_DEPTH_FUNC,&depthFunc);glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);glGetFloatv(GL_DEPTH_CLEAR_VALUE,&clearDepth);
 if(geometryGui){
  glGetFloatv(GL_MODELVIEW_MATRIX,mv);glGetFloatv(GL_PROJECTION_MATRIX,pr);
  auto project=[&](float px,float py){float view[4]={},clip[4]={},point[4]={px,py,0,1};for(int r=0;r<4;++r)for(int k=0;k<4;++k)view[r]+=mv[k*4+r]*point[k];for(int r=0;r<4;++r)for(int k=0;k<4;++k)clip[r]+=pr[k*4+r]*view[k];return std::array<float,2>{{viewport[0]+(clip[0]/clip[3]+1)*viewport[2]*.5f,viewport[1]+(clip[1]/clip[3]+1)*viewport[3]*.5f}};};
  auto a=project(x,y),b=project(x+size,y+size);if(!std::isfinite(a[0])||!std::isfinite(a[1])||!std::isfinite(b[0])||!std::isfinite(b[1])||std::abs(a[0])>10000000||std::abs(a[1])>10000000||std::abs(b[0])>10000000||std::abs(b[1])>10000000)return false;
  int left=int(std::floor(std::min(a[0],b[0]))),bottom=int(std::floor(std::min(a[1],b[1]))),right=int(std::ceil(std::max(a[0],b[0]))),top=int(std::ceil(std::max(a[1],b[1])));
  if(scissor){left=std::max(left,oldScissor[0]);bottom=std::max(bottom,oldScissor[1]);right=std::min(right,oldScissor[0]+oldScissor[2]);top=std::min(top,oldScissor[1]+oldScissor[3]);}
  glEnable2(GL_SCISSOR_TEST);glScissor(left,bottom,std::max(0,right-left),std::max(0,top-bottom));glDepthMask(GL_TRUE);glClearDepthf(1);glClear(GL_DEPTH_BUFFER_BIT);glClearDepthf(clearDepth);glDepthFunc(GL_LEQUAL);
  // TCPE GUI puts its item origin on the near plane (z=-2000). A rotated
  // pixel-sized cube otherwise loses its front half to clipping and looks rounded.
  // Compress clip-space depth only; retain authored XY placement and clip W.
  GLfloat iconProjection[16];std::copy(pr,pr+16,iconProjection);for(int column=0;column<4;++column)iconProjection[column*4+2]*=.001f;
  glMatrixMode(GL_PROJECTION);glPushMatrix2();glLoadMatrixf(iconProjection);glMatrixMode(GL_MODELVIEW);
 }
 glDisable2(GL_LIGHTING);glDisable2(GL_FOG);
 glEnable2(GL_BLEND);glBlendFunc2(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glEnable2(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.1f);glDisable2(GL_CULL_FACE);
 auto& t=Tesselator::instance;bool drawn=true;
 if(isPotion(id)){glDisable2(GL_DEPTH_TEST);potionGui(textures,id,item,x,y,size);}
 else if(shield){glEnable2(GL_DEPTH_TEST);glPushMatrix2();glTranslatef2(x+size*.5f,y+size*.5f,0);glScalef2(size,-size,size);drawn=JavaShieldModel::renderContext(textures,"gui",1.f,item);glPopMatrix2();}
 else if(headTexture(id)){textures->loadAndBindTexture(id=="minecraft:player_head"&&item&&item->hasProfile?item->profileSkinTexture:headTexture(id));glEnable2(GL_DEPTH_TEST);glPushMatrix2();glTranslatef2(x+size*.5f,y+size*.5f,0);glScalef2(size*.65f,size*.65f,size*.65f);glRotatef2(30,1,0,0);glRotatef2(225,0,1,0);t.color(1.f,1.f,1.f);headGeometry(id,id=="minecraft:player_head"&&item&&item->hasProfile?item->profileSkinHeight:32);glPopMatrix2();}
 else if(icon->state<0||bakedGui){glDisable2(GL_DEPTH_TEST);textures->loadAndBindTexture("java-items.png");t.begin();t.color(1.f,1.f,1.f);t.vertexUV(x,y+size,0,icon->uv[0],icon->uv[3]);t.vertexUV(x+size,y+size,0,icon->uv[2],icon->uv[3]);t.vertexUV(x+size,y,0,icon->uv[2],icon->uv[1]);t.vertexUV(x,y,0,icon->uv[0],icon->uv[1]);t.draw();}
 else {textures->loadAndBindTexture(JavaBlockModels::atlasTexture());glEnable2(GL_DEPTH_TEST);glPushMatrix2();glTranslatef2(x+size*.5f,y+size*.5f,0);glScalef2(size*.6f,-size*.6f,size*.6f);glRotatef2(30,1,0,0);glRotatef2(45,0,1,0);glTranslatef2(-.5f,-.5f,-.5f);t.begin();drawn=JavaBlockModels::tesselateDisplay(t,static_cast<uint16_t>(icon->state));t.draw();glPopMatrix2();}
 if(geometryGui){glMatrixMode(GL_PROJECTION);glPopMatrix2();glMatrixMode(matrixMode);}
 glDepthFunc(depthFunc);glDepthMask(depthMask);glScissor(oldScissor[0],oldScissor[1],oldScissor[2],oldScissor[3]);restore(GL_SCISSOR_TEST,scissor);restore(GL_LIGHTING,lighting);restore(GL_FOG,fog);glAlphaFunc(function,threshold);glBlendFunc2(src,dst);restore(GL_BLEND,blend);restore(GL_ALPHA_TEST,alpha);restore(GL_DEPTH_TEST,depth);restore(GL_CULL_FACE,cull);glColor4f2(1,1,1,1);return drawn;
}

// Original TCPE generated-item geometry: textured front/back and one
// alpha-tested side strip per source pixel, with native 1/16 sprite thickness.
static void nativeSpriteVolume(Tesselator& t,const JavaItemIcon& icon,float size,int columns,int rows){
 const float x0=-size*.5f,x1=size*.5f,y0=0,y1=size,z0=0,z1=-size/16.f;
 const float u0=icon.uv[0],u1=icon.uv[2],v0=icon.uv[1],v1=icon.uv[3];
 auto q=[&](float ax,float ay,float az,float au,float av,float bx,float by,float bz,float bu,float bv,float cx,float cy,float cz,float cu,float cv,float dx,float dy,float dz,float du,float dv){t.vertexUV(ax,ay,az,au,av);t.vertexUV(bx,by,bz,bu,bv);t.vertexUV(cx,cy,cz,cu,cv);t.vertexUV(dx,dy,dz,du,dv);};
 q(x0,y0,z0,u0,v1,x1,y0,z0,u1,v1,x1,y1,z0,u1,v0,x0,y1,z0,u0,v0);
 q(x0,y1,z1,u0,v0,x1,y1,z1,u1,v0,x1,y0,z1,u1,v1,x0,y0,z1,u0,v1);
 for(int i=0;i<columns;++i){const float u=u0+(u1-u0)*(i+.5f)/columns,lo=x0+size*i/columns,hi=x0+size*(i+1)/columns;q(lo,y0,z1,u,v1,lo,y0,z0,u,v1,lo,y1,z0,u,v0,lo,y1,z1,u,v0);q(hi,y1,z1,u,v0,hi,y1,z0,u,v0,hi,y0,z0,u,v1,hi,y0,z1,u,v1);}
 for(int i=0;i<rows;++i){const float v=v1+(v0-v1)*(i+.5f)/rows,lo=size*i/rows,hi=size*(i+1)/rows;q(x0,hi,z0,u0,v,x1,hi,z0,u1,v,x1,hi,z1,u1,v,x0,hi,z1,u0,v);q(x1,lo,z0,u1,v,x0,lo,z0,u0,v,x0,lo,z1,u0,v,x1,lo,z1,u1,v);}
}

bool JavaItemIcons::renderWorld(Textures* textures,const std::string& id,float size,float brightness,const JavaGameSession::Item* item){
 brightness=std::isfinite(brightness)?std::max(0.f,std::min(1.f,brightness)):0.f;
 const auto* icon=find(id);if(!textures||!icon||icon->state==-2||!std::isfinite(size)||size<=0)return false;
 if(id=="minecraft:player_head"&&(!item||(item->hasProfile&&(item->profileSkinTexture.empty()||(item->profileSkinHeight!=32&&item->profileSkinHeight!=64)))))return false;
 if(icon->state>=0&&!headTexture(id)&&JavaBlockModels::layer(static_cast<uint16_t>(icon->state))<0)return false;
 const bool blend=glIsEnabled(GL_BLEND),alpha=glIsEnabled(GL_ALPHA_TEST),cull=glIsEnabled(GL_CULL_FACE);GLfloat threshold=0;GLint function=0,src=0,dst=0;glGetIntegerv(GL_BLEND_SRC,&src);glGetIntegerv(GL_BLEND_DST,&dst);glGetFloatv(GL_ALPHA_TEST_REF,&threshold);glGetIntegerv(GL_ALPHA_TEST_FUNC,&function);
 glEnable2(GL_BLEND);glBlendFunc2(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glEnable2(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.1f);glDisable2(GL_CULL_FACE);
 auto& t=Tesselator::instance;bool drawn=true;
 if(isPotion(id)){potionWorld(textures,id,item,size,brightness);}
 else if(headTexture(id)){textures->loadAndBindTexture(id=="minecraft:player_head"&&item&&item->hasProfile?item->profileSkinTexture:headTexture(id));glPushMatrix2();glTranslatef2(0,size*.5f,0);glScalef2(size,-size,size);glRotatef2(180,0,1,0);t.color(brightness,brightness,brightness);headGeometry(id,id=="minecraft:player_head"&&item&&item->hasProfile?item->profileSkinHeight:32);glPopMatrix2();}
 else if(icon->state<0){const auto texture=textures->loadAndBindTexture("java-items.png");const auto* image=textures->getTemporaryTextureData(texture);const int columns=std::max(1,std::min(256,int(std::lround((icon->uv[2]-icon->uv[0])*(image?image->w:1024))))),rows=std::max(1,std::min(256,int(std::lround((icon->uv[3]-icon->uv[1])*(image?image->h:1024)))));t.begin();t.color(brightness,brightness,brightness);nativeSpriteVolume(t,*icon,size,columns,rows);t.draw();}
 else{textures->loadAndBindTexture(JavaBlockModels::atlasTexture());glPushMatrix2();glScalef2(size*.5f,size*.5f,size*.5f);glTranslatef2(-.5f,0,-.5f);t.begin();drawn=JavaBlockModels::tesselateDisplay(t,static_cast<uint16_t>(icon->state),brightness);t.draw();glPopMatrix2();}
 glAlphaFunc(function,threshold);glBlendFunc2(src,dst);restore(GL_BLEND,blend);restore(GL_ALPHA_TEST,alpha);restore(GL_CULL_FACE,cull);glColor4f2(1,1,1,1);return drawn;
}

std::string JavaItemIcons::animatedResource(const JavaGameSession& session,const JavaGameSession::Item& item,double x,double z,double yaw,unsigned long long tick){return animatedResource(session,item,x,z,yaw,tick,0,true);}
std::string JavaItemIcons::animatedResource(const JavaGameSession& session,const JavaGameSession::Item& item,double x,double z,double yaw,unsigned long long tick,int viewerId,bool smoothHeading){
 if(item.resourceId=="minecraft:crossbow"){
  const bool selected=&item==&session.inventory.held||(session.inventory.selected<9&& &item==&session.inventory.hotbar[session.inventory.selected]);
  const bool active=session.itemUseActive&&((session.itemUseOffHand&& &item==&session.inventory.offHand)||(!session.itemUseOffHand&&selected));
  return JavaCrossbowAnimation::resource(active,float(session.itemUseTicks),float(session.itemUseChargeDuration),item.chargedProjectile);
 }
 const bool recovery=item.resourceId=="minecraft:recovery_compass";
 if(item.resourceId!="minecraft:compass"&&!recovery)return item.resourceId;
 const auto& target=recovery?session.compass.lastDeath:(item.hasLodestoneTracker?item.lodestoneTarget:session.compass.defaultSpawn);
 auto wrap=[](double value){return value-std::floor(value);};
 struct Wobble {unsigned long long tick=~0ULL;double rotation=0,velocity=0;};
 struct Pair {Wobble heading,noTarget;};
 struct Cache {const JavaGameSession* session=nullptr;uint64_t scope=0;unsigned world=0;std::string dimension,phase;std::map<int,std::array<Pair,3>> viewers;};
 static Cache cache;
 if(cache.session!=&session||cache.scope!=session.chatScope||cache.world!=session.worldRevision||cache.dimension!=session.compass.dimension||cache.phase!=session.phase){cache=Cache{};cache.session=&session;cache.scope=session.chatScope;cache.world=session.worldRevision;cache.dimension=session.compass.dimension;cache.phase=session.phase;}
 // Vanilla owns separate CompassAngleState instances for spawn/lodestone/recovery.
 // Keep viewer headings independent, and never feed no-target spin into a heading.
 auto& pair=cache.viewers[viewerId][recovery?2:item.hasLodestoneTracker?1:0];
 const double dx=target.position[0]+.5-x,dz=target.position[2]+.5-z;
 const bool valid=target.valid&&target.dimension==session.compass.dimension&&std::isfinite(x)&&std::isfinite(z)&&std::isfinite(yaw)&&dx*dx+dz*dz>=1e-5;
 const double heading=valid?wrap(.75-yaw/360.):wrap(std::sin(double(tick)*12.9898+78.233)*43758.5453);
 auto& state=valid?pair.heading:pair.noTarget;
 if(tick<state.tick&&state.tick!=~0ULL)state=Wobble{};
 if(state.tick!=tick){state.velocity=(state.velocity+(wrap(heading-state.rotation+.5)-.5)*.1)*.8;state.rotation=wrap(state.rotation+state.velocity);state.tick=tick;}
 const double rotation=valid&&!smoothHeading?heading:state.rotation;
 const double angle=wrap(rotation+(valid?std::atan2(dz,dx)/6.2831854820251465:0.));
 return JavaCompassFrames::resource(angle,recovery);
}
