#include "SoundEngine.h"
#include "JavaSoundBank.h"
#include <cmath>
#include <cstdlib>
#include <sstream>
static std::string javaAudioBase64(const std::vector<unsigned char>& bytes){static const char* alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::string out;out.reserve((bytes.size()+2)/3*4);for(size_t i=0;i<bytes.size();i+=3){unsigned n=unsigned(bytes[i])<<16;if(i+1<bytes.size())n|=unsigned(bytes[i+1])<<8;if(i+2<bytes.size())n|=bytes[i+2];out+=alphabet[n>>18];out+=alphabet[(n>>12)&63];out+=i+1<bytes.size()?alphabet[(n>>6)&63]:'=';out+=i+2<bytes.size()?alphabet[n&63]:'=';}return out;}
#if !defined(PRE_ANDROID23) && !defined(__APPLE__) && !defined(RPI)
void stopJavaSounds(SoundSystemAL*,const std::string&,int);
#endif
#include "../Options.h"
#include "../Minecraft.h"
#include "../../world/entity/Mob.h"


SoundEngine::SoundEngine( float maxDistance )
:	idCounter(0),
	mc(0),
	_x(0),
	_y(0),
	_z(0),
	_yRot(0),
	_invMaxDistance(1.0f / maxDistance)
{

}

SoundEngine::~SoundEngine()
{
 JavaSoundPackBridge::uninstall(this);
}

void SoundEngine::init( Minecraft* mc, Options* options )
{
	this->mc = mc;
	this->options = options;
 JavaSoundPackBridge::install(this,[](void* owner,const std::vector<const JavaPackSounds*>& packs){
  auto* engine=static_cast<SoundEngine*>(owner);engine->playServerSound("@reset",0,0,0,0,1,0);
  if(!engine->javaSoundBank)engine->javaSoundBank.reset(new JavaSoundBank);
  engine->javaSoundBank->apply(engine->mc->platform(),packs);
#if defined(PRE_ANDROID23)
  for(const auto& entry:engine->javaSoundBank->registrations())engine->mc->platform()->playSound("register|"+entry.first+"|"+javaAudioBase64(entry.second),0,1);
#endif
 });

	if (/*!loaded && */(options == NULL || (options->sound != 0 || options->music != 0))) {
		loadLibrary();
	}

#if !defined(PRE_ANDROID23) && !defined(__APPLE__) && !defined(RPI)
	sounds.add("step.cloth", SA_cloth1);
	sounds.add("step.cloth", SA_cloth2);
	sounds.add("step.cloth", SA_cloth3);
	sounds.add("step.cloth", SA_cloth4);
	sounds.add("step.grass", SA_grass1);
	sounds.add("step.grass", SA_grass2);
	sounds.add("step.grass", SA_grass3);
	sounds.add("step.grass", SA_grass4);
	sounds.add("step.gravel", SA_gravel1);
	sounds.add("step.gravel", SA_gravel2);
	sounds.add("step.gravel", SA_gravel3);
	sounds.add("step.gravel", SA_gravel4);
	sounds.add("step.sand", SA_sand1);
	sounds.add("step.sand", SA_sand2);
	sounds.add("step.sand", SA_sand3);
	sounds.add("step.sand", SA_sand4);
	sounds.add("step.stone", SA_stone1);
	sounds.add("step.stone", SA_stone2);
	sounds.add("step.stone", SA_stone3);
	sounds.add("step.stone", SA_stone4);
	sounds.add("step.wood", SA_wood1);
	sounds.add("step.wood", SA_wood2);
	sounds.add("step.wood", SA_wood3);
	sounds.add("step.wood", SA_wood4);

	sounds.add("random.splash", SA_splash);
	sounds.add("random.explode", SA_explode);
	sounds.add("random.click", SA_click);

	sounds.add("random.door_open", SA_door_open);
	sounds.add("random.door_close", SA_door_close);
	sounds.add("random.pop", SA_pop);
	sounds.add("random.pop2", SA_pop2);
	sounds.add("random.hurt", SA_hurt);
	sounds.add("random.glass", SA_glass1);
	sounds.add("random.glass", SA_glass2);
	sounds.add("random.glass", SA_glass3);

	sounds.add("mob.sheep", SA_sheep1);
	sounds.add("mob.sheep", SA_sheep2);
	sounds.add("mob.sheep", SA_sheep3);
	sounds.add("mob.pig", SA_pig1);
	sounds.add("mob.pig", SA_pig2);
	sounds.add("mob.pig", SA_pig3);
	sounds.add("mob.pigdeath", SA_pigdeath);

	sounds.add("mob.cow", SA_cow1);
	sounds.add("mob.cow", SA_cow2);
	sounds.add("mob.cow", SA_cow3);
	sounds.add("mob.cow", SA_cow4);
	sounds.add("mob.cowhurt", SA_cowhurt1);
	sounds.add("mob.cowhurt", SA_cowhurt2);
	sounds.add("mob.cowhurt", SA_cowhurt3);

	sounds.add("mob.chicken", SA_chicken2);
	sounds.add("mob.chicken", SA_chicken3);
	sounds.add("mob.chickenhurt", SA_chickenhurt1);
	sounds.add("mob.chickenhurt", SA_chickenhurt2);

	sounds.add("mob.zombie", SA_zombie1);
	sounds.add("mob.zombie", SA_zombie2);
	sounds.add("mob.zombie", SA_zombie3); 
	sounds.add("mob.zombiedeath", SA_zombiedeath);
	sounds.add("mob.zombiehurt", SA_zombiehurt1);
	sounds.add("mob.zombiehurt", SA_zombiehurt2);

	sounds.add("mob.skeleton", SA_skeleton1);
	sounds.add("mob.skeleton", SA_skeleton2);
	sounds.add("mob.skeleton", SA_skeleton3);
	sounds.add("mob.skeletonhurt", SA_skeletonhurt1);
	sounds.add("mob.skeletonhurt", SA_skeletonhurt2);
	sounds.add("mob.skeletonhurt", SA_skeletonhurt3);
	sounds.add("mob.skeletonhurt", SA_skeletonhurt4);

	sounds.add("mob.spider", SA_spider1);
	sounds.add("mob.spider", SA_spider2);
	sounds.add("mob.spider", SA_spider3);
	sounds.add("mob.spider", SA_spider4);
	sounds.add("mob.spiderdeath", SA_spiderdeath);

	sounds.add("mob.zombiepig.zpig", SA_zpig1);
	sounds.add("mob.zombiepig.zpig", SA_zpig2);
	sounds.add("mob.zombiepig.zpig", SA_zpig3);
	sounds.add("mob.zombiepig.zpig", SA_zpig4);
	sounds.add("mob.zombiepig.zpigangry", SA_zpigangry1);
	sounds.add("mob.zombiepig.zpigangry", SA_zpigangry2);
	sounds.add("mob.zombiepig.zpigangry", SA_zpigangry3);
	sounds.add("mob.zombiepig.zpigangry", SA_zpigangry4);
	sounds.add("mob.zombiepig.zpigdeath", SA_zpigdeath);
	sounds.add("mob.zombiepig.zpighurt", SA_zpighurt1);
	sounds.add("mob.zombiepig.zpighurt", SA_zpighurt2);

	sounds.add("damage.fallbig", SA_fallbig1);
	sounds.add("damage.fallbig", SA_fallbig2);
	sounds.add("damage.fallsmall", SA_fallsmall);

	sounds.add("random.bow", SA_bow);
	sounds.add("random.bowhit", SA_bowhit1);
	sounds.add("random.bowhit", SA_bowhit2);
	sounds.add("random.bowhit", SA_bowhit3);
	sounds.add("random.bowhit", SA_bowhit4);

	sounds.add("mob.creeper", SA_creeper1);
	sounds.add("mob.creeper", SA_creeper2);
	sounds.add("mob.creeper", SA_creeper3);
	sounds.add("mob.creeper", SA_creeper4);
	sounds.add("mob.creeperdeath", SA_creeperdeath);
	sounds.add("random.eat", SA_eat1);
	sounds.add("random.eat", SA_eat2);
	sounds.add("random.eat", SA_eat3);
	sounds.add("random.fuse", SA_fuse);

#endif
}

