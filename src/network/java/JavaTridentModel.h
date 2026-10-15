#ifndef TCPE_JAVA_TRIDENT_MODEL_H
#define TCPE_JAVA_TRIDENT_MODEL_H
#include "client/model/Model.h"
// Canonical Java1.21.11 TridentModel.hhg createLayer:32x32UV, five boxes.
class JavaTridentModel:public Model {
 ModelPart pole,base,left,middle,right;
 void box(ModelPart& p,int u,int v,float x,float y,int w,int h,bool mirror=false){p.setModel(this);p.useCustomTextureDimensions=true;p.mirror=mirror;p.texOffs(u,v);p.addBox(x,y,-.5f,w,h,1,0);p.setPos(0,0,0);}
public:
 JavaTridentModel(){texWidth=texHeight=32;box(pole,0,6,-.5f,2,1,25);box(base,4,0,-1.5f,0,3,2);box(left,4,3,-2.5f,-3,1,4);box(middle,0,0,-.5f,-4,1,4);box(right,4,3,1.5f,-3,1,4,true);}
 void draw(){for(ModelPart* p:{&pole,&base,&left,&middle,&right})p->renderHorrible(1.f/16);}
};
#endif
