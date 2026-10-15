#ifndef TCPE_JAVA_FIREWORK_GEOMETRY_H
#define TCPE_JAVA_FIREWORK_GEOMETRY_H
#include <array>
#include <vector>
#include <string>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <functional>
// LegacyRandomSource/BitRandomSource distributions used by Java particles.
class JavaFireworkRandom {
 uint64_t seed;bool cached=false;double gaussian=0;
 uint32_t bits(unsigned n){seed=(seed*0x5deece66dULL+11)&((1ULL<<48)-1);return uint32_t(seed>>(48-n));}
public:
 explicit JavaFireworkRandom(uint64_t value):seed((value^0x5deece66dULL)&((1ULL<<48)-1)){}
 float nextFloat(){return bits(24)/float(1<<24);}
 double nextDouble(){return ((uint64_t(bits(26))<<27)+bits(27))/double(1ULL<<53);}
 int nextInt(int bound){if(bound<=0)return 0;if((bound&-bound)==bound)return int((uint64_t(bound)*bits(31))>>31);uint32_t value,result;do{value=bits(31);result=value%unsigned(bound);}while(uint64_t(value)-result+unsigned(bound-1)>0x7fffffffULL);return int(result);}
 double nextGaussian(){if(cached){cached=false;return gaussian;}double x,y,s;do{x=2*nextDouble()-1;y=2*nextDouble()-1;s=x*x+y*y;}while(s>=1||s==0);double multiplier=std::sqrt(-2*std::log(s)/s);gaussian=y*multiplier;cached=true;return x*multiplier;}
};
namespace JavaFireworkGeometry {
using Vector=std::array<double,3>;
inline std::vector<Vector> velocities(const std::string& shape,JavaFireworkRandom& random,const Vector& motion={{0,0,0}},const std::function<void(const Vector&)>& onParticle={}){
 std::vector<Vector> result;auto emit=[&](const Vector& v){result.push_back(v);if(onParticle)onParticle(v);};
 if(shape=="small_ball"||shape=="large_ball"){
  const int range=shape=="large_ball"?4:2;const double speed=shape=="large_ball"?.5:.25;
  for(int y=-range;y<=range;++y)for(int x=-range;x<=range;++x)for(int z=-range;z<=range;++z){double dx=x+(random.nextDouble()-random.nextDouble())*.5,dy=y+(random.nextDouble()-random.nextDouble())*.5,dz=z+(random.nextDouble()-random.nextDouble())*.5;const double divisor=std::sqrt(dx*dx+dy*dy+dz*dz)/speed+random.nextGaussian()*.05;emit({{dx/divisor,dy/divisor,dz/divisor}});if(y!=-range&&y!=range&&x!=-range&&x!=range)z+=range*2-1;}
 }else if(shape=="star"||shape=="creeper"){
  static const double star[][2]={{0,1},{.3455,.309},{.9511,.309},{.3795918367346939,-.12653061224489795},{.6122448979591837,-.8040816326530612},{0,-.35918367346938773}};
  static const double creeper[][2]={{0,.2},{.2,.2},{.2,.6},{.6,.6},{.6,.2},{.2,.2},{.2,0},{.4,0},{.4,-.6},{.2,-.6},{.2,-.4},{0,-.4}};
  const auto* points=shape=="star"?star:creeper;const int count=shape=="star"?6:12;emit({{points[0][0]*.5,points[0][1]*.5,0}});
  const float base=random.nextFloat()*3.1415927f;const double spread=shape=="creeper"?.034:.34;
  for(int plane=0;plane<3;++plane){const double angle=double(base)+double(float(plane)*3.1415927f)*spread;double previousX=points[0][0],previousY=points[0][1];for(int point=1;point<count;++point){for(double t=.25;t<=1;t+=.25){double x=(previousX+t*(points[point][0]-previousX))*.5,y=(previousY+t*(points[point][1]-previousY))*.5,z=x*std::sin(angle);x*=std::cos(angle);for(double sign=-1;sign<=1;sign+=2)emit({{x*sign,y,z*sign}});}previousX=points[point][0];previousY=points[point][1];}}
 }else if(shape=="burst"){
  const double xBias=random.nextGaussian()*.05,zBias=random.nextGaussian()*.05;for(int i=0;i<70;++i){double x=motion[0]*.5+random.nextGaussian()*.15+xBias,z=motion[2]*.5+random.nextGaussian()*.15+zBias,y=motion[1]*.5+random.nextDouble()*.5;emit({{x,y,z}});}
 }
 return result;
}
inline int sprite(int age,int lifetime){return std::max(0,std::min(7,age*7/std::max(1,lifetime)));}
inline bool visible(int age,int lifetime,bool twinkle){return !twinkle||age<lifetime/3||(age+lifetime)/3%2==0;}
inline float alpha(int age,int lifetime){return age>lifetime/2?1.f-(age-lifetime/2)/float(lifetime):.99f;}
inline float flashSize(int age,float partial){return 7.1f*std::sin((age+partial-1)*.25f*3.1415927f);}
inline float flashAlpha(int age,float partial){return .6f-(age+partial-1)*.25f*.5f;}
}
#endif
