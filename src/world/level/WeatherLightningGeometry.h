#ifndef TCPE_WEATHER_LIGHTNING_GEOMETRY_H
#define TCPE_WEATHER_LIGHTNING_GEOMETRY_H
#include <cstdint>
// Independent seeded construction of the observed four-shell, three-branch
// lightning shape. Geometry remains CPU-only and can be tested without GLES.
namespace WeatherLightningGeometry {
class Random48 {
    uint64_t state;
public:
    explicit Random48(uint64_t seed):state((seed^UINT64_C(0x5deece66d))&((UINT64_C(1)<<48)-1)){}
    uint32_t bits(int count) {state=(state*UINT64_C(0x5deece66d)+11)&((UINT64_C(1)<<48)-1);return uint32_t(state>>(48-count));}
    int below(int bound) {
        if((bound&-bound)==bound)return int((uint64_t(bound)*bits(31))>>31);
        uint32_t n,v;
        do {n=bits(31);v=n%unsigned(bound);}while(uint64_t(n)-v+bound-1>=UINT64_C(0x80000000));
        return int(v);
    }
};
struct Vertex {float x,y,z;};
// Emit four vertices per side; render in one batch instead of one draw/segment.
template<class Quad> void generate(uint64_t seed,Quad emit) {
    float centerX[8],centerZ[8],totalX=0,totalZ=0;
    Random48 path(seed);
    for(int i=7;i>=0;--i) {centerX[i]=totalX;centerZ[i]=totalZ;totalX+=path.below(11)-5;totalZ+=path.below(11)-5;}
    const int cornerX[4]={-1,1,1,-1},cornerZ[4]={-1,-1,1,1};
    for(int shell=0;shell<4;++shell) {
        Random48 branchRandom(seed);
        for(int branch=0;branch<3;++branch) {
            const int top=branch?7-branch:7,bottom=branch?top-2:0;
            float px=centerX[top]-totalX,pz=centerZ[top]-totalZ;
            for(int h=top;h>=bottom;--h) {
                const float upperX=px,upperZ=pz;
                const int spread=branch?31:11,half=branch?15:5;
                px+=branchRandom.below(spread)-half;pz+=branchRandom.below(spread)-half;
                const float radius=.1f+.2f*shell;
                const float upperR=radius*(branch?1.f:1.f+h*.1f);
                const float lowerR=radius*(branch?1.f:1.f+(h-1)*.1f);
                for(int edge=0;edge<4;++edge) {
                    const int next=(edge+1)%4;
                    Vertex a={.5f+px+cornerX[edge]*lowerR,h*16.f,.5f+pz+cornerZ[edge]*lowerR};
                    Vertex b={.5f+upperX+cornerX[edge]*upperR,(h+1)*16.f,.5f+upperZ+cornerZ[edge]*upperR};
                    Vertex c={.5f+upperX+cornerX[next]*upperR,(h+1)*16.f,.5f+upperZ+cornerZ[next]*upperR};
                    Vertex d={.5f+px+cornerX[next]*lowerR,h*16.f,.5f+pz+cornerZ[next]*lowerR};
                    emit(a,b,c,d);
                }
            }
        }
    }
}
}
#endif
