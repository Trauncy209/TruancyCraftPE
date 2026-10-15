#define main populationFixtureMain
#include "native_snapshot_population.cpp"
#undef main
#include "world/level/storage/ExternalFileLevelStorage.h"
#include <chrono>
int main(int argc,char**argv){Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();auto*l=new Level(new ExternalFileLevelStorage("qa",argv[1]),"qa",LevelSettings(1792046386,GameType::Survival,LGV_INFINITE),LGV_INFINITE);for(int x=-3;x<=-1;++x)for(int z=0;z<=2;++z)l->getChunk(x,z);int before=l->getLightsToUpdate(),rounds=0;auto start=std::chrono::steady_clock::now();while(l->updateLights()){assert(++rounds<2000);}uint64_t hash=1469598103934665603ULL;for(int x=-3;x<=-1;++x)for(int z=0;z<=2;++z){auto*c=l->getChunk(x,z);for(int i=0;i<16384;++i){hash=(hash^c->skyLight.data[i])*1099511628211ULL;hash=(hash^c->blockLight.data[i])*1099511628211ULL;}}std::cout<<"LIGHTING queue="<<before<<" rounds="<<rounds<<" ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<" hash="<<hash<<std::endl;}
