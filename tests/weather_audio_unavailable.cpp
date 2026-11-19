#include "platform/audio/SoundSystemAL.h"
#include "client/sound/Sound.h"
#include <cassert>
#include <iostream>
int main(int argc,char**) {
    // Run with ALSOFT_DRIVERS=codex_unavailable_backend, so this never opens
    // speakers. Playing weather with no device must return instead of hanging
    // while repeatedly asking OpenAL for an error without a current context.
    SoundSystemAL audio;
    const bool available=argc>1;
    if(available)assert(audio.isAvailable() && alcGetCurrentContext());
    else assert(!audio.isAvailable());
    short pcm[64]={};
    SoundDesc rain(reinterpret_cast<char*>(pcm),sizeof(pcm),1,2,44100);
    rain.name="@java|3|minecraft:weather.rain|16";
    audio.playAt(rain,0,0,0,.1f,1.f);
    audio.setListenerAngle(90);
    audio.enable(false);
    assert(!alcGetCurrentContext());
    audio.playAt(rain,0,0,0,.1f,1.f);
    audio.destroy(); // Reset/reopen while the app's audio context is suspended.
    assert(!alcGetCurrentContext());
    audio.enable(true);
    if(available) {
        assert(alcGetCurrentContext());
        audio.playAt(rain,0,0,0,.1f,1.f);
        assert(alGetError()==AL_NO_ERROR);
    }
    audio.destroy();
    std::cout<<"PASS: OpenAL weather playback/lifecycle safely handles unavailable and suspended contexts\n";
}
