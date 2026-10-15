#ifndef TCPE_JAVA_EMPTY_SECTION_H
#define TCPE_JAVA_EMPTY_SECTION_H
#include "JavaChunkStore.h"
// Only skip geometry when every authoritative state is the canonical air state.
// Missing data, unusual dimensions and other air variants retain normal meshing.
inline bool javaSectionIsCanonicalAir(const JavaChunkStore& store,int x,int y,int z,int xs,int ys,int zs) {
 if(xs!=16||zs!=16||ys<=0||x%16||z%16)return false;
 const auto* column=store.chunk(x/16,z/16);
 if(!column||column->height<=0)return false;
 const int64_t begin=int64_t(y)-column->minY,end=begin+ys;
 if(begin<0||end>column->height||column->states.size()!=size_t(column->height)*256)return false;
 for(size_t cell=0;cell<256;++cell)
  for(int64_t offset=begin;offset<end;++offset)
   if(column->states[cell*size_t(column->height)+size_t(offset)]!=0)return false;
 return true;
}
#endif
