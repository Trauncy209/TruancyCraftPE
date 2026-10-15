#ifndef SoundSystemAL_H__
#define SoundSystemAL_H__

#include "SoundSystem.h"
#include "SoundVoiceBudget.h"

#include <AL/al.h>
#include <AL/alc.h>

#include <vector>
#include <list>

//
// NOTE: This class is only the core OpenAL part of the sound engine.
//       Some audio setup code can still be managed from respective app
//       setup code (e.g. the main app delegate for iOS).
//

class SoundSystemAL: public SoundSystem
{
	//typedef std::list<SLObjectItf> SoundList;
public:
    SoundSystemAL();
	~SoundSystemAL();

	virtual void init();
	virtual void destroy();

    virtual void enable(bool status);
    
    virtual void setListenerPos(float x, float y, float z);
	virtual void setListenerAngle(float deg);

    virtual void load(const std::string& name){}
    virtual void play(const std::string& name){}
    virtual void pause(const std::string& name){}
    virtual void stop(const std::string& name){}
	virtual void playAt(const SoundDesc& sound, float x, float y, float z, float volume, float pitch);

private:
    class Buffer {
    public:
        Buffer()
        :   inited(false)
        {}
        bool inited;
        ALuint bufferID;
        char* framePtr;
    };
    
	void removeStoppedSounds();

    static const int MaxNumSources = SoundVoiceBudget::TotalVoices;
    SoundVoiceBudget::Voice _voiceBudget[MaxNumSources];
    uint64_t _voiceSequence=0;

	//SoundList playingBuffers;

    Vec3 _listenerPos;
    float _rotation;
    
	bool available;
    
    ALCcontext* context;
	ALCdevice* device;
    
    ALuint _sources[MaxNumSources] = {};
    int _sourceCount=0;
    std::vector<Buffer> _buffers;

    bool getFreeSourceIndex(int* src,bool rain);
    bool getBufferId(const SoundDesc& sound, ALuint* buf);
    
public:
};

#endif /*SoundSystemAL_H__ */
