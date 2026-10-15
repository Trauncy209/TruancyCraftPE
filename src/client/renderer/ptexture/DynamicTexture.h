#ifndef NET_MINECRAFT_CLIENT_RENDERER_PTEXTURE__DynamicTexture_H__
#define NET_MINECRAFT_CLIENT_RENDERER_PTEXTURE__DynamicTexture_H__

#include <vector>

class Textures;

class DynamicTexture
{
public:
    int tex;
	int replicate;
    unsigned char pixels[16*16*4];

    DynamicTexture(int tex_);
	virtual ~DynamicTexture() {}

	virtual void tick() = 0;
    virtual bool nativeWorldOnly() const {return false;}
    virtual void bindTexture(Textures* tex);
};

// Independent playback of the observed reference fire cycles. Supplied art is
// recorded in docs/weather/FIRE.md; no reference source code is included.
class FireTexture: public DynamicTexture {
    std::vector<unsigned char> frames;
    int layer, phase;
public:
    FireTexture(int layer,const unsigned char* rgba,int width,int height);
    bool valid() const {return frames.size()==16*16*4*32;}
    bool nativeWorldOnly() const override {return true;}
    void tick() override;
    static int frameAt(int layer,unsigned tick) {
        return layer==0?int((tick+8u)&15u):int(tick&31u);
    }
};

class WaterTexture: public DynamicTexture
{
    typedef DynamicTexture super;
    int _tick;
	int _frame;

	float* current;
	float* next;
	float* heat;
	float* heata;

public:
    WaterTexture();
	~WaterTexture();

    void tick();
};

class WaterSideTexture: public DynamicTexture
{
	typedef DynamicTexture super;
	int _tick;
	int _frame;
	int _tickCount;

	float* current;
	float* next;
	float* heat;
	float* heata;

public:
	WaterSideTexture();
	~WaterSideTexture();

	void tick();
};

#endif /*NET_MINECRAFT_CLIENT_RENDERER_PTEXTURE__DynamicTexture_H__*/
