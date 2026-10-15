#ifndef TCPE_SOUND_VOICE_BUDGET_H
#define TCPE_SOUND_VOICE_BUDGET_H
#include <cstdint>
#include <algorithm>
#include <string>
namespace SoundVoiceBudget {
static const int TotalVoices=64;
static const int RainVoices=48;
static const int GameplayReserve=12;
struct Voice {bool playing=false;bool rain=false;uint64_t serial=0;};
inline bool rainEvent(const std::string& event) {
    return event=="minecraft:weather.rain" || event=="minecraft:weather.rain.above";
}
inline bool rainTag(const std::string& tag) {
    if(tag.compare(0,6,"@java|")!=0) return false;
    const size_t begin=tag.find('|',6);
    if(begin==std::string::npos) return false;
    const size_t end=tag.find('|',begin+1);
    return rainEvent(tag.substr(begin+1,end==std::string::npos?end:end-begin-1));
}
// Reserve at least the original twelve voices for gameplay. New rain events
// recycle the oldest rain tail at the original event cadence. Gameplay may
// reclaim a rain voice; rain can never steal a gameplay voice.
inline int choose(const Voice* voices,int count,bool incomingRain) {
    int free=-1,oldestRain=-1,rainCount=0;
    for(int i=0;i<count;++i) {
        if(!voices[i].playing) {if(free<0)free=i;continue;}
        if(voices[i].rain) {
            ++rainCount;
            if(oldestRain<0 || voices[i].serial<voices[oldestRain].serial) oldestRain=i;
        }
    }
    const int rainLimit=std::min(RainVoices,std::max(0,count-GameplayReserve));
    if(incomingRain && rainCount>=rainLimit) return oldestRain;
    return free>=0?free:oldestRain;
}
}
#endif
