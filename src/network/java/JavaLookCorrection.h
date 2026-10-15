#ifndef TCPE_JAVA_LOOK_CORRECTION_H
#define TCPE_JAVA_LOOK_CORRECTION_H
#include <algorithm>
#include <cmath>
struct JavaLookCorrection {
 double yaw=0,pitch=0;bool relativeYaw=false,relativePitch=false;
 bool apply(double& currentYaw,double& currentPitch)const{
  if(!std::isfinite(yaw)||!std::isfinite(pitch)||!std::isfinite(currentYaw)||!std::isfinite(currentPitch))return false;
  const double nextYaw=relativeYaw?currentYaw+yaw:yaw;
  const double nextPitch=relativePitch?currentPitch+pitch:pitch;
  if(!std::isfinite(nextYaw)||!std::isfinite(nextPitch))return false;
  currentYaw=nextYaw;currentPitch=std::max(-90.,std::min(90.,nextPitch));return true;
 }
};
#endif
