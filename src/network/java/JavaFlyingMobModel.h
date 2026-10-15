#ifndef TCPE_JAVA_FLYING_MOB_MODEL_H
#define TCPE_JAVA_FLYING_MOB_MODEL_H
#include "client/model/Model.h"
#include "client/renderer/Textures.h"
#include <cmath>
// Geometry/UVs and normal flying poses verified against the user's local
// official Java 1.21.11 client (AllayModel gzu, BeeModel haa, Mojang mappings).
// Slow native geometry avoids retaining context-dependent display lists.
class JavaFlyingMobModel:public Model {
 bool bee;
 ModelPart root,body,head,rightArm,leftArm,rightWing,leftWing,stinger,leftAntenna,rightAntenna,frontLeg,middleLeg,backLeg;
 void box(ModelPart& part,int u,int v,float x,float y,float z,int w,int h,int d,float inflate,float px,float py,float pz,bool mirror=false){
  part.setModel(this);part.useCustomTextureDimensions=true;part.mirror=mirror;part.texOffs(u,v);part.addBox(x,y,z,w,h,d,inflate);part.setPos(px,py,pz);
 }
 static void drawPart(ModelPart& part){
  if(!part.visible)return;constexpr float scale=1.f/16;
  glPushMatrix2();glTranslatef2(part.x*scale,part.y*scale,part.z*scale);
  if(part.zRot)glRotatef2(part.zRot*57.2957795f,0,0,1);
  if(part.yRot)glRotatef2(part.yRot*57.2957795f,0,1,0);
  if(part.xRot)glRotatef2(part.xRot*57.2957795f,1,0,0);
  if(!part.cubes.empty())part.drawSlow(scale);
  for(auto* child:part.children)drawPart(*child);
  glPopMatrix2();
 }
public:
 static bool supports(const std::string& kind){return kind=="Bee"||kind=="Allay";}
 static const char* texture(const std::string& kind){return kind=="Bee"?"mob/java-bee.png":kind=="Allay"?"mob/java-allay.png":nullptr;}
 explicit JavaFlyingMobModel(bool isBee):bee(isBee){
  texWidth=bee?64:32;texHeight=bee?64:32;young=false;
  if(bee){
   root.setPos(0,19,0);root.addChild(&body);root.addChild(&rightWing);root.addChild(&leftWing);root.addChild(&frontLeg);root.addChild(&middleLeg);root.addChild(&backLeg);
   box(body,0,0,-3.5f,-4,-5,7,7,10,0,0,0,0);
   body.addChild(&stinger);body.addChild(&leftAntenna);body.addChild(&rightAntenna);
   box(stinger,26,7,0,-1,5,0,1,2,0,0,0,0);
   box(leftAntenna,2,0,1.5f,-2,-3,1,2,3,0,0,-2,-5);
   box(rightAntenna,2,3,-2.5f,-2,-3,1,2,3,0,0,-2,-5);
   box(rightWing,0,18,-9,0,0,9,0,6,.001f,-1.5f,-4,-3);
   box(leftWing,0,18,0,0,0,9,0,6,.001f,1.5f,-4,-3,true);
   box(frontLeg,26,1,-5,0,0,7,2,0,0,1.5f,3,-2);
   box(middleLeg,26,3,-5,0,0,7,2,0,0,1.5f,3,0);
   box(backLeg,26,5,-5,0,0,7,2,0,0,1.5f,3,2);
  }else{
   root.setPos(0,23.5f,0);root.addChild(&head);root.addChild(&body);
   box(head,0,0,-2.5f,-5,-2.5f,5,5,5,0,0,-3.99f,0);
   box(body,0,10,-1.5f,0,-1,3,4,2,0,0,-4,0);body.texOffs(0,16).addBox(-1.5f,0,-1,3,5,2,-.2f);
   body.addChild(&rightArm);body.addChild(&leftArm);body.addChild(&rightWing);body.addChild(&leftWing);
   box(rightArm,23,0,-.75f,-.5f,-1,1,4,2,-.01f,-1.75f,.5f,0);
   box(leftArm,23,6,-.25f,-.5f,-1,1,4,2,-.01f,1.75f,.5f,0);
   box(rightWing,16,14,0,1,0,0,5,8,0,-.5f,0,.6f);
   box(leftWing,16,14,0,1,0,0,5,8,0,.5f,0,.6f);
  }
 }
 void renderFlying(float ticks,float relativeHeadYaw,float pitch){
  constexpr float pi=3.14159265359f;const float radians=pi/180;
  if(bee){
   const float hover=std::cos(ticks*.18f);root.y=19-hover*.9f;root.xRot=.1f+hover*pi*.025f;
   rightWing.yRot=leftWing.yRot=0;rightWing.zRot=std::cos(ticks*120.32113f*radians)*pi*.15f;leftWing.zRot=-rightWing.zRot;
   leftAntenna.xRot=rightAntenna.xRot=hover*pi*.03f;
   frontLeg.xRot=-hover*pi*.1f+pi*.125f;middleLeg.xRot=pi*.25f;backLeg.xRot=-hover*pi*.05f+pi*.25f;
  }else{
   const float slow=ticks*9*radians;root.y=23.5f+std::cos(slow)*.25f;
   head.xRot=pitch*radians;head.yRot=relativeHeadYaw*radians;
   rightWing.xRot=leftWing.xRot=.43633232f;rightWing.yRot=-pi*.25f+std::cos(ticks*20*radians)*pi*.15f;leftWing.yRot=-rightWing.yRot;
   const float arm=.43633232f-std::cos(slow+pi*1.5f)*pi*.075f;rightArm.zRot=arm;leftArm.zRot=-arm;
  }
  drawPart(root);
 }
};
#endif
