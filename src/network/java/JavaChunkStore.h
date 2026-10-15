#ifndef TCPE_JAVA_CHUNK_STORE_H
#define TCPE_JAVA_CHUNK_STORE_H
#include <cstdint>
#include <vector>
#include <map>
#include <utility>
#include <memory>
#include "tcpe_java_network.h"
// Authoritative modern states; no legacy height crop or lossy ID conversion.
class JavaChunkStore {
public:
 struct Chunk {int minY,height;std::vector<uint16_t> states;std::vector<uint8_t> skyLight,blockLight,lightSections;};
 bool apply(const TcpeJavaEvent& event);
 bool applyLight(const TcpeJavaEvent& event);
 bool light(bool sky,int x,int y,int z,uint8_t& result)const;
 int rawBrightness(int x,int y,int z,int skyDarken)const;
 void unload(int x,int z){if(chunks.erase(std::make_pair(x,z)))++revision;}
 void clear(){if(!chunks.empty()){chunks.clear();++revision;}}
 bool state(int x,int y,int z,uint16_t& result)const;
 size_t size()const{return chunks.size();}
 const Chunk* chunk(int x,int z)const;
 uint64_t version()const{return revision;}
 const std::map<std::pair<int,int>,std::shared_ptr<const Chunk>>& entries()const{return chunks;}
private:
 std::map<std::pair<int,int>,std::shared_ptr<const Chunk>> chunks;
 uint64_t revision=0;
};
#endif
