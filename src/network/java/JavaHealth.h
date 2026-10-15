#ifndef TCPE_JAVA_HEALTH_H
#define TCPE_JAVA_HEALTH_H
#include <cmath>
#include <limits>
inline int javaNativeHealth(double health){
 if(!std::isfinite(health)||health<=0)return 0;
 const double rounded=std::ceil(health);
 return rounded>=std::numeric_limits<int>::max()?std::numeric_limits<int>::max():static_cast<int>(rounded);
}
#endif
