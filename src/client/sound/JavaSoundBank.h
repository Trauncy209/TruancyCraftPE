#ifndef TCPE_JAVA_SOUND_BANK_H
#define TCPE_JAVA_SOUND_BANK_H
#include "Sound.h"
#include "../../network/java/JavaPackSounds.h"
#include <map>
#include <set>
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
class AppPlatform;
class JavaSoundBank{
public:
 struct Variant{std::string file,resource;bool event=false;unsigned weight=1;float volume=1,pitch=1,distance=16;};
 struct Sample{std::vector<unsigned char> data;int rate=0,channels=0;SoundDesc desc;};
 bool select(AppPlatform*,const std::string&,uint64_t,Variant&,SoundDesc&,bool loadPcm);
 void apply(AppPlatform*,const std::vector<const JavaPackSounds*>&);
 void clear();
 std::vector<std::pair<std::string,std::vector<unsigned char>>> registrations()const;
 static bool wav(const std::vector<unsigned char>&,size_t&,size_t&,int&,int&);
private:
 bool loaded=false;
 std::map<std::string,std::vector<Variant>> events;
 std::map<std::string,std::unique_ptr<Sample>> samples;
 std::map<std::string,std::vector<Variant>> activeEvents;
 std::map<std::string,std::string> namedFiles,fileNames;
 struct Metadata{int rate=0,channels=0;};std::map<std::string,Metadata> metadata;
 std::map<std::string,std::unique_ptr<Sample>> packSamples;
 std::map<std::string,std::string> packFiles;
 void load(AppPlatform*);
 bool available(const Variant&)const;
 uint32_t weight(const std::string&,std::set<std::string>&)const;
 bool resolve(const std::string&,uint64_t&,Variant&,std::set<std::string>&)const;
};
#endif
