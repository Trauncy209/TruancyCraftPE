#ifndef TCPE_JAVA_PLAYER_MODEL_H
#define TCPE_JAVA_PLAYER_MODEL_H
#include "client/model/HumanoidModel.h"
#include "JavaEntityAdapter.h"
// Uses the native TCPE pose and renderer with modern Java skin UV geometry.
class JavaPlayerModel:public HumanoidModel {
 ModelPart hat,jacket,rightSleeve,leftSleeve,rightPants,leftPants;
 void box(ModelPart& part,int u,int v,float x,float y,float z,int w,int h,int d,float inflate,float px,float py){
  part.clear();part.mirror=false;part.setModel(this);part.useCustomTextureDimensions=true;part.texOffs(u,v);part.addBox(x,y,z,w,h,d,inflate);part.setPos(px,py,0);
 }
public:
 explicit JavaPlayerModel(bool slim):HumanoidModel(){
  texWidth=texHeight=64;const int armWidth=slim?3:4;const float rightX=slim?-2.f:-3.f;
  box(head,0,0,-4,-8,-4,8,8,8,0,0,0);box(body,16,16,-4,0,-2,8,12,4,0,0,0);
  box(arm0,40,16,rightX,-2,-2,armWidth,12,4,0,-5,2);box(arm1,32,48,-1,-2,-2,armWidth,12,4,0,5,2);
  box(leg0,0,16,-2,0,-2,4,12,4,0,-2,12);box(leg1,16,48,-2,0,-2,4,12,4,0,2,12);
  box(hat,32,0,-4,-8,-4,8,8,8,.5f,0,0);box(jacket,16,32,-4,0,-2,8,12,4,.25f,0,0);
  box(rightSleeve,40,32,rightX,-2,-2,armWidth,12,4,.25f,-5,2);box(leftSleeve,48,48,-1,-2,-2,armWidth,12,4,.25f,5,2);
  box(rightPants,0,32,-2,0,-2,4,12,4,.25f,-2,12);box(leftPants,0,48,-2,0,-2,4,12,4,.25f,2,12);
 }
 void onGraphicsReset()override{HumanoidModel::onGraphicsReset();hat.onGraphicsReset();jacket.onGraphicsReset();rightSleeve.onGraphicsReset();leftSleeve.onGraphicsReset();rightPants.onGraphicsReset();leftPants.onGraphicsReset();}
 void render(Entity* e,float time,float r,float bob,float yaw,float pitch,float scale)override{
  HumanoidModel::render(e,time,r,bob,yaw,pitch,scale);
  hat.mimic(&head);jacket.mimic(&body);rightSleeve.mimic(&arm0);leftSleeve.mimic(&arm1);rightPants.mimic(&leg0);leftPants.mimic(&leg1);
  const unsigned layers=javaPlayerSkinLayers(e);if(layers&64)hat.render(scale);if(layers&2)jacket.render(scale);if(layers&8)rightSleeve.render(scale);if(layers&4)leftSleeve.render(scale);if(layers&32)rightPants.render(scale);if(layers&16)leftPants.render(scale);
 }
};
#endif
