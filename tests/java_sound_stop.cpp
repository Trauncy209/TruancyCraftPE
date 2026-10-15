#include <string>
#include <vector>
#include <list>
#include <sstream>
#include <map>
#include <set>
#include <memory>
#include <iostream>
#define private public
#include "../original-tcpe/src/platform/audio/SoundSystemAL.h"
#undef private
#include "../original-tcpe/src/client/sound/Sound.h"
#include <cassert>
#include <iostream>
void stopJavaSounds(SoundSystemAL*,const std::string&,int);
static ALint state(ALuint source){ALint value;alGetSourcei(source,AL_SOURCE_STATE,&value);return value;}
int main(){SoundSystemAL backend;assert(backend.context);std::vector<short> pcm(48000,500);SoundDesc sample(reinterpret_cast<char*>(pcm.data()),96000,1,2,48000);sample.name="@java|4|custom:one|24";backend.playAt(sample,1,0,0,1,1);auto one=backend._sources[0];assert(state(one)==AL_PLAYING);float range,reference;alGetSourcef(one,AL_MAX_DISTANCE,&range);alGetSourcef(one,AL_REFERENCE_DISTANCE,&reference);assert(range==24&&reference==0);sample.name="@java|8|custom:two|16";backend.playAt(sample,0,0,0,1,1);auto two=backend._sources[1];assert(state(two)==AL_PLAYING);stopJavaSounds(&backend,"custom:one",8);assert(state(one)==AL_PLAYING&&state(two)==AL_PLAYING);stopJavaSounds(&backend,"custom:one",255);assert(state(one)==AL_STOPPED&&state(two)==AL_PLAYING);stopJavaSounds(&backend,"",8);assert(state(two)==AL_STOPPED);backend.playAt(sample,0,0,0,1,1);stopJavaSounds(&backend,"",255);assert(state(one)==AL_STOPPED);backend.destroy();assert(backend._buffers.empty());assert(alGetError()==AL_NO_ERROR);std::cout<<"Real AL sources: exact event/category/all StopSound, Java attenuation and scope buffer reset PASS\n";}
