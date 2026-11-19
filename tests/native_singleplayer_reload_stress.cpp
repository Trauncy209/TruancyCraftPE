#include "world/level/Level.h"
#include "world/level/storage/ExternalFileLevelStorage.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/entity/ChestTileEntity.h"
#include "world/level/tile/entity/FurnaceTileEntity.h"
#include "world/level/levelgen/feature/BasicTree.h"
#include "world/entity/MobCategory.h"
#include "world/entity/MobFactory.h"
#include "world/entity/item/ItemEntity.h"
#include "world/entity/player/Player.h"
#include "world/entity/player/Inventory.h"
#include "world/item/Item.h"
#include "world/level/biome/Biome.h"
#include "nbt/CompoundTag.h"
#include "util/Mth.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
struct State { uint64_t blocks;int entities,phase; };
static uint64_t hashChunk(LevelChunk* chunk,uint64_t hash) {
 for(int i=0;i<32768;++i){hash^=chunk->getBlockData()[i];hash*=1099511628211ULL;}
 for(int i=0;i<16384;++i){hash^=chunk->data.data[i];hash*=1099511628211ULL;}
 return hash;
}
static uint64_t fingerprint(Level* level,int first){uint64_t hash=1469598103934665603ULL;for(int x=first;x<first+5;++x)for(int z=first;z<first+5;++z)hash=hashChunk(level->getChunk(x,z),hash);return hash;}
static int savedEntities(Level* level){int n=0;for(auto* entity:level->entities){CompoundTag tag;if(entity->save(&tag))++n;tag.deleteChildren();}return n;}
static ChestTileEntity* chest(Level* level,int i){auto* te=dynamic_cast<ChestTileEntity*>(level->getTileEntity(8+i*3,95,8));assert(te);return te;}
static FurnaceTileEntity* furnace(Level* level,int i){auto* te=dynamic_cast<FurnaceTileEntity*>(level->getTileEntity(8+i*3,95,12));assert(te);return te;}
static int run(const std::string& dir,int gen,int phase) {
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();TileEntity::initTileEntities();
 LevelSettings settings(771234,GameType::Survival,gen,true,true,true,true,true,true,true,true,false,true,true);
 auto* store=new ExternalFileLevelStorage("stress",dir);auto* level=new Level(store,"stress",settings,gen);level->setUpdateLights(false);const int first=gen==LGV_INFINITE?-1:0;
 for(int x=first;x<first+5;++x)for(int z=first;z<first+5;++z)level->getChunk(x,z);
 auto* player=new Player(level,false);player->name="ReloadAudit";
 if(phase==0){
  for(int x=8;x<60;++x)for(int z=8;z<60;++z){for(int y=65;y<128;++y)level->setTileNoUpdate(x,y,z,0);level->setTileNoUpdate(x,64,z,Tile::grass->id);}
  BasicTree tree(false);tree.init(1,1,1);for(int i=0;i<3;++i){Random random(123+i);assert(tree.place(level,&random,20+(i==1?24:0),65,20+(i==2?24:0)));}
  const int types[]={10,11,12,13,32,33,34,35,36};for(int type:types)for(int i=0;i<3;++i){auto* mob=MobFactory::CreateMob(type,level);assert(mob);mob->setPos(5+i*2,80,5+type%8);level->addEntity(mob);}
  for(int i=0;i<12;++i){auto* drop=new ItemEntity(level,6+i,80,6,ItemInstance(17,3+i,0));level->addEntity(drop);}
  for(int i=0;i<4;++i){level->setTile(8+i*3,95,8,Tile::chest->id);level->setTile(8+i*3,95,12,Tile::furnace_lit->id);assert(chest(level,i)&&furnace(level,i));}
  player->setPos(gen==LGV_INFINITE?-1.25f:24.5f,80.62f,24.5f);player->health=17;level->addEntity(player);
 } else {
  State expected{};std::ifstream input(dir+"/audit-state.bin",std::ios::binary);input.read((char*)&expected,sizeof(expected));assert(input&&expected.phase==phase-1);
  assert(fingerprint(level,first)==expected.blocks);
  store->loadEntities(level,NULL);assert(savedEntities(level)==expected.entities);
  level->loadPlayer(player,true);assert(player->health==17&&player->inventory->selected==5);
  assert(!player->inventory->getLinked(0)&&player->inventory->getLinked(5)&&player->inventory->getLinked(5)->count==20+phase-1);
  assert(player->inventory->getLinked(2)&&player->inventory->getLinked(2)->count==7+phase-1);
  const char* previous=(phase-1)%3==0?"thunder":(phase-1)%3==1?"rain":"clear";auto& weather=level->getLevelData()->weather;
  assert(weather.raining==(std::string(previous)!="clear")&&weather.thundering==(std::string(previous)=="thunder")&&weather.rainTicks==1200+(phase-1)*20);
  for(int i=0;i<4;++i){assert(chest(level,i)->getItem(0)&&chest(level,i)->getItem(0)->count==11+i+phase-1);assert(furnace(level,i)->getItem(1)->count==8+i+phase-1);assert(furnace(level,i)->tickCount==10+phase-1);}
 }
 // Thousands of real block edits span chunk/region boundaries, including negative coordinates.
 for(int i=0;i<4096;++i){int x=first*16+i%80,z=first*16+(i/80)%80,y=20+(i*7)%35;level->setTileAndDataNoUpdate(x,y,z,(i+phase)%2?Tile::rock->id:Tile::glass->id,0);}
 player->inventory->clearInventory();ItemInstance wood(17,20+phase,0),dirt(3,7+phase,0);player->inventory->setItem(9,&wood);player->inventory->setItem(15,&dirt);player->inventory->moveToSelectionSlot(5,9,false);player->inventory->moveToSelectionSlot(2,15,false);player->inventory->selectSlot(5);
 for(int i=0;i<4;++i){ItemInstance logs(17,11+i+phase,0),ore(15,4+i,0),coal(263,8+i+phase,0),ingots(265,2+i,0);chest(level,i)->setItem(0,&logs);furnace(level,i)->setItem(0,&ore);furnace(level,i)->setItem(1,&coal);furnace(level,i)->setItem(2,&ingots);furnace(level,i)->tickCount=10+phase;furnace(level,i)->litTime=100;furnace(level,i)->litDuration=1600;}
 auto& weather=level->getLevelData()->weather;assert(weather.command(phase%3==0?"thunder":phase%3==1?"rain":"clear",1200+phase*20));
 State state{fingerprint(level,first),savedEntities(level),phase};level->saveLevelData();store->saveGame(level);level->getChunkSource()->saveAll(true);
 std::ofstream output(dir+"/audit-state.bin",std::ios::binary|std::ios::trunc);output.write((char*)&state,sizeof(state));output.close();assert(output);
 delete level;delete store;
 std::cout<<"Modified "<<(gen==LGV_INFINITE?"infinite":"finite")<<" world phase "<<phase<<": exact block/meta hash, mobs/drops, 8 storage inventories, player/hotbar and weather PASS\n";return 0;
}
int main(int argc,char** argv){
 if(argc==4)return run(argv[1],std::stoi(argv[2]),std::stoi(argv[3]));assert(argc==1);
 for(int gen:{LGV_ORIGINAL,LGV_INFINITE}){char dir[]="/tmp/tcpe-modified-world-XXXXXX";assert(mkdtemp(dir));for(int phase=0;phase<4;++phase){pid_t pid=fork();assert(pid>=0);if(!pid){std::string g=std::to_string(gen),p=std::to_string(phase);execl(argv[0],argv[0],dir,g.c_str(),p.c_str(),(char*)NULL);_exit(127);}int status=0;assert(waitpid(pid,&status,0)==pid&&WIFEXITED(status)&&WEXITSTATUS(status)==0);}}
 std::cout<<"Separate-process modified finite/infinite survival save/reopen stress PASS\n";
}
