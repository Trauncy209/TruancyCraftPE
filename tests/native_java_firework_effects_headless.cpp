#include "network/java/JavaWindChargeModel.h"
#include "network/java/JavaIronGolemModel.h"
#include "client/renderer/entity/EntityRenderDispatcher.h"
#include "network/java/JavaEntityAdapter.h"
#include "network/java/JavaGameSession.h"
#include "network/java/JavaLevel.h"
#include "world/entity/player/Player.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "client/renderer/Tesselator.h"
#include "client/renderer/Textures.h"
#include "world/level/tile/Tile.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "util/Mth.h"
#include "AppPlatform_glfw.h"
#include <cassert>
#include <png.h>
#include <iostream>
extern "C" TcpeJavaSession* tcpe_java_connect(const char*,size_t,const char*,size_t,bool){return nullptr;}
extern "C" void tcpe_java_close(TcpeJavaSession*){}
extern "C" bool tcpe_java_command(TcpeJavaSession*,const char*,size_t){return true;}
#include "headless_egl.h"
#include "client/particle/JavaFireworkParticle.h"
template<class Tag,typename Tag::type Member>struct TestAccess{friend typename Tag::type field(Tag){return Member;}};
struct EngineListTag{using type=ParticleList (ParticleEngine::*)[4];friend type field(EngineListTag);};
struct ParticleAgeTag{using type=int Particle::*;friend type field(ParticleAgeTag);};
struct ParticleLifeTag{using type=int Particle::*;friend type field(ParticleLifeTag);};
struct SparkSpriteTag{using type=int JavaFireworkParticles::Spark::*;friend type field(SparkSpriteTag);};
template struct TestAccess<EngineListTag,&ParticleEngine::particles>;
template struct TestAccess<ParticleAgeTag,&Particle::age>;
template struct TestAccess<ParticleLifeTag,&Particle::lifetime>;
template struct TestAccess<SparkSpriteTag,&JavaFireworkParticles::Spark::sprite>;


int main(){
 NativeHeadlessEGL offscreen(400,400);Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);JavaGameSession session;JavaLevel level(new MemoryLevelStorage(),session.chunks);ParticleEngine engine(&level,&textures);
 glViewport(0,0,400,400);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(-.4f,.4f,-.4f,.4f,-4,4);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glClearColor(0,0,0,1);Particle::xOff=Particle::yOff=Particle::zOff=0;
 auto render=[&](Particle& particle,const char* name){glMatrixMode(GL_PROJECTION);glLoadIdentity();const float span=std::string(name)=="flash"?8.f:.4f;glOrthof(-span,span,-span,span,-4,4);glMatrixMode(GL_MODELVIEW);Particle::xOff=particle.x;Particle::yOff=particle.y;Particle::zOff=particle.z;glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glDisable2(GL_TEXTURE_2D);GLint binding;glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding);glDisable2(GL_BLEND);glEnable2(GL_CULL_FACE);glEnable2(GL_ALPHA_TEST);glColor4f2(.2,.3,.4,.5);particle.render(Tesselator::instance,1,1,1,0,0,0);assert(!glIsEnabled(GL_TEXTURE_2D)&&!glIsEnabled(GL_BLEND)&&glIsEnabled(GL_CULL_FACE)&&glIsEnabled(GL_ALPHA_TEST));GLint afterBinding;glGetIntegerv(GL_TEXTURE_BINDING_2D,&afterBinding);assert(afterBinding==binding);GLfloat color[4];glGetFloatv(GL_CURRENT_COLOR,color);assert(std::abs(color[0]-.2)<1e-5);glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> pixels(400*400*4);glReadPixels(0,0,400,400,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());size_t lit=0;for(size_t i=0;i<pixels.size();i+=4)if(pixels[i]+pixels[i+1]+pixels[i+2]>30)++lit;assert(lit>100);std::vector<unsigned char> flipped(pixels.size());for(int row=0;row<400;++row)std::copy(pixels.begin()+row*1600,pixels.begin()+(row+1)*1600,flipped.begin()+(399-row)*1600);png_image image{};image.version=PNG_IMAGE_VERSION;image.width=image.height=400;image.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&image,("../headless-firework-effect-"+std::string(name)+".png").c_str(),0,flipped.data(),0,nullptr));return pixels;};
 JavaFireworkParticles::Spark spark(nullptr,&textures,&level,0,0,0,{{0,0,0}});spark.configure(0xff0000,false,false);spark.fade(0x00ff00);auto initial=render(spark,"red-spark");auto energy=[](const std::vector<unsigned char>& frame,int channel){uint64_t sum=0;for(size_t i=channel;i<frame.size();i+=4)sum+=frame[i];return sum;};assert(energy(initial,0)>energy(initial,1)*10);for(int i=0;i<35;++i)spark.tick();auto faded=render(spark,"faded-spark");assert(initial!=faded&&energy(faded,1)>energy(faded,0));for(int i=0;i<64;++i)spark.tick();assert(spark.removed);
 JavaFireworkParticles::Flash flash(&textures,&level,0,0,0,0x00ffff);flash.tick();auto first=render(flash,"flash");assert(energy(first,1)>energy(first,0)&&energy(first,2)>energy(first,0));for(int i=0;i<5;++i)flash.tick();assert(flash.removed);
 for(const char* shape:{"small_ball","large_ball","star","creeper","burst"}){JavaGameSession::VisualEffect effect;effect.kind="firework_explosion";effect.position={{0,0,0}};effect.explosions.push_back(JavaGameSession::FireworkExplosion{});effect.explosions.back().shape=shape;effect.explosions.back().colors={0xff0000};effect.explosions.back().fadeColors={0x00ff00};effect.explosions.back().trail=true;effect.explosions.back().twinkle=true;auto* starter=new JavaFireworkParticles::Starter(&engine,&textures,&level,effect);engine.add(starter);engine.tick();const auto initialCount=JavaFireworkParticles::live();assert(initialCount>10);bool retainedConstructorSprite=false;for(int tick=0;tick<8;++tick){engine.tick();for(auto* p:(engine.*field(EngineListTag{}))[ParticleEngine::ENTITY_PARTICLE_TEXTURE])if(auto* child=dynamic_cast<JavaFireworkParticles::Spark*>(p))if(child->*field(ParticleAgeTag{})==child->*field(ParticleLifeTag{})/2&&child->*field(SparkSpriteTag{})==7)retainedConstructorSprite=true;}assert(retainedConstructorSprite&&JavaFireworkParticles::live()>initialCount);engine.setLevel(&level);}
 assert(JavaFireworkParticles::live()==2);std::cout<<"HEADLESS EGL pbuffer: actual official firework Spark/Flash textures, RGB fade/motion/lifetime,5shape emitter dispatch, child-trail ownership/cleanup and GLstate PASS; no window created"<<std::endl;std::_Exit(0);
}
