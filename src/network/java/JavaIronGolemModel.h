#ifndef TCPE_JAVA_IRON_GOLEM_MODEL_H
#define TCPE_JAVA_IRON_GOLEM_MODEL_H
#include "client/model/Model.h"
#include "JavaItemIcons.h"
#include <cmath>
// Exact base geometry, UVs and pose equations verified against official local
// Java 1.21.11 IronGolemModel (hbr) and renderer (hxm), with Mojang mappings.
class JavaIronGolemModel:public Model {
 ModelPart head,body,rightArm,leftArm,rightLeg,leftLeg;
 void box(ModelPart& part,int u,int v,float x,float y,float z,int w,int h,int d,float inflate,float px,float py,float pz,bool mirror=false){
  part.setModel(this);part.useCustomTextureDimensions=true;part.mirror=mirror;part.texOffs(u,v);part.addBox(x,y,z,w,h,d,inflate);part.setPos(px,py,pz);
 }
 static void draw(ModelPart& part){part.renderHorrible(1.f/16);}
public:
 static const char* texture(){return "mob/java-iron-golem.png";}
 static const char* crackTexture(float health,float maximum){
  if(!std::isfinite(health)||!std::isfinite(maximum)||maximum<=0)return nullptr;
  const float fraction=health/maximum;
  if(fraction<.25f)return "mob/java-iron-golem-crackiness-high.png";
  if(fraction<.5f)return "mob/java-iron-golem-crackiness-medium.png";
  if(fraction<.75f)return "mob/java-iron-golem-crackiness-low.png";
  return nullptr;
 }
 static float triangle(float phase,float period){return (std::abs(std::fmod(phase,period)-period*.5f)-period*.25f)/(period*.25f);}
 JavaIronGolemModel(){
  texWidth=texHeight=128;young=false;
  box(head,0,0,-4,-12,-5.5f,8,10,8,0,0,-7,-2);head.texOffs(24,0).addBox(-1,-5,-7.5f,2,4,2);
  box(body,0,40,-9,-2,-6,18,12,11,0,0,-7,0);body.texOffs(0,70).addBox(-4.5f,10,-3,9,5,6,.5f);
  box(rightArm,60,21,-13,-2.5f,-3,4,30,6,0,0,-7,0);
  box(leftArm,60,58,9,-2.5f,-3,4,30,6,0,0,-7,0);
  box(rightLeg,37,0,-3.5f,-3,-3,6,16,5,0,-4,11,0);
  box(leftLeg,60,0,-3.5f,-3,-3,6,16,5,0,5,11,0,true);
 }
 // attackTicks/flowerTicks must be sourced from distinct server entity events;
 // hurt ticks are never a substitute.
 void renderPose(float walkPosition,float walkSpeed,float relativeYaw,float pitch,float attackTicks=0,int flowerTicks=0){
  head.yRot=relativeYaw*.017453292f;head.xRot=pitch*.017453292f;
  const float wave=triangle(walkPosition,13);
  if(attackTicks>0)rightArm.xRot=leftArm.xRot=-2+1.5f*triangle(attackTicks,10);
  else if(flowerTicks>0){rightArm.xRot=-.8f+.025f*triangle(float(flowerTicks),70);leftArm.xRot=0;}
  else {rightArm.xRot=(-.2f+1.5f*wave)*walkSpeed;leftArm.xRot=(-.2f-1.5f*wave)*walkSpeed;}
  rightLeg.xRot=-1.5f*wave*walkSpeed;leftLeg.xRot=1.5f*wave*walkSpeed;rightLeg.yRot=leftLeg.yRot=0;
  for(auto* part:{&head,&body,&rightArm,&leftArm,&rightLeg,&leftLeg})draw(*part);
 }
 bool renderFlower(Textures* textures,float brightness){
  // Verified vanilla IronGolemFlowerLayer arm attachment and block transform.
  glPushMatrix2();glTranslatef2(rightArm.x/16,rightArm.y/16,rightArm.z/16);
  glRotatef2(rightArm.xRot*57.2957795f,1,0,0);
  glTranslatef2(-1.1875f,1.0625f,-.9375f);glTranslatef2(.5f,.5f,.5f);
  glScalef2(.5f,.5f,.5f);glRotatef2(-90,1,0,0);
  // renderWorld(size=2) applies unit scale and its X/Z -0.5 centering;
  // the remaining Y -0.5 completes vanilla's final block translation.
  glTranslatef2(0,-.5f,0);const bool rendered=JavaItemIcons::renderWorld(textures,"minecraft:poppy",2,brightness);glPopMatrix2();return rendered;
 }
};
#endif
