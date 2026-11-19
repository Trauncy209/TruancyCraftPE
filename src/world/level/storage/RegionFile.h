#ifndef NET_MINECRAFT_WORLD_LEVEL_STORAGE__RegionFile_H__
#define NET_MINECRAFT_WORLD_LEVEL_STORAGE__RegionFile_H__

#include <map>
#include <set>
#include <string>
#include "../../../raknet/BitStream.h"

typedef std::map<int, bool> FreeSectorMap;

class RegionFile
{
public:
RegionFile(const std::string& basePath);
virtual ~RegionFile();

bool open();
bool readChunk(int x, int z, RakNet::BitStream** destChunkData);
bool writeChunk(int x, int z, RakNet::BitStream& chunkData);
void markChunkCorrupt(int x, int z);
private:
struct RegionState {
RegionState() : file(NULL), offsets(NULL) {}

FILE* file;
std::string filename;
unsigned int* offsets;
int totalSectors = 1;
bool headerComplete = true, damaged = false, preserved = false;
std::set<int> corruptChunks;
FreeSectorMap sectorFree;
};

RegionState* getRegion(int x, int z, bool createIfMissing);
bool openRegion(RegionState* region, bool createIfMissing);
bool write(RegionState* region, int sector, RakNet::BitStream& chunkData);
bool preserveOriginal(RegionState* region);
void close();

std::string basePath;
std::map<long long, RegionState*> regions;
int* emptyChunk;
};

#endif /*NET_MINECRAFT_WORLD_LEVEL_STORAGE__RegionFile_H__*/
