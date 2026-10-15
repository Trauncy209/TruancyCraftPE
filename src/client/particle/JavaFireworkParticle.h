#ifndef TCPE_JAVA_FIREWORK_PARTICLE_H
#define TCPE_JAVA_FIREWORK_PARTICLE_H
#include "Particle.h"
#include "../renderer/Textures.h"
#include "../../network/java/JavaGameSession.h"
#include "../../network/java/JavaFireworkGeometry.h"
#include <chrono>
namespace JavaFireworkParticles {
inline unsigned& live(){static unsigned count=0;return count;}
struct Budget {Budget(){++live();}~Budget(){--live();}};
inline bool canSpawn(){return live()<16384;}
inline uint64_t freshSeed(){static uint64_t sequence=0;return uint64_t(std::chrono::steady_clock::now().time_since_epoch().count())+(++sequence*0x9e3779b97f4a7c15ULL);}
inline void quad(Textures* textures,Tesselator& t,const std::string& texture,float px,float py,float pz,float size,float r,float g,float b,float alpha,float xa,float ya,float za,float xa2,float za2){
 GLint src,dst,alphaFunc,binding;GLfloat alphaRef,color[4];GLboolean depthWrite;glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding);glGetIntegerv(GL_BLEND_SRC,&src);glGetIntegerv(GL_BLEND_DST,&dst);glGetIntegerv(GL_ALPHA_TEST_FUNC,&alphaFunc);glGetFloatv(GL_ALPHA_TEST_REF,&alphaRef);glGetFloatv(GL_CURRENT_COLOR,color);glGetBooleanv(GL_DEPTH_WRITEMASK,&depthWrite);const bool textured=glIsEnabled(GL_TEXTURE_2D),blend=glIsEnabled(GL_BLEND),cull=glIsEnabled(GL_CULL_FACE),test=glIsEnabled(GL_ALPHA_TEST),lighting=glIsEnabled(GL_LIGHTING);
 textures->loadAndBindTexture(texture);glEnable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);glBlendFunc2(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDisable2(GL_CULL_FACE);glDisable2(GL_LIGHTING);glEnable2(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,1.f/255.f);glDepthMask(GL_FALSE);
 const float ax=xa*size,ay=ya*size,az=za*size,bx=xa2*size,bz=za2*size;t.begin();t.color(r,g,b,alpha);t.vertexUV(px-ax-bx,py-ay,pz-az-bz,1,1);t.vertexUV(px-ax+bx,py+ay,pz-az+bz,1,0);t.vertexUV(px+ax+bx,py+ay,pz+az+bz,0,0);t.vertexUV(px+ax-bx,py-ay,pz+az-bz,0,1);t.draw();
 textures->bind(binding);if(textured)glEnable2(GL_TEXTURE_2D);else glDisable2(GL_TEXTURE_2D);glDepthMask(depthWrite);glBlendFunc2(src,dst);glAlphaFunc(alphaFunc,alphaRef);if(blend)glEnable2(GL_BLEND);else glDisable2(GL_BLEND);if(cull)glEnable2(GL_CULL_FACE);else glDisable2(GL_CULL_FACE);if(test)glEnable2(GL_ALPHA_TEST);else glDisable2(GL_ALPHA_TEST);if(lighting)glEnable2(GL_LIGHTING);else glDisable2(GL_LIGHTING);glColor4f2(color[0],color[1],color[2],color[3]);
}
class Spark:public Particle {
 Budget budget;
 Textures* textures;ParticleEngine* engine;bool trail=false,twinkle=false,hasFade=false;float fadeR=0,fadeG=0,fadeB=0,quadSize=0,opacity=1;int sprite=7;
public:
 Spark(ParticleEngine* engine,Textures* textures,Level* level,float x,float y,float z,const JavaFireworkGeometry::Vector& motion):Particle(level,x,y,z,0,0,0),textures(textures),engine(engine){JavaFireworkRandom random(freshSeed());age=0;quadSize=(.1f*(random.nextFloat()*.5f+.5f)*2)*.75f;lifetime=48+random.nextInt(12);xd=motion[0];yd=motion[1];zd=motion[2];rCol=gCol=bCol=1;noPhysics=false;setSize(.2f,.2f);}
 int getParticleTexture()override{return ParticleEngine::ENTITY_PARTICLE_TEXTURE;}
 void configure(uint32_t color,bool trailing,bool flashing){rCol=((color>>16)&255)/255.f;gCol=((color>>8)&255)/255.f;bCol=(color&255)/255.f;trail=trailing;twinkle=flashing;opacity=.99f;}
 void fade(uint32_t color){hasFade=true;fadeR=((color>>16)&255)/255.f;fadeG=((color>>8)&255)/255.f;fadeB=(color&255)/255.f;}
 void tick()override{
  xo=x;yo=y;zo=z;if(age++>=lifetime){remove();return;}yd-=.04*double(.1f);move(xd,yd,zd);xd*=double(.91f);yd*=double(.91f);zd*=double(.91f);if(onGround){xd*=double(.7f);zd*=double(.7f);}
  sprite=7-JavaFireworkGeometry::sprite(age,lifetime);
  if(age>lifetime/2){opacity=JavaFireworkGeometry::alpha(age,lifetime);if(hasFade){rCol+=(fadeR-rCol)*.2f;gCol+=(fadeG-gCol)*.2f;bCol+=(fadeB-bCol)*.2f;}}
  if(trail&&engine&&canSpawn()&&age<lifetime/2&&(age+lifetime)%2==0){auto* child=new Spark(engine,textures,level,x,y,z,{{0,0,0}});child->opacity=.99f;child->rCol=rCol;child->gCol=gCol;child->bCol=bCol;child->age=child->lifetime/2;child->twinkle=twinkle;child->hasFade=hasFade;child->fadeR=fadeR;child->fadeG=fadeG;child->fadeB=fadeB;engine->add(child);}
 }
 void render(Tesselator& t,float partial,float xa,float ya,float za,float xa2,float za2)override{if(!JavaFireworkGeometry::visible(age,lifetime,twinkle))return;quad(textures,t,"particle/java-firework-spark-"+std::to_string(sprite)+".png",xo+(x-xo)*partial-xOff,yo+(y-yo)*partial-yOff,zo+(z-zo)*partial-zOff,quadSize,rCol,gCol,bCol,opacity,xa,ya,za,xa2,za2);}
};
class Flash:public Particle {
 Budget budget;
 Textures* textures;
public:
 Flash(Textures* textures,Level* level,float x,float y,float z,uint32_t color):Particle(level,x,y,z,0,0,0),textures(textures){age=0;lifetime=4;rCol=((color>>16)&255)/255.f;gCol=((color>>8)&255)/255.f;bCol=(color&255)/255.f;xd=yd=zd=0;noPhysics=true;}
 int getParticleTexture()override{return ParticleEngine::ENTITY_PARTICLE_TEXTURE;}
 void tick()override{xo=x;yo=y;zo=z;if(age++>=lifetime)remove();}
 void render(Tesselator& t,float partial,float xa,float ya,float za,float xa2,float za2)override{quad(textures,t,"particle/java-firework-flash.png",x-xOff,y-yOff,z-zOff,JavaFireworkGeometry::flashSize(age,partial),rCol,gCol,bCol,JavaFireworkGeometry::flashAlpha(age,partial),xa,ya,za,xa2,za2);}
};
class Starter:public Particle {
 Budget budget;
 ParticleEngine* engine;Textures* textures;std::vector<JavaGameSession::FireworkExplosion> explosions;JavaFireworkRandom random;JavaFireworkGeometry::Vector motion;
public:
 Starter(ParticleEngine* engine,Textures* textures,Level* level,const JavaGameSession::VisualEffect& effect):Particle(level,effect.position[0],effect.position[1],effect.position[2],0,0,0),engine(engine),textures(textures),explosions(effect.explosions),random(freshSeed()),motion(effect.velocity){age=0;lifetime=int(explosions.size())*2-1;for(const auto& e:explosions)if(e.twinkle){lifetime+=15;break;}noPhysics=true;xd=yd=zd=0;}
 int getParticleTexture()override{return ParticleEngine::ENTITY_PARTICLE_TEXTURE;}
 void render(Tesselator&,float,float,float,float,float,float)override{}
 void tick()override{
  if(age%2==0&&size_t(age/2)<explosions.size()){
   const auto& e=explosions[age/2];const std::vector<uint32_t> colors=e.colors.empty()?std::vector<uint32_t>{1973019}:e.colors;
   JavaFireworkGeometry::velocities(e.shape,random,motion,[&](const JavaFireworkGeometry::Vector& v){if(!canSpawn())return;auto* spark=new Spark(engine,textures,level,x,y,z,v);spark->configure(colors[random.nextInt(int(colors.size()))],e.trail,e.twinkle);if(!e.fadeColors.empty())spark->fade(e.fadeColors[random.nextInt(int(e.fadeColors.size()))]);engine->add(spark);});if(canSpawn())engine->add(new Flash(textures,level,x,y,z,colors[0]));
  }
  if(++age>lifetime)remove();
 }
};
inline Spark* rocketTrail(ParticleEngine* engine,Textures* textures,Level* level,const JavaGameSession::VisualEffect& effect){if(!canSpawn())return nullptr;JavaFireworkRandom random(freshSeed());return new Spark(engine,textures,level,effect.position[0],effect.position[1],effect.position[2],{{random.nextGaussian()*.05,-effect.velocity[1]*.5,random.nextGaussian()*.05}});}
}
#endif