void SoundEngine::enable( bool status )
{
#if defined(__APPLE__)
	soundSystem.enable(status);
#endif
}

void SoundEngine::updateOptions()
{

}

void SoundEngine::destroy()
{
	//if (loaded) soundSystem.cleanup();
}

void SoundEngine::update( Mob* player, float a )
{
	if (/*!loaded || */options->sound == 0) return;
	if (player == NULL) return;

	_x = player->xo + (player->x - player->xo) * a;
	_y = player->yo + (player->y - player->yo) * a;
	_z = player->zo + (player->z - player->zo) * a;
	_yRot = player->yRotO + (player->yRot - player->yRotO) * a;

	soundSystem.setListenerAngle(_yRot);
	//soundSystem.setListenerPos(_x, _y, _z); //@note: not used, since we translate all sounds to Player space
}

float SoundEngine::_getVolumeMult( float x, float y, float z )
{
	const float dx = x - _x;
	const float dy = y - _y;
	const float dz = z - _z;
	const float dist = Mth::sqrt(dx*dx + dy*dy + dz*dz);
	const float out =  Mth::clamp(1.1f - dist*_invMaxDistance, -1.0f, 1.0f);
	return out;
}

#if defined(PRE_ANDROID23)
void SoundEngine::play(const std::string& name,float x,float y,float z,float volume,float pitch){playServerSound("tcpe:"+name,x,y,z,volume,pitch,uint64_t(random.nextInt()));}
void SoundEngine::playUI(const std::string& name,float volume,float pitch){playServerSound("tcpe:"+name,_x,_y,_z,volume,pitch,uint64_t(random.nextInt()));}
#elif defined(__APPLE__)
void SoundEngine::play(const std::string& name, float x, float y, float z, float volume, float pitch) {
	if ((volume *= options->sound) <= 0) return;

	volume = Mth::clamp(volume, 0.0f, 1.0f);

	SoundDesc sound;
	if (sounds.get(name, sound)) {
		soundSystem.playAt(sound, x-_x, y-_y, z-_z, volume, pitch);
	}
}
void SoundEngine::playUI(const std::string& name, float volume, float pitch) {
	if ((volume *= options->sound) <= 0) return;

	volume = Mth::clamp(volume, 0.0f, 1.0f);
	if (/*!loaded || */options->sound == 0 || volume <= 0) return;

	SoundDesc sound;
	if (sounds.get(name, sound)) {
		soundSystem.playAt(sound, 0, 0, 0, volume, pitch);
	}
}
#elif defined(RPI)
void SoundEngine::play(const std::string& name, float x, float y, float z, float volume, float pitch) {}
void SoundEngine::playUI(const std::string& name, float volume, float pitch) {}
#else
void SoundEngine::play(const std::string& name, float x, float y, float z, float volume, float pitch) {
	if ((volume *= options->sound) <= 0) return;

	volume = Mth::clamp( volume * _getVolumeMult(x, y, z), 0.0f, 1.0f);
	if (/*!loaded || */options->sound == 0 || volume <= 0) return;

	SoundDesc sound;
	if (sounds.get(name, sound)) {
		float dist = SOUND_DISTANCE;
		if (volume > 1) dist *= volume;
		soundSystem.playAt(sound, x-_x, y-_y, z-_z, volume, pitch);
	}
}
void SoundEngine::playUI(const std::string& name, float volume, float pitch) {
	if ((volume *= options->sound) <= 0) return;

	volume = Mth::clamp(volume, 0.0f, 1.0f);
	if (/*!loaded || */options->sound == 0 || volume <= 0) return;

	SoundDesc sound;
	if (sounds.get(name, sound)) {
		soundSystem.playAt(sound, 0, 0, 0, volume, pitch);
	}
}
#endif

