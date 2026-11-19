#include "headless_egl.h"
#include "client/Minecraft.h"
#include "client/renderer/Textures.h"
#include "client/renderer/Tesselator.h"
#include <map>
#define private public
#define protected public
#include "client/renderer/LevelRenderer.h"
#include "client/particle/ParticleEngine.h"
#undef private
#undef protected
#include "client/particle/SmokeParticle.h"
#include "client/sound/JavaSoundBank.h"
#include "world/level/Level.h"
#include "world/level/LevelListener.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/chunk/EmptyLevelChunk.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/FireTile.h"
#include "world/entity/MobCategory.h"
#include "world/entity/animal/Pig.h"
#include "world/item/Item.h"
#include "world/level/biome/Biome.h"
#include "AppPlatform.h"
#include "util/Mth.h"
#include <png.h>
#include <fstream>
#include <cassert>
#include <iostream>
#include <cstdlib>
#include <csignal>
#include <execinfo.h>
#include <unistd.h>
static unsigned routedFire=0;
extern "C" bool __wrap__ZN11SoundEngine15playServerSoundERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEfffffmf(void*,const std::string& event,float,float,float,float volume,float pitch,uint64_t,float) {
    if(event=="@play|4|minecraft:block.fire.ambient") { assert(volume>=1&&volume<2&&pitch>=.3f&&pitch<1.f);++routedFire; }
    return true;
}
static GLuint particleTexture=0;
static TextureId texture(const std::string& name) {
    assert(name=="particles.png");
    if(!particleTexture) {
        png_image image{};image.version=PNG_IMAGE_VERSION;
        assert(png_image_begin_read_from_file(&image,"/assets/images/particles.png"));image.format=PNG_FORMAT_RGBA;
        std::vector<unsigned char> pixels(PNG_IMAGE_SIZE(image));assert(png_image_finish_read(&image,nullptr,pixels.data(),0,nullptr));
        glGenTextures(1,&particleTexture);glBindTexture(GL_TEXTURE_2D,particleTexture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,image.width,image.height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());png_image_free(&image);
    }
    glBindTexture(GL_TEXTURE_2D,particleTexture);return particleTexture;
}
extern "C" TextureId __wrap__ZN8Textures18loadAndBindTextureERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE(void*,const std::string& name) { return texture(name); }
extern "C" TextureId __wrap__ZN8Textures11loadTextureERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEEb(void*,const std::string& name,bool) { return texture(name); }
struct Game: Minecraft { Game():Minecraft(){} };
struct Source:ChunkSource {
    Level* level;int missingReads=0;std::map<std::pair<int,int>,LevelChunk*> chunks;EmptyLevelChunk* empty;
    explicit Source(Level* l):level(l),empty(new EmptyLevelChunk(l,0,0)){}
    bool hasChunk(int x,int z)override{return chunks.count({x,z});}
    LevelChunk* getChunk(int x,int z)override{auto it=chunks.find({x,z});if(it!=chunks.end())return it->second;++missingReads;return empty;}
    LevelChunk* create(int x,int z)override{auto* chunk=new LevelChunk(level,new unsigned char[32768](),x,z);chunks[{x,z}]=chunk;return chunk;}
    void postProcess(ChunkSource*,int,int)override{}bool tick()override{return false;}bool shouldSave()override{return false;}
    Biome::MobList getMobsAt(const MobCategory&,int,int,int)override{return {};}
    std::string gatherStats()override{return "fire ambient fixture";}
};
struct World:Level {
    using Level::Level;bool java=false;bool isJavaServerLevel()const override{return java;}
    void use(Source* source){delete _chunkSource;_chunkSource=source;}
    float getBrightness(int,int,int)override{return 1.f;}
};
struct Probe:LevelListener {
    int smoke=0,fire=0;
    void addParticle(const std::string& name,float,float,float,float,float,float,int)override{assert(name=="largesmoke");++smoke;}
    void playSound(const std::string& name,float,float,float,float volume,float pitch)override{assert(name=="fire.fire");assert(volume>=1&&volume<2&&pitch>=.3f&&pitch<1);++fire;}
};
struct Assets:AppPlatform {
    BinaryBlob readAssetFile(const std::string& name)override{
        std::ifstream file("/assets/"+name,std::ios::binary|std::ios::ate);if(!file)return {};
        size_t size=file.tellg();file.seekg(0);auto* bytes=new unsigned char[size];file.read(reinterpret_cast<char*>(bytes),size);return {bytes,unsigned(size)};
    }
};
int main() {
    std::signal(SIGFPE,[](int code){void* frames[32];int count=backtrace(frames,32);backtrace_symbols_fd(frames,count,2);std::_Exit(128+code);});
    std::cout.setf(std::ios::unitbuf);NativeHeadlessEGL egl(256,256);
    Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();
    auto* world=new World(new MemoryLevelStorage(),"fire ambient",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);world->setUpdateLights(false);
    auto* source=new Source(world);world->use(source);for(int x=1;x<=3;++x)for(int z=1;z<=3;++z)source->create(x,z);
    for(int x=36;x<=44;++x)for(int z=36;z<=44;++z)world->setTileNoUpdate(x,63,z,Tile::rock->id);
    world->setTileNoUpdate(40,64,40,Tile::fire->id);
    auto* game=new Game();game->level=world;game->textures=new Textures(nullptr,nullptr);game->particleEngine=new ParticleEngine(world,game->textures);
    auto* camera=new Pig(world);camera->setPos(40,64,43);camera->xOld=camera->x;camera->yOld=camera->y;camera->zOld=camera->z;camera->yRot=camera->xRot=0;game->cameraTargetPlayer=camera;
    auto* renderer=new LevelRenderer(game);renderer->level=world;
    Probe probe;world->addListener(&probe);world->addListener(renderer);
    Random random(1234);
    for(int i=0;i<120;++i){Tile::fire->animateTick(world,40,64,40,&random);if(i<119)game->particleEngine->clear();}
    assert(probe.smoke==360&&probe.fire>0&&routedFire==unsigned(probe.fire));
    assert(game->particleEngine->particles[0].size()==3);
    for(int i=0;i<6;++i)game->particleEngine->tick();
    assert(!game->particleEngine->particles[0].empty());
    glViewport(0,0,256,256);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(-1.5,1.5,0,3,-10,10);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    glDisable2(GL_DEPTH_TEST);glDisable2(GL_LIGHTING);glDisable2(GL_FOG);glEnable2(GL_TEXTURE_2D);glEnable2(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.01);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glClearColor(.7f,.8f,1.f,1.f);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    game->particleEngine->render(camera,.5f);glFinish();assert(glGetError()==GL_NO_ERROR);
    std::vector<unsigned char> pixels(256*256*4);glReadPixels(0,0,256,256,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());size_t changed=0;
    for(size_t i=0;i<pixels.size();i+=4)if(pixels[i]<170||pixels[i+1]<195||pixels[i+2]<245)++changed;
    assert(changed>20);std::cout<<"Actual native fire smoke GLES3 pixels="<<changed<<" crackles="<<probe.fire<<"\n";
    png_image image{};image.version=PNG_IMAGE_VERSION;image.width=image.height=256;image.format=PNG_FORMAT_RGBA;
    assert(png_image_write_to_file(&image,"/qa/fire-smoke.png",0,pixels.data(),0,nullptr));
    const auto particlesBefore=game->particleEngine->particles[0].size();const unsigned audioBefore=routedFire;
    game->cameraTargetPlayer=NULL;renderer->addParticle("largesmoke",40,65,40,0,0,0,0);renderer->playSound("fire.fire",40,65,40,1,.5f);
    assert(game->particleEngine->particles[0].size()==particlesBefore&&routedFire==audioBefore);game->cameraTargetPlayer=camera;
    const int nativeSmoke=probe.smoke,nativeSound=probe.fire;world->java=true;
    Tile::fire->animateTick(world,40,64,40,&random);assert(probe.smoke==nativeSmoke&&probe.fire==nativeSound);world->java=false;
    // Neither stale fire animation nor smoke movement may generate an unloaded
    // neighboring chunk. Missing seams simply defer/expire local visual work.
    world->removeListener(renderer); // This fixture intentionally has no terrain-renderer chunk grid.
    game->particleEngine->clear();source->chunks.erase({3,2});world->setTileNoUpdate(47,63,40,Tile::rock->id);world->setTileNoUpdate(47,64,40,Tile::fire->id);
    const int before=source->missingReads,smokeBefore=probe.smoke;
    Tile::fire->animateTick(world,47,64,40,&random);assert(probe.smoke==smokeBefore&&source->missingReads==before);
    SmokeParticle seam(world,47.95f,64.5f,40.5f,0,0,0,2.5f);seam.tick();assert(seam.removed&&source->missingReads==before);
    Assets assets;JavaSoundBank bank;JavaSoundBank::Variant choice;SoundDesc sound;
    assert(bank.select(&assets,"minecraft:block.fire.ambient",1,choice,sound,true));assert(sound.isValid()&&sound.channels==1&&sound.size>0);
    std::cout<<"PASS: actual native fire callback smoke/crackle, canonical audio decode, GLES3 particle render and unloaded-seam safety (surfaceless EGL, no window/audio output)\n";
    std::_Exit(0);
}
