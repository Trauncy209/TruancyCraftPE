#include "network/java/JavaBlockModels.h"
#include "client/renderer/Tesselator.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>
#include <array>
#include <iostream>
static std::vector<std::array<float,5>> emittedVertices;
static std::vector<std::array<float,3>> emittedColors;
// Record the actual display renderer's output without requiring a GL context.
Tesselator::Tesselator(int){}
Tesselator::~Tesselator(){}
Tesselator Tesselator::instance(1);
void Tesselator::color(float r,float g,float b){emittedColors.push_back({r,g,b});}
void Tesselator::vertexUV(float x,float y,float z,float u,float v){emittedVertices.push_back({x,y,z,u,v});}
int main(){
 auto& t=Tesselator::instance;
 assert(JavaBlockModels::tesselateDisplay(t,1,.75f));assert(emittedVertices.size()==24&&emittedColors.size()==6);
 for(const auto& v:emittedVertices)for(float value:v)assert(std::isfinite(value)&&value>=0&&value<=1);
 for(const auto& c:emittedColors)for(float value:c)assert(value>=0&&value<=.75f);
 assert(JavaBlockModels::layer(1)==0);assert(JavaBlockModels::layer(562)==2);
 emittedVertices.clear();emittedColors.clear();assert(JavaBlockModels::tesselateDisplay(t,562,1));assert(emittedVertices.size()==24);
 emittedVertices.clear();assert(!JavaBlockModels::tesselateDisplay(t,0,1)&&emittedVertices.empty());assert(!JavaBlockModels::tesselateDisplay(t,65535,1)&&emittedVertices.empty());
 assert(JavaBlockModels::tesselateDisplay(t,1,std::numeric_limits<float>::quiet_NaN()));for(const auto& c:emittedColors)for(float value:c)assert(std::isfinite(value));
 std::cout<<"PASS actual display model API: complete independent cube faces, glass pass, air, unknown state and finite shading\n";
}
