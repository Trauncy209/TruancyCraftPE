#ifndef TCPE_TOUCH_MOUSE_OWNER_H
#define TCPE_TOUCH_MOUSE_OWNER_H
#include <cstdint>
// One finger owns the single-pointer menu cursor. Existing world fingers do
// not acquire a newly opened menu until a fresh press arrives.
class TouchMouseOwner {
 int owner=-1;std::uintptr_t context=0;
public:
 void observe(std::uintptr_t next){if(context!=next){context=next;owner=-1;}}
 bool down(int id){if(owner>=0)return false;owner=id;return true;}
 bool move(int id)const{return owner==id;}
 bool up(int id){if(owner!=id)return false;owner=-1;return true;}
 void cancel(){owner=-1;}
};
#endif
