#include "client/sound/JavaSoundBank.h"
#include "AppPlatform.h"
#include <fstream>
#include <cassert>
#include <iostream>
class Assets:public AppPlatform {
public:
 std::string root;
 bool isSuperFast(){return true;}
 BinaryBlob readAssetFile(const std::string& name) override {
  std::ifstream file(root+"/"+name,std::ios::binary|std::ios::ate);if(!file)return {};
  auto bytes=file.tellg();file.seekg(0);auto* data=new unsigned char[size_t(bytes)];file.read((char*)data,bytes);return {data,unsigned(bytes)};
 }
};
int main(int argc,char**argv) {
 assert(argc==2);Assets assets;assets.root=argv[1];JavaSoundBank bank;JavaSoundBank::Variant choice;SoundDesc sample;
 for(uint64_t seed=0;seed<12;++seed) {
  assert(bank.select(&assets,"minecraft:entity.lightning_bolt.thunder",seed*967,choice,sample,true));
  assert(sample.isValid()&&sample.size>0&&sample.channels==1&&sample.frameRate>0);
  assert(bank.select(&assets,"tcpe:random.explode",seed*967,choice,sample,true));assert(sample.isValid());
 }
 assert(!bank.select(&assets,"tcpe:ambient.weather.thunder",1,choice,sample,true));
 std::cout<<"Existing thunder Java-bank event and legacy impact decode PCM; missing legacy thunder alias confirms dedicated routing needed PASS\n";
}
