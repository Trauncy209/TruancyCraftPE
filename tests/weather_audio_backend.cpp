#include <vector>
#include <list>
#include <string>
#include <sstream>
#include <cassert>
#include <iostream>
// Inspect actual source states in this isolated backend fixture only.
#define private public
#include "../src/platform/audio/SoundSystemAL.h"
#undef private
#include "../src/client/sound/Sound.h"
int main() {
 SoundSystemAL audio;
 assert(audio._sourceCount==64);
 std::vector<short> pcm(44100*5,1);
 SoundDesc rain(reinterpret_cast<char*>(pcm.data()),pcm.size()*2,1,2,44100);
 rain.name="@java|3|minecraft:weather.rain|16";
 for(int n=0;n<44;++n)audio.playAt(rain,0,0,0,.1f,.5f);
 int rainCount=0;
 for(int i=0;i<audio._sourceCount;++i){ALint state;alGetSourcei(audio._sources[i],AL_SOURCE_STATE,&state);rainCount+=state==AL_PLAYING&&audio._voiceBudget[i].rain;}
 assert(rainCount==44);
 SoundDesc game=rain;game.name="test.gameplay";
 for(int n=0;n<12;++n)audio.playAt(game,0,0,0,1,1);
 for(int n=0;n<100;++n)audio.playAt(rain,0,0,0,.1f,.5f);
 int gameCount=0;rainCount=0;
 for(int i=0;i<audio._sourceCount;++i){ALint state;alGetSourcei(audio._sources[i],AL_SOURCE_STATE,&state);if(state==AL_PLAYING){if(audio._voiceBudget[i].rain)++rainCount;else ++gameCount;}}
 assert(gameCount==12&&rainCount==48);
 for(int n=0;n<5;++n)audio.playAt(game,0,0,0,1,1);
 gameCount=0;
 for(int i=0;i<audio._sourceCount;++i){ALint state;alGetSourcei(audio._sources[i],AL_SOURCE_STATE,&state);gameCount+=state==AL_PLAYING&&!audio._voiceBudget[i].rain;}
 assert(gameCount==17);
 assert(alGetError()==AL_NO_ERROR);
 std::cout<<"Real OpenAL null backend: 44 rain tails, 48 voice cap, protected 12 effects and gameplay reclaim PASS\n";
}
