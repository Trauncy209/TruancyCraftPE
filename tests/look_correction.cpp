#include "network/java/JavaLookCorrection.h"
#include <cassert>
#include <limits>
int main(){double yaw=12,pitch=20;JavaLookCorrection look;look.yaw=90;look.pitch=-15;assert(look.apply(yaw,pitch)&&yaw==90&&pitch==-15);look.relativeYaw=true;look.relativePitch=true;look.yaw=-30;look.pitch=25;assert(look.apply(yaw,pitch)&&yaw==60&&pitch==10);look.pitch=100;assert(look.apply(yaw,pitch)&&pitch==90);look.yaw=std::numeric_limits<double>::quiet_NaN();double before=yaw;assert(!look.apply(yaw,pitch)&&yaw==before&&pitch==90);}
