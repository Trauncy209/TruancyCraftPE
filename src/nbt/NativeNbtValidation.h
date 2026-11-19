#ifndef TCPE_NATIVE_NBT_VALIDATION_H
#define TCPE_NATIVE_NBT_VALIDATION_H
#include <cstddef>
#include <cstdint>
#include <cstring>
// TCPE's legacy save NBT uses native-endian primitives. Validate the entire
// bounded structure before legacy tag classes allocate arrays or recurse.
namespace NativeNbtValidation {
class Reader {
    const unsigned char* data; size_t size, offset = 0, nodes = 0;
    bool skip(size_t n) { if (n > size-offset) return false; offset+=n; return true; }
    template<class T> bool read(T& value) {
        if (sizeof(T)>size-offset) return false;
        std::memcpy(&value,data+offset,sizeof(T)); offset+=sizeof(T); return true;
    }
    bool string() { int16_t n=0; return read(n) && n>=0 && n<32767 && skip(size_t(n)); }
    bool payload(unsigned char type,unsigned depth) {
        if (depth>64 || ++nodes>1000000) return false;
        switch(type) {
        case 1:return skip(1);case 2:return skip(2);case 3:case 5:return skip(4);
        case 4:case 6:return skip(8);
        case 7:{int32_t n=0;return read(n)&&n>=0&&skip(size_t(n));}
        case 8:return string();
        case 9:{unsigned char child=0;int32_t n=0;
            if(!read(child)||!read(n)||n<0||child>10||(child==0&&n!=0)||size_t(n)>size-offset) return false;
            for(int32_t i=0;i<n;++i)if(!payload(child,depth+1))return false;
            return true;}
        case 10:for(;;){unsigned char child=0;if(!read(child))return false;
            if(child==0)return true;
            if(child>10||!string()||!payload(child,depth+1))return false;}
        default:return false;
        }
    }
public:
    Reader(const unsigned char* data,size_t size):data(data),size(size){}
    bool compound(){unsigned char root=0;return data&&size<=64u*1024u*1024u&&read(root)&&root==10&&string()&&payload(root,0)&&offset==size;}
};
inline bool compound(const unsigned char* data,size_t size){return Reader(data,size).compound();}
}
#endif
