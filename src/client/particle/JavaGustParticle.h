#ifndef TCPE_JAVA_GUST_PARTICLE_H
#define TCPE_JAVA_GUST_PARTICLE_H
#include "Particle.h"
#include "../renderer/Textures.h"
#include "../../util/Random.h"
#include <algorithm>
// Canonical Java1.21.11 GustParticle: stationary, fullbright, 12-15 ticks,
// twelve age-selected frames. No original offline particle is modified.
class JavaGustParticle : public Particle {
 Textures* textures;
 float quadSize;
public:
 JavaGustParticle(Textures* textures,Level* level,float x,float y,float z,float scale=1)
 :Particle(level,x,y,z,0,0,0),textures(textures),quadSize(scale){
  age=0;lifetime=12+sharedRandom.nextInt(4);rCol=gCol=bCol=1;noPhysics=true;setSize(1,1);
 }
 void tick()override{xo=x;yo=y;zo=z;if(age++>=lifetime)remove();}
 int getParticleTexture()override{return ParticleEngine::ENTITY_PARTICLE_TEXTURE;}
 static int frame(int age,int lifetime){return std::max(0,std::min(11,age*12/(std::max(1,lifetime)+1)));}
 void render(Tesselator& t,float partial,float xa,float ya,float za,float xa2,float za2)override{
  textures->loadAndBindTexture("particle/java-gust-"+std::to_string(frame(age,lifetime))+".png");
  const float px=xo+(x-xo)*partial-xOff,py=yo+(y-yo)*partial-yOff,pz=zo+(z-zo)*partial-zOff;
  const float ax=xa*quadSize,ay=ya*quadSize,az=za*quadSize,bx=xa2*quadSize,bz=za2*quadSize;
  t.begin();t.color(1.f,1.f,1.f,1.f);
  t.vertexUV(px-ax-bx,py-ay,pz-az-bz,1,1);t.vertexUV(px-ax+bx,py+ay,pz-az+bz,1,0);
  t.vertexUV(px+ax+bx,py+ay,pz+az+bz,0,0);t.vertexUV(px+ax-bx,py-ay,pz+az-bz,0,1);t.draw();
 }
};
class JavaGustEmitter : public Particle {
 ParticleEngine* engine;Textures* textures;float spread;int delay;
public:
 JavaGustEmitter(ParticleEngine* engine,Textures* textures,Level* level,float x,float y,float z,bool large)
 :Particle(level,x,y,z,0,0,0),engine(engine),textures(textures),spread(large?3:1),delay(large?0:2){age=0;lifetime=large?7:3;noPhysics=true;}
 int getParticleTexture()override{return ParticleEngine::ENTITY_PARTICLE_TEXTURE;}
 void render(Tesselator&,float,float,float,float,float,float)override{}
 void tick()override{
  if(age%(delay+1)==0)for(int i=0;i<3;++i)engine->add(new JavaGustParticle(textures,level,x+(sharedRandom.nextDouble()-sharedRandom.nextDouble())*spread,y+(sharedRandom.nextDouble()-sharedRandom.nextDouble())*spread,z+(sharedRandom.nextDouble()-sharedRandom.nextDouble())*spread));
  if(++age==lifetime)remove();
 }
};
#endif
