#include "WeatherLightningRuntime.h"
#include "WeatherEcology.h"
#include "Level.h"
#include "dimension/Dimension.h"
#include "tile/Tile.h"
#include "tile/FireTile.h"
#include "../entity/Entity.h"
#include "../entity/player/Player.h"
#include "../entity/animal/Pig.h"
#include "../entity/monster/PigZombie.h"
#include "../entity/monster/Creeper.h"
#include "../../network/RakNetInstance.h"
#include "../../network/packet/WeatherLightningPacket.h"
#include <algorithm>
namespace WeatherLightningRuntime {
namespace {
uint64_t seed(Level& level) {
    return (uint64_t(level.random.nextInt())<<33) ^ (uint64_t(level.random.nextInt())<<2) ^ uint64_t(level.random.nextInt(4));
}
bool enabled(Level& level) {return !level.isJavaServerLevel() && level.dimension && !level.dimension->hasCeiling;}
void send(Level& level,const WeatherLightning& bolt,unsigned char phase) {
    if(!level.isClientSide && level.raknetInstance) {WeatherLightningPacket packet(bolt,phase);level.raknetInstance->send(packet);}
}
bool neighborhoodLoaded(Level& level,int x,int z) {
    for(int cx=Mth::floor((x-10)/16.f);cx<=Mth::floor((x+10)/16.f);++cx)
        for(int cz=Mth::floor((z-10)/16.f);cz<=Mth::floor((z+10)/16.f);++cz)
            if(!level.hasChunkNow(cx,cz))return false;
    return true;
}
bool tracked(Level& level,int x,int z) {
    if(!level.hasChunkNow(Mth::floor(x/16.f),Mth::floor(z/16.f)))return false;
    // Native TCPE synchronizes already-loaded chunks in the active player area.
    // No absent chunk may be generated merely to place strike fire.
    for(Player* player:level.players)if(player && !player->removed &&
        std::abs(Mth::floor(player->x/16.f)-Mth::floor(x/16.f))<=4 &&
        std::abs(Mth::floor(player->z/16.f)-Mth::floor(z/16.f))<=4)return true;
    return false;
}
void fireAt(Level& level,int x,int y,int z) {
    if(y<=0 || y>=LEVEL_HEIGHT || !Tile::fire || !tracked(level,x,z))return;
    // Fire placement checks adjacent material; guard those lookups as well.
    for(int dx=-1;dx<=1;++dx)for(int dz=-1;dz<=1;++dz)
        if(!level.hasChunkAtNow(x+dx,y,z+dz))return;
    if(WeatherLightning::fireAtAllowed(true,level.getTile(x,y,z)==0,
        Tile::fire->mayPlace(&level,x,y,z,1)))level.setTile(x,y,z,Tile::fire->id);
}
void hit(Level& level,const WeatherLightning& bolt) {
    const AABB area(bolt.x-3,bolt.y-3,bolt.z-3,bolt.x+3,bolt.y+9,bolt.z+3);
    // Copy before replacement: addEntity may invalidate Level's scratch list.
    const EntityList entities=level.getEntities(nullptr,area);
    for(Entity* entity:entities) {
        if(!entity || entity->removed)continue;
        if(Pig* pig=dynamic_cast<Pig*>(entity)) {
            PigZombie* zombie=new PigZombie(&level);
            zombie->moveTo(pig->x,pig->y,pig->z,pig->yRot,pig->xRot);
            if(level.addEntity(zombie))pig->remove();else delete zombie;
        } else {
            entity->thunderHit();
            if(Creeper* creeper=dynamic_cast<Creeper*>(entity))creeper->setPowered(true);
        }
    }
}
}
bool spawn(Level& level,double x,double y,double z) {
    if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || std::abs(x)>30000000.0 || std::abs(z)>30000000.0)return false;
    if(level.isClientSide || !enabled(level) || level.weatherLightning.size()>=64 ||
        y<0 || y>=LEVEL_HEIGHT || !level.hasChunkAtNow(Mth::floor(x),Mth::floor(y),Mth::floor(z)))return false;
    WeatherLightning bolt;bolt.x=x;bolt.y=y;bolt.z=z;
    bolt.id=level.nextWeatherLightningId++;if(!bolt.id)bolt.id=level.nextWeatherLightningId++;
    bolt.seed=seed(level);if(!bolt.valid())return false;
    level.weatherLightning.push_back(bolt);send(level,bolt,0);
    const int bx=Mth::floor(x),by=Mth::floor(y),bz=Mth::floor(z);
    if(WeatherLightning::initialFireAllowed(true,level.difficulty,neighborhoodLoaded(level,bx,bz))) {
        fireAt(level,bx,by,bz);
        for(int n=0;n<4;++n)fireAt(level,bx+level.random.nextInt(3)-1,
            by+level.random.nextInt(3)-1,bz+level.random.nextInt(3)-1);
    }
    return true;
}
void tickChunk(Level& level,int cx,int cz) {
    if(level.isClientSide || !enabled(level) || !level.hasChunkNow(cx,cz))return;
    // Match reference chance ordering: draw once per eligible active chunk.
    if(level.random.nextInt(100000)!=0 || !level.getLevelData()->weather.wet() ||
        !level.getLevelData()->weather.storm())return;
    const int x=cx*16+level.random.nextInt(16),z=cz*16+level.random.nextInt(16);
    const int y=level.getTopRainBlock(x,z);
    if(WeatherEcology::exposedToRain(level,x,y,z))spawn(level,x,y,z);
}
void tick(Level& level) {
    if(level.lightningBoltTime>0)--level.lightningBoltTime;
    if(!enabled(level)){level.weatherLightning.clear();return;}
    for(WeatherLightning& bolt:level.weatherLightning) {
        const auto effects=bolt.tick(!level.isClientSide,[&](int n){return level.random.nextInt(n);},[&](){return seed(level);});
        if(effects.sound) {
            // SoundSystem clamps its native gain. Use the reference admission
            // volume here; the parent renderer/audio path must verify distance.
            level.playSound(bolt.x,bolt.y,bolt.z,"ambient.weather.thunder",10000.f,.8f+level.random.nextFloat()*.2f);
            level.playSound(bolt.x,bolt.y,bolt.z,"random.explode",2.f,.5f+level.random.nextFloat()*.2f);
        }
        if(effects.illuminate)level.lightningBoltTime=2;
        if(effects.hitEntities)hit(level,bolt);
        if(!level.isClientSide && effects.retryFire) {
            const int x=Mth::floor(bolt.x),y=Mth::floor(bolt.y),z=Mth::floor(bolt.z);
            if(neighborhoodLoaded(level,x,z))fireAt(level,x,y,z);
            send(level,bolt,1);
        }
        if(!level.isClientSide && effects.removed)send(level,bolt,2);
        // A disconnected client must not retain an invisible event forever.
        if(level.isClientSide && bolt.life < -40)bolt.removed=true;
    }
    auto& bolts=level.weatherLightning;
    bolts.erase(std::remove_if(bolts.begin(),bolts.end(),[](const WeatherLightning& b){return b.removed;}),bolts.end());
}
bool receive(Level& level,const WeatherLightning& bolt,unsigned char phase) {
    if(!level.isClientSide || !enabled(level) || !bolt.valid() || phase>2)return false;
    auto& bolts=level.weatherLightning;
    auto it=std::find_if(bolts.begin(),bolts.end(),[&](const WeatherLightning& b){return b.id==bolt.id;});
    if(phase==2){if(it!=bolts.end())bolts.erase(it);return true;}
    if(phase==0) {
        if(bolt.life!=2 || bolt.flashes!=1 || it!=bolts.end() || bolts.size()>=64)return false;
        auto& seen=level.weatherLightningSeen;
        if(std::find(seen.begin(),seen.end(),bolt.id)!=seen.end())return false;
        seen.push_back(bolt.id);if(seen.size()>128)seen.pop_front();
        bolts.push_back(bolt);return true;
    }
    if(it==bolts.end() || bolt.life!=1 || bolt.flashes!=0 || it->flashes==0)return false;
    // Coordinates are immutable for an existing strike ID.
    if(it->x!=bolt.x || it->y!=bolt.y || it->z!=bolt.z)return false;
    *it=bolt;return true;
}
}
