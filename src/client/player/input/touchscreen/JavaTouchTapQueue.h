#ifndef TCPE_JAVA_TOUCH_TAP_QUEUE_H
#define TCPE_JAVA_TOUCH_TAP_QUEUE_H
#include <deque>
#include <cstdint>
// Preserve short gestures through the asynchronous native->Rust pick round trip.
// Old/world-changing input is discarded, never replayed against a new world.
class JavaTouchTapQueue {
public:
 struct Tap {int x,y;double time;uint64_t epoch;unsigned world;uint64_t revision=0;};
 std::deque<Tap> taps;
 void clear(){taps.clear();}
 void prune(double now,uint64_t epoch,unsigned world){while(!taps.empty()&&(now-taps.front().time>.5||taps.front().epoch!=epoch||taps.front().world!=world))taps.pop_front();}
 bool push(int x,int y,double now,uint64_t epoch,unsigned world){prune(now,epoch,world);if(taps.size()>=8)return false;taps.push_back({x,y,now,epoch,world,0});return true;}
 bool locked(uint64_t revision)const{return !taps.empty()&&taps.front().revision!=0&&taps.front().revision==revision;}
 void bind(uint64_t revision){if(!taps.empty())taps.front().revision=revision;}
};
#endif
