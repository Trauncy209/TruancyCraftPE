#ifndef TCPE_JAVA_LEVEL_H
#define TCPE_JAVA_LEVEL_H
#include "client/multiplayer/MultiPlayerLevel.h"
#include "JavaChunkStore.h"
class JavaLevel:public MultiPlayerLevel {
public:
 JavaLevel(LevelStorage* storage,const JavaChunkStore& initial);
 bool isJavaServerLevel()const override{return true;}
 int minBuildHeight()const override{return minY;}
 int maxBuildHeight()const override{return maxY;}
 int getTile(int x,int y,int z)override;
 EntityList& getEntities(Entity* except,const AABB& bounds)override;
 int getData(int x,int y,int z)override;
 float getBrightness(int x,int y,int z)override;
 int getRawBrightness(int x,int y,int z)override;
 int getRawBrightness(int x,int y,int z,bool propagate)override;
 int getBrightness(const LightLayer& layer,int x,int y,int z)override;
 void synchronize(const JavaChunkStore& chunks);
 void tick()override {updateSkyDarken();}
 const JavaChunkStore& serverChunks()const{return view;}
private:
 EntityList queriedEntities;
 JavaChunkStore view;int minY,maxY;
};
#endif