// Java server sounds use canonical assets, packet identity, position, gain and pitch.
bool SoundEngine::playServerSound(const std::string& command,float x,float y,float z,float volume,float pitch,uint64_t seed,float fixedRange){
 if(!mc||!options)return false;
 if(command=="@reset"){
#if defined(PRE_ANDROID23)
 mc->platform()->playSound("reset",0,1);
#elif !defined(__APPLE__) && !defined(RPI)
 soundSystem.destroy();
#endif
 if(javaSoundBank)javaSoundBank->clear();return true;
 }
 std::string name=command;int category=-1;
 if(command.compare(0,6,"@stop|")==0){auto end=command.find('|',6);if(end==std::string::npos)return false;category=std::atoi(command.substr(6,end-6).c_str());name=command.substr(end+1);
#if defined(PRE_ANDROID23)
 mc->platform()->playSound("stop|"+std::to_string(category)+"|"+name,0,1);
#elif !defined(__APPLE__) && !defined(RPI)
 stopJavaSounds(&soundSystem,name,category);
#endif
 return true;
 }
 if(command.compare(0,6,"@play|")==0){auto end=command.find('|',6);if(end==std::string::npos)return false;category=std::atoi(command.substr(6,end-6).c_str());name=command.substr(end+1);}
 if(options->sound<=0||!std::isfinite(volume)||!std::isfinite(pitch)||volume<=0||pitch<=0)return false;
 if(!javaSoundBank)javaSoundBank.reset(new JavaSoundBank);JavaSoundBank::Variant variant;SoundDesc sample;
#if defined(PRE_ANDROID23)
 const bool pcm=false;
#else
 const bool pcm=true;
#endif
 if(!javaSoundBank->select(mc->platform(),name,seed,variant,sample,pcm))return false;
 float range=variant.distance*std::max(1.f,volume*variant.volume);float dx=x-_x,dy=y-_y,dz=z-_z;
 float gain=Mth::clamp(volume*variant.volume*options->sound*(category==1?options->music:1.f),0.f,1.f);float speed=Mth::clamp(pitch*variant.pitch,.5f,2.f);if(gain<=0||range<=0)return false;
#if defined(PRE_ANDROID23)
 float distance=std::sqrt(dx*dx+dy*dy+dz*dz);if(sample.channels==1)gain*=std::max(0.f,1-distance/range);if(gain<=0)return false;
 mc->platform()->playSound("play|"+std::to_string(category)+"|"+name+"|sounds/java/"+variant.file,gain,speed);
#elif !defined(RPI)
 sample.name="@java|"+std::to_string(category)+"|"+name+"|"+std::to_string(range);soundSystem.playAt(sample,dx,dy,dz,gain,speed);
#endif
 return true;
}
