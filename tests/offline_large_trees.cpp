#include "world/level/Level.h"
#include "world/level/LevelSettings.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/level/levelgen/feature/BasicTree.h"
#include "world/level/levelgen/feature/TreeFeature.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include "util/Mth.h"
#include <cassert>
#include <iostream>
#include <cstdlib>
static void clear(Level* level){for(int x=12;x<=36;++x)for(int z=12;z<=36;++z)for(int y=81;y<Level::DEPTH;++y)level->setTileNoUpdate(x,y,z,0);level->setTileNoUpdate(24,80,24,Tile::grass->id);}
static int top(Level* level){int highest=80;for(int x=12;x<=36;++x)for(int z=12;z<=36;++z)for(int y=81;y<Level::DEPTH;++y)if(level->getTile(x,y,z)==Tile::treeTrunk->id||level->getTile(x,y,z)==Tile::leaves->id)highest=std::max(highest,y);return highest;}
int main(){std::cout.setf(std::ios::unitbuf);Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();auto* level=new Level(new MemoryLevelStorage(),"Tree CPU fixture",LevelSettings(1,GameType::Creative,LGV_ORIGINAL),LGV_ORIGINAL);for(Biome* biome:{Biome::seasonalForest,Biome::forest,Biome::rainForest}){Random choice(12345);int bigCount=0,smallCount=0;for(int n=0;n<1000;++n){Feature* f=biome->getTreeFeature(&choice);bigCount+=dynamic_cast<BasicTree*>(f)!=nullptr;smallCount+=dynamic_cast<TreeFeature*>(f)!=nullptr;delete f;}assert(bigCount>0&&smallCount>0);std::cout<<"biome="<<biome->name<<" big="<<bigCount<<" small="<<smallCount<<'\n';}BasicTree big(false);big.init(1,1,1);int tall=0,branched=0;for(int seed=1;seed<=16;++seed){clear(level);Random random(seed);assert(big.place(level,&random,24,81,24));int highest=top(level);if(highest>87)++tall;int branches=0;for(int x=12;x<=36;++x)for(int z=12;z<=36;++z)if(x!=24||z!=24)for(int y=81;y<Level::DEPTH;++y)if(level->getTile(x,y,z)==Tile::treeTrunk->id)++branches;if(branches>0)++branched;std::cout<<"seed="<<seed<<" height="<<highest-80<<" branchBlocks="<<branches<<'\n';}assert(tall>=8&&branched>=8);clear(level);Random blocked(7);level->setTileNoUpdate(24,81,24,Tile::rock->id);assert(!big.place(level,&blocked,24,81,24));assert(level->getTile(24,81,24)==Tile::rock->id);assert(!big.place(level,&blocked,24,Level::DEPTH-2,24));assert(!big.place(level,&blocked,24,0,24));clear(level);TreeFeature small(false);Random sr(4);assert(small.place(level,&sr,24,81,24));assert(top(level)<=87);std::cout<<"PASS actual offline Level: varied tall branched trees, instance reuse, obstacle/height rejection, retained small trees; no window or saved world\n";std::_Exit(0);}
