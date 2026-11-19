//#include "ios/OpenALSupport.h"
#include "SoundSystemAL.h"
#include "../../util/Mth.h"
#include "../../world/level/tile/Tile.h"
#include "../../world/phys/Vec3.h"
#include "../../client/sound/Sound.h"

#include "../log.h"
#include <map>
#include <sstream>
struct JavaVoiceTag{int category;std::string name;};
static std::map<SoundSystemAL*,std::map<ALuint,JavaVoiceTag>> javaVoices;
void stopJavaSounds(SoundSystemAL* owner,const std::string& name,int category){for(const auto& v:javaVoices[owner])if((category==255||category==v.second.category)&&(name.empty()||name==v.second.name))alSourceStop(v.first);}


static const char* errIdString = 0;

void checkError() {

    // With no current context OpenAL can report the same error forever.
    if (!alcGetCurrentContext()) return;
    for (int attempt = 0; attempt < 16; ++attempt) {
        ALenum err = alGetError();
        if(err == AL_NO_ERROR) return;

        LOGI("### SoundSystemAL error: %d ####: %s\n", err, errIdString==0?"(none)":errIdString);
    }
}

//typedef ALvoid	AL_APIENTRY	(*alBufferDataStaticProcPtr) (const ALint bid, ALenum format, ALvoid *data, ALsizei size, ALsizei freq);
//ALvoid alBufferDataStaticProc(const ALint bid, ALenum format, ALvoid* data, ALsizei size, ALsizei freq)
//{
//	static alBufferDataStaticProcPtr proc = NULL;
//    
//    if (proc == NULL) {
//        proc = (alBufferDataStaticProcPtr) alcGetProcAddress(NULL, (const ALCchar*) "alBufferDataStatic");
//    }
//
//    if (proc)
//        proc(bid, format, data, size, freq);
//	
//    return;
//}
//
SoundSystemAL::SoundSystemAL()
:	available(false),
    context(0),
    device(0),
    _rotation(-9999.9f)
{
    _buffers.reserve(64);
	init();
}

SoundSystemAL::~SoundSystemAL()
{
    javaVoices.erase(this);
    ALCcontext* previous=alcGetCurrentContext();
    if (context && alcMakeContextCurrent(context)) {
        if (_sourceCount) alDeleteSources(_sourceCount,_sources);
        for (const Buffer& buffer:_buffers)
            if (buffer.inited) alDeleteBuffers(1,&buffer.bufferID);
    }
    if (context) {
        alcMakeContextCurrent(previous==context?NULL:previous);
        alcDestroyContext(context);
    }
    if (device) alcCloseDevice(device);
}

