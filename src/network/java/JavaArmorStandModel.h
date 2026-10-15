#ifndef TCPE_JAVA_ARMOR_STAND_MODEL_H
#define TCPE_JAVA_ARMOR_STAND_MODEL_H
#include "client/model/Model.h"
#include "JavaGameSession.h"
// Exact vanilla 1.21.11 ArmorStandModel geometry/UVs (official hgg/hgf).
class JavaArmorStandModel:public Model {
 ModelPart head,body,leftArm,rightArm,leftLeg,rightLeg,rightStick,leftStick,shoulder,plate;
 void box(ModelPart& p,int u,int v,float x,float y,float z,int w,int h,int d,float px,float py,float pz,bool mirror=false){p.setModel(this);p.useCustomTextureDimensions=true;p.mirror=mirror;p.texOffs(u,v);p.addBox(x,y,z,w,h,d);p.setPos(px,py,pz);}
 static void pose(ModelPart& p,const std::array<float,3>& angles){p.xRot=angles[0]*.017453292f;p.yRot=angles[1]*.017453292f;p.zRot=angles[2]*.017453292f;}
public:
 JavaArmorStandModel(){texWidth=texHeight=64;young=false;
  box(head,0,0,-1,-7,-1,2,7,2,0,1,0);box(body,0,26,-6,0,-1.5f,12,3,3,0,0,0);
  box(rightArm,24,0,-2,-2,-1,2,12,2,-5,2,0);box(leftArm,32,16,0,-2,-1,2,12,2,5,2,0,true);
  box(rightLeg,8,0,-1,0,-1,2,11,2,-1.9f,12,0);box(leftLeg,40,16,-1,0,-1,2,11,2,1.9f,12,0,true);
  box(rightStick,16,0,-3,3,-1,2,7,2,0,0,0);box(leftStick,48,16,1,3,-1,2,7,2,0,0,0);box(shoulder,0,48,-4,10,-1,8,2,2,0,0,0);box(plate,0,32,-6,11,-6,12,1,12,0,12,0);
 }
 void apply(const JavaGameSession::Entity& e){const auto&p=e.armorStand.poses;pose(head,p[0]);pose(body,p[1]);pose(leftArm,p[2]);pose(rightArm,p[3]);pose(leftLeg,p[4]);pose(rightLeg,p[5]);pose(rightStick,p[1]);pose(leftStick,p[1]);pose(shoulder,p[1]);plate.yRot=-float(e.yaw)*.017453292f;}
 void renderBody(const JavaGameSession::Entity& e){for(auto*p:{&head,&body,&leftLeg,&rightLeg,&rightStick,&leftStick,&shoulder})p->renderHorrible(1.f/16);if(e.armorStand.showArms){leftArm.renderHorrible(1.f/16);rightArm.renderHorrible(1.f/16);}if(e.armorStand.showBasePlate)plate.renderHorrible(1.f/16);}
 static void transform(ModelPart&p){glTranslatef2(p.x/16,p.y/16,p.z/16);glRotatef2(p.zRot*57.2957795f,0,0,1);glRotatef2(p.yRot*57.2957795f,0,1,0);glRotatef2(p.xRot*57.2957795f,1,0,0);}
 void headTransform(){transform(head);}
 void handTransform(bool left){transform(left?leftArm:rightArm);}
};
class JavaArmorStandArmorModel:public Model{
 ModelPart head,body,rightArm,leftArm,rightLeg,leftLeg;
 void box(ModelPart&p,int u,int v,float x,float y,float z,int w,int h,int d,float px,float py,float pz,float inflate,bool mirror=false){p.setModel(this);p.useCustomTextureDimensions=true;p.mirror=mirror;p.texOffs(u,v);p.addBox(x,y,z,w,h,d,inflate);p.setPos(px,py,pz);}
 static void pose(ModelPart&p,const std::array<float,3>&a){p.xRot=a[0]*.017453292f;p.yRot=a[1]*.017453292f;p.zRot=a[2]*.017453292f;}
public:
 explicit JavaArmorStandArmorModel(float inflate){texWidth=64;texHeight=32;young=false;
 box(head,0,0,-4,-8,-4,8,8,8,0,1,0,inflate);box(body,16,16,-4,0,-2,8,12,4,0,0,0,inflate);
 box(rightArm,40,16,-3,-2,-2,4,12,4,-5,2,0,inflate);box(leftArm,40,16,-1,-2,-2,4,12,4,5,2,0,inflate,true);
 box(rightLeg,0,16,-2,0,-2,4,12,4,-1.9f,11,0,inflate-.1f);box(leftLeg,0,16,-2,0,-2,4,12,4,1.9f,11,0,inflate-.1f,true);
 }
 void render(const JavaGameSession::Entity&e,int slot){const auto&p=e.armorStand.poses;pose(head,p[0]);pose(body,p[1]);pose(leftArm,p[2]);pose(rightArm,p[3]);pose(leftLeg,p[4]);pose(rightLeg,p[5]);
 if(slot==5)head.renderHorrible(1.f/16);if(slot==4){body.renderHorrible(1.f/16);leftArm.renderHorrible(1.f/16);rightArm.renderHorrible(1.f/16);}if(slot==3)body.renderHorrible(1.f/16);if(slot==2||slot==3){leftLeg.renderHorrible(1.f/16);rightLeg.renderHorrible(1.f/16);}}
};
#endif
