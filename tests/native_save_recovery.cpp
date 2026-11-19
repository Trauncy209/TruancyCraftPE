#include "world/level/Level.h"
#include "world/level/storage/ExternalFileLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/entity/MobCategory.h"
#include "world/entity/MobFactory.h"
#include "world/entity/EntityFactory.h"
#include "world/entity/animal/Cow.h"
#include "world/entity/player/Player.h"
#include "world/item/Item.h"
#include "world/level/biome/Biome.h"
#include "util/Mth.h"
#include "util/RakDataIO.h"
#include "nbt/NbtIo.h"
#include "../src/nbt/NativeNbtValidation.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <limits>
#include <dirent.h>
#include <unistd.h>
static void badEntities(const std::string& dir) {
 std::ofstream file(dir+"/entities.dat",std::ios::binary|std::ios::trunc);
 const int version=1,size=4; const unsigned char body[]={1,0,0,0}; // Valid byte tag, wrong root type.
 file.write("ENT\0",4);file.write((const char*)&version,4);file.write((const char*)&size,4);file.write((const char*)body,4);
}
static int quarantines(const std::string& dir){int n=0;DIR* d=opendir(dir.c_str());assert(d);while(auto* e=readdir(d))if(std::string(e->d_name).find("entities.dat.corrupt-")==0)++n;closedir(d);return n;}
int main(){
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
 char scratch[]="/tmp/tcpe-native-recovery-XXXXXX";assert(mkdtemp(scratch));const std::string dir=scratch;
 auto* store=new ExternalFileLevelStorage("recovery",dir);
 auto* level=new Level(store,"recovery",LevelSettings(1234,GameType::Survival,LGV_ORIGINAL),LGV_ORIGINAL);level->setUpdateLights(false);
 badEntities(dir);store->loadEntities(level,NULL);assert(level->entities.empty());
 auto* cow=new Cow(level);cow->setPos(40,70,40);level->addEntity(cow);
 store->saveEntities(level,NULL);assert(quarantines(dir)==1);
 store->saveEntities(level,NULL);assert(access((dir+"/entities.dat_old").c_str(),R_OK)==0);
 auto& weather=level->getLevelData()->weather;assert(weather.command("thunder",1200));
 store->saveLevelData(*level->getLevelData(),NULL);store->saveLevelData(*level->getLevelData(),NULL);
 assert(!ExternalFileLevelStorage::writeLevelData("/dev/full",*level->getLevelData(),NULL));
 badEntities(dir);
 auto* restoredStore=new ExternalFileLevelStorage("recovery",dir);
 auto* restored=new Level(restoredStore,"recovery",LevelSettings(9,GameType::Creative,LGV_ORIGINAL),LGV_ORIGINAL);restored->setUpdateLights(false);
 assert(restored->getSeed()==1234&&restored->getLevelData()->weather.thundering&&restored->getLevelData()->weather.rainTicks==1200);
 restoredStore->loadEntities(restored,NULL);assert(restored->entities.size()==1&&restored->entities[0]->getEntityTypeId()==11);
 restoredStore->saveEntities(restored,NULL);assert(quarantines(dir)==2);
 CompoundTag tag;cow->save(&tag);auto* pos=tag.getList("Pos");static_cast<FloatTag*>(pos->get(0))->data=std::numeric_limits<float>::quiet_NaN();
 assert(EntityFactory::loadEntity(&tag,restored)==NULL);tag.deleteChildren();
 CompoundTag valid;valid.putString("name","weather reload");valid.putInt("ticks",123);
 RakNet::BitStream encoded;RakDataOutput output(encoded);NbtIo::write(&valid,&output);valid.deleteChildren();
 const unsigned char* bytes=encoded.GetData();size_t size=encoded.GetNumberOfBytesUsed();assert(NativeNbtValidation::compound(bytes,size));
 for(size_t n=0;n<size;++n)assert(!NativeNbtValidation::compound(bytes,n));
 const unsigned char negativeArray[]={10,0,0,7,0,0,255,255,255,255,0};assert(!NativeNbtValidation::compound(negativeArray,sizeof(negativeArray)));
 const unsigned char unknownList[]={10,0,0,9,0,0,127,1,0,0,0,0};assert(!NativeNbtValidation::compound(unknownList,sizeof(unknownList)));
 std::vector<unsigned char> nested;for(int i=0;i<70;++i)nested.insert(nested.end(),{10,0,0});for(int i=0;i<70;++i)nested.push_back(0);assert(!NativeNbtValidation::compound(nested.data(),nested.size()));
 std::ofstream broken(dir+"/level.dat",std::ios::binary|std::ios::trunc);int version=3,len=sizeof(unknownList);broken.write((char*)&version,4);broken.write((char*)&len,4);broken.write((const char*)unknownList,len);broken.close();
 LevelData recovered;assert(ExternalFileLevelStorage::readLevelData(dir,recovered));assert(recovered.getSeed()==1234&&recovered.weather.thundering);
 Player invalidPlayer(level,false);invalidPlayer.setPos(std::numeric_limits<float>::quiet_NaN(),80,40);
 std::vector<Player*> players(1,&invalidPlayer);weather.command("clear",20);
 assert(ExternalFileLevelStorage::writeLevelData(dir+"/level.dat",*level->getLevelData(),&players));
 LevelData validBackup;assert(ExternalFileLevelStorage::readLevelData(dir,validBackup));assert(validBackup.weather.thundering);
 std::cout<<"Native reopen recovery: malformed/truncated NBT, entity backup/quarantine, weather metadata fallback, invalid saved entity coordinates and failed writes PASS\n";
}
