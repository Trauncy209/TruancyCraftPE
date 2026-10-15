#ifndef TCPE_JAVA_BLOCK_MODELS_H
#define TCPE_JAVA_BLOCK_MODELS_H
#include <cstdint>
class JavaLevel;
class Tesselator;
namespace JavaBlockModels {
// -1 means supplied assets have no model; original TCPE translation remains active.
int layer(uint16_t state);
bool fullCube(uint16_t state);
// Same atlas as Java chunks. Caller binds it and owns begin/end and transforms.
const char* atlasTexture();
// Emit all model faces at local block coordinates, never cull against world blocks.
// layer(state): 0 opaque, 1 alpha-test, 2 blend; -1 has no supplied geometry.
// Air emits no vertices. Missing models return false for caller translation.
bool tesselateDisplay(Tesselator& t,uint16_t state,float brightness=1.0f);
bool tesselate(JavaLevel& level,Tesselator& t,int x,int y,int z,uint16_t state);
}
#endif
