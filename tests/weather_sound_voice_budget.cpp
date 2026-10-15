#include "../src/platform/audio/SoundVoiceBudget.h"
#include <cassert>
#include <iostream>
int main() {
 using namespace SoundVoiceBudget;
 assert(rainTag("@java|3|minecraft:weather.rain|16"));
 assert(rainTag("@java|3|minecraft:weather.rain.above|16"));
 assert(!rainTag("@java|3|minecraft:entity.lightning_bolt.thunder|16000"));
 assert(!rainTag("step.grass"));
 // Longest bundled rain WAV is 2.1920181405895693 seconds; .5 pitch
 // yields 4.384036281179139 seconds. At earliest reference cadence (10 Hz),
 // at most 44 tails overlap, fitting the 48-rain budget without truncation.
 Voice cadence[TotalVoices];double endTime[TotalVoices]={};
 for(int i=0;i<12;++i){cadence[i].playing=true;cadence[i].rain=false;}
 for(int event=0;event<1000;++event) {
  const double now=event*.1;
  for(int i=12;i<TotalVoices;++i)if(cadence[i].playing&&endTime[i]<=now)cadence[i].playing=false;
  const int slot=choose(cadence,TotalVoices,true);
  assert(slot>=12&&!cadence[slot].playing);
  cadence[slot].playing=true;cadence[slot].rain=true;cadence[slot].serial=event;
  endTime[slot]=now+4.384036281179139;
 }
 Voice voices[TotalVoices];uint64_t serial=0;
 auto play=[&](bool rain){int slot=choose(voices,TotalVoices,rain);assert(slot>=0);assert(!voices[slot].playing||voices[slot].rain);voices[slot].playing=true;voices[slot].rain=rain;voices[slot].serial=++serial;return slot;};
 // Even a burst of 1,000 rain starts leaves gameplay headroom.
 for(int n=0;n<1000;++n)play(true);
 int rain=0;for(auto& v:voices)rain+=v.playing&&v.rain;assert(rain==RainVoices);
 for(int n=0;n<TotalVoices-RainVoices;++n)play(false);
 for(int n=0;n<1000;++n)play(true);
 int game=0;for(auto& v:voices)game+=v.playing&&!v.rain;assert(game==TotalVoices-RainVoices);
 // Another effect reclaims oldest ambience; rain never reclaims that effect.
 play(false);game=0;for(auto& v:voices)game+=v.playing&&!v.rain;assert(game==TotalVoices-RainVoices+1);
 for(int n=0;n<100;++n)play(true);
 game=0;for(auto& v:voices)game+=v.playing&&!v.rain;assert(game==TotalVoices-RainVoices+1);
 for(auto& v:voices){v.playing=true;v.rain=false;}
 assert(choose(voices,TotalVoices,true)==-1);
 // Low-capacity devices keep a minimum gameplay reserve.
 Voice fallback[24];
 for(int n=0;n<100;++n){int slot=choose(fallback,24,true);assert(slot>=0);fallback[slot].playing=true;fallback[slot].rain=true;fallback[slot].serial=n;}
 int fallbackRain=0;for(auto& v:fallback)fallbackRain+=v.playing&&v.rain;assert(fallbackRain==12);
 assert(choose(fallback,12,true)>=0); // existing rain is reusable
 Voice limited[12];assert(choose(limited,12,true)==-1);
 // Finished effects become reusable even while all other voices are busy.
 voices[5].playing=false;assert(choose(voices,TotalVoices,true)==5);
 std::cout<<"Bounded rain voice recycling and protected gameplay voices PASS\n";
}
