#include "raknet/BitStream.h"
#include "nbt/NbtIo.h"
#include "util/RakDataIO.h"
#include <cassert>
#include <cstring>
#include <iostream>
int main(){
 const char original[]={1,2,3,4,5};TagMemoryChunk borrowed;borrowed.data=(void*)original;borrowed.len=sizeof(original);
 CompoundTag tag;tag.putByteArray("array",borrowed);RakNet::BitStream bytes;RakDataOutput output(bytes);NbtIo::write(&tag,&output);
 RakDataInput input(bytes);CompoundTag* restored=NbtIo::read(&input);assert(restored);auto data=restored->getByteArray("array");
 assert(data.len==sizeof(original)&&!memcmp(data.data,original,sizeof(original)));
 Tag* copy=restored->copy();auto second=static_cast<CompoundTag*>(copy)->getByteArray("array");assert(second.len==data.len&&second.data!=data.data&&!memcmp(second.data,original,sizeof(original)));
 restored->deleteChildren();delete restored;assert(!memcmp(second.data,original,sizeof(original)));copy->deleteChildren();delete copy;
 tag.deleteChildren(); // Borrowed bytes remain owned by the caller.
 assert(original[0]==1);std::cout<<"Native NBT byte arrays retain lengths/content through load, deep-copy and ownership cleanup PASS\n";
}
