#ifndef TCPE_JAVA_WIND_CHARGE_MODEL_H
#define TCPE_JAVA_WIND_CHARGE_MODEL_H
#include "client/model/Model.h"
#include <cmath>
// Official 1.21.11 WindChargeModel hhh: three boxes, 64x32 UV page.
// Renderer hzt scrolls energy-swirl U by age * .03; each child spins
// independently at +/-16 degrees per tick (the setup pose is reset).
class JavaWindChargeModel : public Model {
 ModelPart wind, charge;
public:
 JavaWindChargeModel() {
  texWidth=64;texHeight=32;young=false;
  wind.setModel(this);wind.useCustomTextureDimensions=true;
  wind.texOffs(15,20).addBox(-4,-1,-4,8,2,8);
  wind.texOffs(0,9).addBox(-3,-2,-3,6,4,6);
  charge.setModel(this);charge.useCustomTextureDimensions=true;
  charge.texOffs(0,0).addBox(-2,-2,-2,4,4,4);
 }
 static float scroll(float age){return std::isfinite(age)?age*.03f-std::floor(age*.03f):0;}
 void renderAge(float age) {
  if(!std::isfinite(age))age=0;
  wind.yRot=age*16*.017453292f;charge.yRot=-wind.yRot;
  wind.renderHorrible(1.f/16);charge.renderHorrible(1.f/16);
 }
};
#endif
