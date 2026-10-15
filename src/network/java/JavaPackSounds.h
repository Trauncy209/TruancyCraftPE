#ifndef TCPE_JAVA_PACK_SOUNDS_H
#define TCPE_JAVA_PACK_SOUNDS_H
#include <string>
#include <vector>
#include <cstdint>
struct JavaPackSoundVariant{std::string name,kind="file";uint32_t weight=1;float volume=1,pitch=1,attenuationDistance=16;};
struct JavaPackSoundDefinition{std::string name;bool replace=false;std::vector<JavaPackSoundVariant> variants;};
struct JavaPackSoundSample{std::string name,file;int channels=0,rate=0;std::vector<unsigned char> pcm;};
struct JavaPackSounds{std::vector<JavaPackSoundDefinition> definitions;std::vector<JavaPackSoundSample> samples;std::vector<std::string> errors;};
// One render-thread bridge per game process, activated with the SAME complete,
// ordered pack stack as fonts/models. No callback on repeated inactive clears.
class JavaSoundPackBridge{
 using Callback=void(*)(void*,const std::vector<const JavaPackSounds*>&);
 struct State{void*owner=nullptr;Callback callback=nullptr;bool active=false;};
 static State& state(){static State s;return s;}
public:
 static void install(void*owner,Callback callback){auto&s=state();s.owner=owner;s.callback=callback;s.active=false;}
 static void uninstall(void*owner){auto&s=state();if(s.owner==owner){s.owner=nullptr;s.callback=nullptr;s.active=false;}}
 static void clear(){auto&s=state();if(!s.active)return;s.active=false;if(s.callback)s.callback(s.owner,{});}
 static void apply(const std::vector<const JavaPackSounds*>&packs){if(packs.empty()){clear();return;}auto&s=state();s.active=true;if(s.callback)s.callback(s.owner,packs);}
};
#endif
