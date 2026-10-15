#include "WeatherEffects.h"
#include "../Minecraft.h"
#include "../particle/Particle.h"
#include "../particle/ParticleEngine.h"
#include "../particle/SmokeParticle.h"
#include "../sound/SoundEngine.h"
#include "../../world/entity/Mob.h"
#include "../../world/level/Level.h"
#include "../../world/level/storage/LevelData.h"
#include "../../world/level/biome/Biome.h"
#include "../../world/level/dimension/Dimension.h"
#include "../../world/level/material/Material.h"
#include "../../world/level/tile/Tile.h"

namespace {
// Independent short-lived impact using the existing TCPE particle atlas.
// A downward drop with collision; no new assets and no server entities.
class RainImpactParticle : public Particle {
public:
    RainImpactParticle(Level* level, float x, float y, float z)
        : Particle(level, x, y, z, 0, 0, 0) {
        // Particle randomness remains separate from the deterministic column
        // sampler, as in the reference's per-particle/Math random streams.
        tex = 19 + sharedRandom.nextInt(4);
        xd *= .3f; zd *= .3f;
        yd = Mth::random()*.2f+.1f;
        rCol=gCol=bCol=1.f;
        setSize(.01f,.01f);
        gravity=.06f;
        lifetime=WeatherImpactPolicy::lifetime(Mth::random());
        noPhysics=true;
    }
    void tick() {
        xo=x;yo=y;zo=z;
        yd-=gravity;
        move(xd,yd,zd);
        xd*=.98f;yd*=.98f;zd*=.98f;
        if(lifetime--<=0)remove();
        if(onGround) {
            if(Mth::random()<.5f)remove();
            xd*=.7f;zd*=.7f;
        }
        const int bx=Mth::floor(x),by=Mth::floor(y),bz=Mth::floor(z);
        // Do not load a neighboring chunk just to expire a visual particle.
        if(!level->hasChunkAtNow(bx,by,bz)){remove();return;}
        const Material* material=level->getMaterial(bx,by,bz);
        if((material->isLiquid()||material->isSolid()) &&
            WeatherImpactPolicy::submerged(y,by,level->getData(bx,by,bz),true))remove();
    }
};
}

void WeatherEffects::tick(Minecraft& mc) {tick(mc,tickCounter+1);}

void WeatherEffects::tick(Minecraft& mc,uint32_t rendererTick) {
    Level* level = mc.level;
    if (previousLevel != level) {
        previousLevel = level; rainSoundTime = 0; tickCounter = 0;
    }
    tickCounter=rendererTick;
    Mob* camera = mc.cameraTargetPlayer;
    if (!level || !camera || !mc.particleEngine || level->isJavaServerLevel() ||
        !level->dimension || level->dimension->hasCeiling) return;
    int attempts = WeatherImpactPolicy::attempts(level->getLevelData()->weather.rainAt(1.f), mc.options.fancyGraphics);
    if (!attempts) { rainSoundTime = 0; return; }
    // Tick-local seed keeps sampling reproducible without disturbing world RNG.
    random.setSeed(WeatherImpactPolicy::tickSeed(tickCounter));
    const int cx = Mth::floor(camera->x), cy = Mth::floor(camera->y), cz = Mth::floor(camera->z);
    float soundX = 0, soundY = 0, soundZ = 0;
    int samples = 0;
    for (int i = 0; i < attempts; ++i) {
        const int xPositive=random.nextInt(10),xNegative=random.nextInt(10);
        const int zPositive=random.nextInt(10),zNegative=random.nextInt(10);
        const int x=cx+xPositive-xNegative,z=cz+zPositive-zNegative;
        if (!level->hasChunkAtNow(x, 64, z)) continue;
        Biome* biome = level->getBiome(x, z);
        if (!biome || !biome->hasRain()) continue;
        const int surface = level->getTopRainBlock(x, z);
        if (!WeatherImpactPolicy::eligible(true, true, surface, cy)) continue;
        const int tileId = level->getTile(x, surface - 1, z);
        if (tileId <= 0 || tileId >= 256 || !Tile::tiles[tileId]) continue;
        Tile* tile = Tile::tiles[tileId];
        const float px = x + random.nextFloat(), pz = z + random.nextFloat();
        // Top surface accounts for slabs, snow layers and other partial blocks.
        tile->updateShape(level,x,surface-1,z);
        const float py = WeatherImpactPolicy::impactHeight(surface,tile->yy1);
        if (tile->material == Material::lava) {
            mc.particleEngine->add(new SmokeParticle(level, px, py, pz, 0, 0, 0));
        } else {
            mc.particleEngine->add(new RainImpactParticle(level, px, py, pz));
            if (random.nextInt(++samples) == 0) {
                soundX = px; soundY = py; soundZ = pz;
            }
        }
    }
    if (samples && random.nextInt(3) < rainSoundTime++) {
        rainSoundTime = 0;
        const bool sheltered = WeatherImpactPolicy::sheltered(soundY, camera->y,
            level->getTopRainBlock(cx, cz), cy);
        // Canonical rain audio already imported from the user's Java assets.
        // Same event as the reference, with roof-dependent gain and pitch.
        if (mc.soundEngine) mc.soundEngine->playServerSound("@play|3|minecraft:weather.rain",
            soundX, soundY, soundZ, sheltered ? .1f : .2f, sheltered ? .5f : 1.f,
            uint64_t(tickCounter));
    }
}