void SoundSystemAL::init()
{
    available=false;
	device = alcOpenDevice(NULL);
	if(device) {
		context = alcCreateContext(device, NULL);
        if (!context || !alcMakeContextCurrent(context)) {
            if(context)alcDestroyContext(context);
            context=NULL;alcCloseDevice(device);device=NULL;
            return;
        }
        
        alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
        
        // Generate individually so devices exposing fewer than the requested
        // voices retain a working pool instead of failing an all-or-none call.
        _sourceCount=0;
        for(int i=0;i<MaxNumSources;++i) {
            ALuint source=0;
            alGenSources(1,&source);
            if(alGetError()!=AL_NO_ERROR) break;
            _sources[_sourceCount++]=source;
        }
        if(_sourceCount<MaxNumSources)
            LOGI("Audio voice fallback: %d/%d sources; gameplay reserve %d\n",_sourceCount,MaxNumSources,SoundVoiceBudget::GameplayReserve);
		for(int index = 0; index < _sourceCount; index++) {
            ALuint sourceID = _sources[index];
            
            alSourcef(sourceID, AL_REFERENCE_DISTANCE, 5.0f);
            alSourcef(sourceID, AL_MAX_DISTANCE, 16.0f);
            alSourcef(sourceID, AL_ROLLOFF_FACTOR, 6.0f);
		}
        
		float listenerPos[] = {0, 0, 0};
		float listenerOri[] = {0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
		float listenerVel[] = {0, 0, 0};
		alListenerfv(AL_POSITION, listenerPos);
		alListenerfv(AL_ORIENTATION, listenerOri);
		alListenerfv(AL_VELOCITY, listenerVel);
        
        available=_sourceCount>0;
        errIdString = "Init audio";
        checkError();
	}
}

void SoundSystemAL::enable(bool status) {
    if (!context) return;
    if (status) {
        available=alcMakeContextCurrent(context) && _sourceCount>0;
        if(available)checkError();
    } else if (alcGetCurrentContext()==context) {
        alcMakeContextCurrent(NULL);
    }
}

void SoundSystemAL::destroy() {
    if(!context)return;
    ALCcontext* previous=alcGetCurrentContext();
    if(previous!=context && !alcMakeContextCurrent(context)) { available=false;return; }
    for(int i=0;i<_sourceCount;++i){alSourceStop(_sources[i]);alSourcei(_sources[i],AL_BUFFER,0);_voiceBudget[i]=SoundVoiceBudget::Voice();}
    _voiceSequence=0;
    for(auto& b:_buffers)if(b.inited)alDeleteBuffers(1,&b.bufferID);
    _buffers.clear();javaVoices.erase(this);
    if(previous!=context)alcMakeContextCurrent(previous);
}

void SoundSystemAL::setListenerPos( float x, float y, float z )
{
    // Note: listener position is thought to be 0,0,0 now
    
    /*
    if (_listenerPos.x != x || _listenerPos.y != y || _listenerPos.z != z) {
        _listenerPos.set(x, y, z);
        alListener3f(AL_POSITION, x, y, z);
        
        static int _n = 0;
        if (++_n == 20) {
            _n = 0;
            LOGI("Setting position for listener: %f, %f, %f\n", _listenerPos.x, _listenerPos.y, _listenerPos.z);
        }
    }
  */
}

void SoundSystemAL::setListenerAngle( float deg )
{
    if(!available || !context || alcGetCurrentContext()!=context)return;
    if (_rotation != deg) {
        _rotation = deg;

        float rad = deg * Mth::DEGRAD;

        static ALfloat orientation[] = {0, 0, 0,    0, 1, 0};
        orientation[0] = -Mth::sin( rad );
        orientation[2] =  Mth::cos( rad );
        alListenerfv(AL_ORIENTATION, orientation);
    }
}

void SoundSystemAL::playAt( const SoundDesc& sound, float x, float y, float z, float volume, float pitch )
{
    if(!available || !context || alcGetCurrentContext()!=context)return;
    if (pitch < 0.01f) pitch = 1;

    //LOGI("playing sound '%s' with volume/pitch: %f, %f @ %f, %f, %f\n", sound.name.c_str(), volume, pitch, x, y, z);
    
    ALuint bufferID;
    if (!getBufferId(sound, &bufferID)) {
        errIdString = "Get buffer (failed)";
        checkError();
        LOGE("getBufferId returned false!\n");
        return;
    }
    errIdString = "Get buffer";
    checkError();
    //LOGI("playing sound %d - '%s' with volume/pitch: %f, %f @ %f, %f, %f\n", bufferID, sound.name.c_str(), volume, pitch, x, y, z);

    int sourceIndex;
    errIdString = "Get free index";
    const bool rain=SoundVoiceBudget::rainTag(sound.name);
    if (!getFreeSourceIndex(&sourceIndex,rain)) {
        if(!rain) LOGI("No free sound sources left @ SoundSystemAL::playAt\n");
        return;
    }

    ALuint sourceID = _sources[sourceIndex];
    if(sound.name.compare(0,6,"@java|")==0){
     std::istringstream tag(sound.name.substr(6));std::string category,name,range;
     std::getline(tag,category,'|');std::getline(tag,name,'|');std::getline(tag,range,'|');
     javaVoices[this][sourceID]={std::atoi(category.c_str()),name};
     alSourcef(sourceID,AL_REFERENCE_DISTANCE,0);alSourcef(sourceID,AL_MAX_DISTANCE,std::atof(range.c_str()));alSourcef(sourceID,AL_ROLLOFF_FACTOR,1);
    }else{javaVoices[this].erase(sourceID);alSourcef(sourceID,AL_REFERENCE_DISTANCE,5);alSourcef(sourceID,AL_MAX_DISTANCE,16);alSourcef(sourceID,AL_ROLLOFF_FACTOR,6);}

    checkError();

    alSourcei(sourceID, AL_BUFFER, 0);
    errIdString = "unbind";
    checkError();
  	alSourcei(sourceID, AL_BUFFER, bufferID);
    errIdString = "bind";
    checkError();

    alSourcef(sourceID, AL_PITCH, pitch);
    errIdString = "pitch";
    checkError();
	alSourcef(sourceID, AL_GAIN, volume);
    errIdString = "gain";
    checkError();
	
    alSourcei(sourceID, AL_LOOPING, AL_FALSE);
    errIdString = "looping";
    checkError();
	alSource3f(sourceID, AL_POSITION, x, y, z);
    errIdString = "position";
    checkError();
	
	alSourcePlay(sourceID);
    errIdString = "source play";

    checkError();
}

/*static*/
void SoundSystemAL::removeStoppedSounds()
{
}

bool SoundSystemAL::getFreeSourceIndex(int* sourceIndex,bool rain) {
    for (int i=0;i<_sourceCount;++i) {
        ALint state=AL_STOPPED;
        alGetSourcei(_sources[i],AL_SOURCE_STATE,&state);
        _voiceBudget[i].playing=state==AL_PLAYING;
    }
    const int selected=SoundVoiceBudget::choose(_voiceBudget,_sourceCount,rain);
    if(selected<0) return false;
    // A playing source can only be selected when it belongs to rain ambience.
    // Stop it before rebinding the buffer; ordinary effects keep their voices.
    if(_voiceBudget[selected].playing) alSourceStop(_sources[selected]);
    _voiceBudget[selected].playing=true;
    _voiceBudget[selected].rain=rain;
    _voiceBudget[selected].serial=++_voiceSequence;
    *sourceIndex=selected;
    return true;
}

bool SoundSystemAL::getBufferId(const SoundDesc& sound, ALuint* buf) {
    if(!sound.isValid() || !sound.frames || sound.size<=0 || sound.frameRate<=0 ||
       (sound.channels!=1 && sound.channels!=2) || (sound.byteWidth!=1 && sound.byteWidth!=2))return false;
    for (int i = 0; i < (int)_buffers.size(); ++i) {
        // Points to the same data buffer -> sounds equal
        if (_buffers[i].framePtr == sound.frames) {
            //LOGI("Found %p for %s!\n", sound.frames, sound.name.c_str());
            *buf = _buffers[i].bufferID;
            return true;
        }
    }

    if (!sound.isValid()) {
        LOGE("Err: sound is invalid @ getBufferId! %s\n", sound.name.c_str());
        return false;
    }
    
    ALuint bufferID=0;
    alGenBuffers(1, &bufferID);
    const ALenum allocationError=alGetError();
    if(!bufferID || allocationError!=AL_NO_ERROR) { if(bufferID)alDeleteBuffers(1,&bufferID);return false; }
    
    ALenum format = (sound.byteWidth==2) ?
        (sound.channels==2? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16)
    :   (sound.channels==2? AL_FORMAT_STEREO8  : AL_FORMAT_MONO8);
    
    alBufferData(bufferID, format, sound.frames, sound.size, sound.frameRate);
    //LOGI("Creating %d (%p) from sound: '%s'\n", bufferID, sound.frames, sound.name.c_str());
    if(alGetError()!=AL_NO_ERROR) { alDeleteBuffers(1,&bufferID);return false; }
    errIdString = "Buffer data";
    //LOGI("Creating buffer with data: %d (%d), %p, %d, %d\n", format, sound.byteWidth, sound.frames, sound.size, sound.frameRate);
    checkError();

    //LOGI("Sound ch: %d, fmt: %d, frames: %p, len: %f, fr: %d, sz: %d, numfr: %d\n", sound.channels, format, sound.frames, sound.length(), sound.frameRate, sound.size, sound.numFrames);
    
    
    Buffer buffer;
    buffer.inited = true;
    buffer.framePtr = sound.frames;
    buffer.bufferID = bufferID;
    *buf = bufferID;
    _buffers.push_back(buffer);

    // @huge @attn @note @fix: The original data is free'd
    // On PLATFORM_DESKTOP the PCM data lives in static arrays (not heap),
    // so calling delete[] on them causes a debug-heap __debugbreak crash.
#if !defined(PLATFORM_DESKTOP)
    sound.destroy();
#endif
    return true;
}
