#include "world/level/storage/RegionFile.h"
#include "world/level/storage/ExternalFileLevelStorage.h"
#include "world/level/Level.h"
#include "world/level/chunk/LevelChunk.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include "world/level/biome/Biome.h"
#include "util/Mth.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>
#include <cstdint>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <csignal>

static std::string scratch() {char path[]="/tmp/tcpe-region-recovery-XXXXXX";assert(mkdtemp(path));return path;}
static std::vector<unsigned char> readBytes(const std::string& path) {std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};}
static void writeBytes(const std::string& path,const std::vector<unsigned char>& bytes) {std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write((const char*)bytes.data(),bytes.size());assert(out.good());}
static void put32(std::vector<unsigned char>& bytes,size_t offset,uint32_t value) {assert(offset+4<=bytes.size());memcpy(bytes.data()+offset,&value,4);}
static uint32_t get32(const std::vector<unsigned char>& bytes,size_t offset) {uint32_t value;assert(offset+4<=bytes.size());memcpy(&value,bytes.data()+offset,4);return value;}
static void release(RakNet::BitStream* bytes) {if(bytes){delete[] bytes->GetData();delete bytes;}}
static int backups(const std::string& dir) {int count=0;DIR* d=opendir((dir+"/region").c_str());assert(d);while(auto* e=readdir(d))if(std::string(e->d_name).find(".corrupt-")!=std::string::npos)++count;closedir(d);return count;}
static std::string firstBackup(const std::string& dir) {
    DIR* entries=opendir((dir+"/region").c_str());assert(entries);std::string result;
    while(auto* entry=readdir(entries))if(std::string(entry->d_name).find(".corrupt-")!=std::string::npos) {result=dir+"/region/"+entry->d_name;break;}
    closedir(entries);assert(!result.empty());return result;
}
int main(int argc,char** argv) {
    const std::string dir=scratch();assert(mkdir((dir+"/region").c_str(),0700)==0);
    const std::string file=dir+"/region/r.0.0.mcr";
    // A plausible sector header whose payload was truncated by a failed save.
    std::vector<unsigned char> truncated(4100,0);put32(truncated,0,0x101);put32(truncated,4096,16);writeBytes(file,truncated);
    {RegionFile region(dir);assert(region.open());RakNet::BitStream* output=nullptr;assert(!region.readChunk(0,0,&output)&&output==nullptr);}
    assert(readBytes(file)==truncated); // Read-only recovery must not rewrite evidence.
    if(argc>1&&std::string(argv[1])=="baseline")return 0;
    // More corrupt bounds must fail without allocating the claimed length.
    for(uint32_t length:{uint32_t(0),uint32_t(3),uint32_t(4097),uint32_t(0x7fffffff),uint32_t(0xffffffff)}) {
        auto corrupt=truncated;put32(corrupt,4096,length);writeBytes(file,corrupt);
        RegionFile region(dir);assert(region.open());RakNet::BitStream* output=nullptr;assert(!region.readChunk(0,0,&output)&&!output);
        assert(readBytes(file)==corrupt);
    }
    writeBytes(file,truncated);
    {RegionFile region(dir);assert(region.open());RakNet::BitStream* output=nullptr;assert(!region.readChunk(0,0,&output));
        RakNet::BitStream valid;const unsigned char payload[]={1,2,3,4,5};valid.Write((const char*)payload,sizeof(payload));
        assert(region.writeChunk(0,0,valid));assert(backups(dir)==1);assert(region.readChunk(0,0,&output));
        assert(output->GetNumberOfBytesUsed()==sizeof(payload)&&memcmp(output->GetData(),payload,sizeof(payload))==0);release(output);
    }
    assert(readBytes(firstBackup(dir))==truncated); // Recovery retains the exact original bytes.
    const std::string partial=scratch();assert(mkdir((partial+"/region").c_str(),0700)==0);
    std::vector<unsigned char> shortHeader(128,0xa7);writeBytes(partial+"/region/r.0.0.mcr",shortHeader);
    {RegionFile broken(partial);assert(broken.open());RakNet::BitStream* out=nullptr;assert(!broken.readChunk(0,0,&out));
        RakNet::BitStream restored;restored.Write((unsigned int)123);assert(broken.writeChunk(0,0,restored));}
    assert(readBytes(firstBackup(partial))==shortHeader);
    // A real operating-system write failure must not report success or commit
    // a header pointing at an incomplete chunk. Limit only this test process.
    const std::string full=scratch();{RegionFile disk(full);assert(disk.open());
        RakNet::BitStream emptyHeaderProbe;RakNet::BitStream* out=nullptr;assert(!disk.readChunk(0,0,&out));
        struct rlimit previous;assert(getrlimit(RLIMIT_FSIZE,&previous)==0);
        auto oldSignal=signal(SIGXFSZ,SIG_IGN);struct rlimit limited=previous;limited.rlim_cur=4096;
        assert(setrlimit(RLIMIT_FSIZE,&limited)==0);RakNet::BitStream payload;payload.Write((unsigned int)99);
        const bool written=disk.writeChunk(0,0,payload);
        assert(setrlimit(RLIMIT_FSIZE,&previous)==0);signal(SIGXFSZ,oldSignal);
        assert(!written);assert(!disk.readChunk(0,0,&out));assert(get32(readBytes(full+"/region/r.0.0.mcr"),0)==0);
    }
    const std::string clean=scratch();RegionFile region(clean);assert(region.open());
    RakNet::BitStream one;one.Write((unsigned int)0x12345678);assert(region.writeChunk(-1,-1,one));
    const std::string negative=clean+"/region/r.-1.-1.mcr";const auto before=readBytes(negative);
    const uint32_t firstOffset=get32(before,(31+31*32)*4);
    RakNet::BitStream two;two.Write((unsigned int)0x23456789);assert(region.writeChunk(-1,-1,two));
    const auto after=readBytes(negative);const uint32_t nextOffset=get32(after,(31+31*32)*4);
    assert(firstOffset!=nextOffset); // Data-first copy-on-write keeps previous record intact.
    const size_t firstSector=(firstOffset>>8)*4096;
    assert(memcmp(before.data()+firstSector,after.data()+firstSector,8)==0);
    RakNet::BitStream* output=nullptr;assert(region.readChunk(-1,-1,&output));unsigned int number=0;assert(output->Read(number)&&number==0x23456789);release(output);
    assert(region.writeChunk(-32,-33,one));assert(region.writeChunk(INT32_MIN,INT32_MAX,one));
    std::vector<unsigned char> maximum(255*4096-4,0x4f);RakNet::BitStream max(maximum.data(),maximum.size(),false);
    assert(region.writeChunk(0,0,max));
    const auto maxFile=readBytes(clean+"/region/r.0.0.mcr");assert((get32(maxFile,0)&255)==255);
    maximum.push_back(0);RakNet::BitStream excessive(maximum.data(),maximum.size(),false);
    assert(!region.writeChunk(0,0,excessive));assert(readBytes(clean+"/region/r.0.0.mcr")==maxFile);
    // Generic RegionFile data may be valid while the actual chunk arrays are short.
    Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
    const std::string shortChunk=scratch();{RegionFile raw(shortChunk);assert(raw.open());assert(raw.writeChunk(0,0,one));}
    auto* store=new ExternalFileLevelStorage("short-chunk",shortChunk);
    auto* level=new Level(store,"short-chunk",LevelSettings(1234,GameType::Survival,LGV_ORIGINAL),LGV_ORIGINAL);level->setUpdateLights(false);
    const auto original=readBytes(shortChunk+"/region/r.0.0.mcr");
    assert(store->load(level,0,0)==nullptr);assert(readBytes(shortChunk+"/region/r.0.0.mcr")==original);
    auto* blocks=new unsigned char[16*16*128]();LevelChunk replacement(level,blocks,0,0);store->save(level,&replacement);
    assert(backups(shortChunk)==1);assert(readBytes(firstBackup(shortChunk))==original);
    auto* restored=store->load(level,0,0);assert(restored&&restored->getTile(0,0,0)==0);
    for(int pass=0;pass<12;++pass) {
        blocks[70]=pass%2?Tile::wood->id:Tile::dirt->id;replacement.data.set(0,70,0,pass&3);
        store->save(level,&replacement);
        auto* reader=new ExternalFileLevelStorage("short-chunk",shortChunk);
        auto* loaded=reader->load(level,0,0);assert(loaded&&loaded->getTile(0,70,0)==blocks[70]&&loaded->getData(0,70,0)==(pass&3));
        delete reader;
    }
    assert(readBytes(shortChunk+"/region/r.0.0.mcr").size()<3*82176); // Reuse retired sectors instead of growing every save.
    std::cout<<"Actual region/chunk recovery: corrupt length/short reads bounded, originals preserved, copy-on-write records, negative coordinates, 255-sector boundary and short-array rejection PASS\n";
}
