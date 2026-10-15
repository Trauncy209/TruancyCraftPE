#include "world/level/Level.h"
#include "world/level/tile/LeafTile.h"
#include "world/level/LevelSettings.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/level/biome/BiomeSource.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include "util/Mth.h"
#include <cassert>
#include <iostream>
#include <set>
#include <queue>
#include <array>
#include <map>
#include <cstdlib>
using Position=std::array<int,3>;
struct Totals{int oak=0,branchedOak=0,tallBirch=0,tallSpruce=0,smallOak=0;};
int main(int argc,char** argv){if(argc!=2||(std::string(argv[1])!="Forest"&&std::string(argv[1])!="Rainforest"&&std::string(argv[1])!="Taiga")){std::cerr<<"Use one isolated memory world per process: Forest, Rainforest or Taiga\n";return 2;}std::cout.setf(std::ios::unitbuf);Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Totals total;
for(Biome* wanted:{Biome::forest,Biome::rainForest,Biome::taiga}){
 if(argc>1&&wanted->name!=argv[1])continue;
 auto* level=new Level(new MemoryLevelStorage(),"Natural tree population fixture",LevelSettings(1,GameType::Creative,LGV_INFINITE,false,false,false,false,false,false,true,true,true),LGV_INFINITE);
 int cx=0,cz=0;bool found=false;
 for(int z=-64;z<=64&&!found;++z)for(int x=-64;x<=64&&!found;++x)if(level->getBiomeSource()->getBiome(x*16+16,z*16+16)==wanted){cx=x;cz=z;found=true;}
 assert(found);std::cout<<"seed=1 biome="<<wanted->name<<" chunk="<<cx<<","<<cz<<"\n";
 auto* source=level->getChunkSource();for(int z=cz;z<cz+9;++z)for(int x=cx;x<cx+9;++x)source->getChunk(x,z);
 int populated=0;for(int z=cz;z<cz+8;++z)for(int x=cx;x<cx+8;++x)populated+=source->getChunk(x,z)->terrainPopulated;assert(populated==64);
 std::set<Position> logs;for(int z=cz*16;z<(cz+8)*16;++z)for(int x=cx*16;x<(cx+8)*16;++x)for(int y=1;y<Level::DEPTH;++y)if(level->getTile(x,y,z)==Tile::treeTrunk->id)logs.insert({{x,y,z}});
 std::cout<<"automatic ChunkCache->RandomLevelSource postProcess populated="<<populated<<" logs="<<logs.size()<<"\n";
 while(!logs.empty()){
  Position first=*logs.begin();int metadata=level->getData(first[0],first[1],first[2])&3,minY=first[1],maxY=first[1],count=0;std::set<std::pair<int,int>> columns;std::queue<Position> pending;pending.push(first);logs.erase(first);
  while(!pending.empty()){auto p=pending.front();pending.pop();++count;minY=std::min(minY,p[1]);maxY=std::max(maxY,p[1]);columns.insert({p[0],p[2]});for(int dz=-1;dz<=1;++dz)for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){Position n={{p[0]+dx,p[1]+dy,p[2]+dz}};auto it=logs.find(n);if(it!=logs.end()&&(level->getData(n[0],n[1],n[2])&3)==metadata){pending.push(n);logs.erase(it);}}}
  int canopyTop=maxY;int minX=first[0],maxX=first[0],minZ=first[2],maxZ=first[2];for(auto column:columns){minX=std::min(minX,column.first);maxX=std::max(maxX,column.first);minZ=std::min(minZ,column.second);maxZ=std::max(maxZ,column.second);for(int y=maxY;y<std::min<int>(Level::DEPTH,maxY+8);++y)if(level->getTile(column.first,y,column.second)==Tile::leaves->id)canopyTop=std::max(canopyTop,y);}const int canopyHeight=canopyTop-minY+1;const int height=maxY-minY+1;
  if(metadata==0){++total.oak;if(columns.size()>1&&height>=7){++total.branchedOak;std::cout<<"natural branched oak woodHeight="<<height<<" canopyHeight="<<canopyHeight<<" woodWidth="<<maxX-minX+1<<"x"<<maxZ-minZ+1<<" offAxisColumns="<<columns.size()-1<<" wood="<<count<<"\n";}if(columns.size()==1&&height<=6)++total.smallOak;}
  if(metadata==2&&height>=9){++total.tallBirch;std::cout<<"natural tall birch woodHeight="<<height<<"\n";}
  if(metadata==1&&height>=11){++total.tallSpruce;std::cout<<"natural tall spruce woodHeight="<<height<<"\n";}
 }
}
std::cout<<"totals oak="<<total.oak<<" branched="<<total.branchedOak<<" small="<<total.smallOak<<" tallBirch="<<total.tallBirch<<" tallSpruce="<<total.tallSpruce<<"\n";if(argc>1){if(std::string(argv[1])=="Taiga")assert(total.tallSpruce>0);else {assert(total.branchedOak>0&&total.smallOak>0);if(std::string(argv[1])=="Rainforest")assert(total.tallBirch>0);}}else assert(total.branchedOak>0&&total.smallOak>0&&total.tallBirch>0&&total.tallSpruce>0);std::cout<<"PASS actual Level + ChunkCache automatic natural RandomLevelSource population, no manual feature placement, no window or saved world\n";std::_Exit(0);}
