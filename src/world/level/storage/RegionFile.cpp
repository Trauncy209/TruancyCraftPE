#include "RegionFile.h"
#include "FolderMethods.h"
#include "../../../platform/log.h"
#include "../../../raknet/GetTime.h"
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#endif

namespace {
const int SectorBytes = 4096;
const int SectorInts = SectorBytes / 4;
const int SectorCols = 32;
const int MaxSectorCount = 255; // The low eight bits of each location entry.
const int MaxSectorNumber = 0xffffff;
int floorDiv32(int value) { return value / SectorCols - (value % SectorCols < 0); }
int mod32(int value) { const int remainder = value % SectorCols; return remainder < 0 ? remainder + SectorCols : remainder; }
long long regionKey(int x, int z) {
    return static_cast<long long>((uint64_t(uint32_t(x)) << 32) | uint32_t(z));
}
bool syncFile(FILE* file) {
    if (fflush(file)) return false;
#ifndef _WIN32
    if (fsync(fileno(file))) return false;
#endif
    return true;
}
void syncDirectory(const std::string& filename) {
#ifndef _WIN32
    const std::string directory = filename.substr(0, filename.find_last_of('/'));
    const int fd = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd >= 0) { fsync(fd); ::close(fd); }
#endif
}
bool seekSector(FILE* file, int sector) {
    // Region locations are 24-bit; reject rather than overflow a 32-bit long.
    const uint64_t offset = uint64_t(sector) * SectorBytes;
#ifdef _WIN32
    return _fseeki64(file, offset, SEEK_SET) == 0;
#else
    return fseeko(file, static_cast<off_t>(offset), SEEK_SET) == 0;
#endif
}
long long fileLength(FILE* file) {
#ifdef _WIN32
    if (_fseeki64(file, 0, SEEK_END)) return -1;
    return _ftelli64(file);
#else
    if (fseeko(file, 0, SEEK_END)) return -1;
    return ftello(file);
#endif
}
}

RegionFile::RegionFile(const std::string& basePath) : basePath(basePath) {
    emptyChunk = new int[SectorInts]();
}
RegionFile::~RegionFile() { close(); delete[] emptyChunk; }
bool RegionFile::open() {
    return createFolderIfNotExists(basePath.c_str()) && createFolderIfNotExists((basePath + "/region").c_str());
}
void RegionFile::close() {
    for (auto& entry : regions) {
        RegionState* region = entry.second;
        if (region->file) fclose(region->file);
        delete[] region->offsets;
        delete region;
    }
    regions.clear();
}

bool RegionFile::openRegion(RegionState* region, bool createIfMissing) {
    if (!region) return false;
    if (region->file) return true;
    delete[] region->offsets;
    region->offsets = new unsigned int[SectorInts]();
    region->sectorFree.clear();
    region->sectorFree[0] = false;
    region->file = fopen(region->filename.c_str(), "r+b");
    if (!region->file) {
        // Never replace an existing but inaccessible region after fopen failed.
        if (!createIfMissing || errno != ENOENT) return false;
        region->file = fopen(region->filename.c_str(), "w+bx");
        if (!region->file) return false;
        if (fwrite(region->offsets, sizeof(unsigned int), SectorInts, region->file) != SectorInts || !syncFile(region->file)) {
            fclose(region->file); region->file = NULL; return false;
        }
        region->totalSectors = 1;
        return true;
    }
    const long long length = fileLength(region->file);
    if (length < 0 || uint64_t(length) > uint64_t(MaxSectorNumber + MaxSectorCount) * SectorBytes) {
        fclose(region->file); region->file = NULL; return false;
    }
    region->totalSectors = int((length + SectorBytes - 1) / SectorBytes);
    region->headerComplete = length >= SectorBytes;
    if (!region->headerComplete) {
        // Keep a truncated original untouched until a recovery copy is durable.
        region->damaged = true;
        return true;
    }
    if (fseek(region->file, 0, SEEK_SET) || fread(region->offsets, sizeof(unsigned int), SectorInts, region->file) != SectorInts) {
        fclose(region->file); region->file = NULL; return false;
    }
    // Track occupied sectors only. A large sparse/corrupt file must not cause
    // one map allocation for every byte range implied by its physical length.
    for (int index = 0; index < SectorInts; ++index) {
        const unsigned int offset = region->offsets[index];
        if (!offset) continue;
        const int first = int(offset >> 8), count = int(offset & 255);
        bool valid = first > 0 && count > 0 && first < region->totalSectors &&
                     int64_t(first) + count <= region->totalSectors;
        for (int i = 0; valid && i < count; ++i)
            if (region->sectorFree.count(first + i)) valid = false;
        if (!valid) {
            region->offsets[index] = 0;
            region->damaged = true;
            region->corruptChunks.insert(index);
            continue;
        }
        for (int i = 0; i < count; ++i) region->sectorFree[first + i] = false;
    }
    return true;
}

RegionFile::RegionState* RegionFile::getRegion(int x, int z, bool createIfMissing) {
    const int rx = floorDiv32(x), rz = floorDiv32(z);
    const long long key = regionKey(rx, rz);
    auto found = regions.find(key);
    if (found != regions.end()) {
        if (!found->second->file && !openRegion(found->second, createIfMissing)) return NULL;
        return found->second;
    }
    RegionState* region = new RegionState();
    region->filename = basePath + "/region/r." + std::to_string(rx) + "." + std::to_string(rz) + ".mcr";
    regions[key] = region;
    if (!openRegion(region, createIfMissing)) {
        delete[] region->offsets; delete region; regions.erase(key); return NULL;
    }
    return region;
}

bool RegionFile::preserveOriginal(RegionState* region) {
    if (!region->damaged || region->preserved) return true;
    FILE* input = fopen(region->filename.c_str(), "rb");
    if (!input) return false;
    std::string path = region->filename + ".corrupt-" + std::to_string(RakNet::GetTimeMS());
    FILE* output = NULL;
    for (;;) {
        output = fopen(path.c_str(), "wbx");
        if (output || errno != EEXIST) break;
        path += "-1";
    }
    if (!output) { fclose(input); return false; }
    unsigned char bytes[64 * 1024];
    bool ok = true;
    for (;;) {
        const size_t size = fread(bytes, 1, sizeof(bytes), input);
        if (size && fwrite(bytes, 1, size, output) != size) { ok = false; break; }
        if (size < sizeof(bytes)) { if (ferror(input)) ok = false; break; }
    }
    if (fclose(input)) ok = false;
    if (ok && !syncFile(output)) ok = false;
    if (fclose(output)) ok = false;
    if (!ok) { remove(path.c_str()); return false; }
    syncDirectory(path);
    region->preserved = true;
    LOGE("Preserved corrupt region before recovery: %s\n", path.c_str());
    return true;
}

void RegionFile::markChunkCorrupt(int x, int z) {
    if (RegionState* region = getRegion(x, z, false)) {
        region->damaged = true;
        region->corruptChunks.insert(mod32(x) + mod32(z) * SectorCols);
    }
}

bool RegionFile::readChunk(int x, int z, RakNet::BitStream** destChunkData) {
    if (!destChunkData) return false;
    *destChunkData = NULL;
    RegionState* region = getRegion(x, z, false);
    if (!region || !region->file) return false;
    const int index = mod32(x) + mod32(z) * SectorCols;
    const unsigned int offset = region->offsets[index];
    if (!offset || region->corruptChunks.count(index)) return false;
    const int sector = int(offset >> 8), count = int(offset & 255);
    int length = 0;
    bool valid = sector > 0 && count > 0 && seekSector(region->file, sector) &&
        fread(&length, sizeof(length), 1, region->file) == 1 &&
        length > int(sizeof(length)) && length <= count * SectorBytes;
    if (!valid) { markChunkCorrupt(x, z); return false; }
    const int payload = length - sizeof(length);
    std::unique_ptr<unsigned char[]> data(new unsigned char[payload]);
    if (fread(data.get(), 1, payload, region->file) != size_t(payload)) {
        markChunkCorrupt(x, z); return false;
    }
    *destChunkData = new RakNet::BitStream(data.release(), payload, false);
    return true;
}

bool RegionFile::writeChunk(int x, int z, RakNet::BitStream& chunkData) {
    const unsigned int payload = chunkData.GetNumberOfBytesUsed();
    if (!payload || payload > unsigned(MaxSectorCount * SectorBytes - sizeof(int))) return false;
    RegionState* region = getRegion(x, z, true);
    if (!region || !region->file || !preserveOriginal(region)) return false;
    if (!region->headerComplete) {
        if (fseek(region->file, 0, SEEK_SET) || fwrite(region->offsets, sizeof(unsigned int), SectorInts, region->file) != SectorInts || !syncFile(region->file)) return false;
        region->headerComplete = true; region->totalSectors = 1;
    }
    const int index = mod32(x) + mod32(z) * SectorCols;
    const unsigned int previous = region->offsets[index];
    const int count = int((uint64_t(payload) + sizeof(int) + SectorBytes - 1) / SectorBytes);
    // Find a free run without releasing the current record. Its sectors remain
    // readable if a write is interrupted before the new header is committed.
    int slot = 1, run = 0;
    while (run < count) {
        if (slot > MaxSectorNumber || int64_t(slot) + count > MaxSectorNumber + MaxSectorCount) return false;
        auto used = region->sectorFree.find(slot + run);
        if (used == region->sectorFree.end() || used->second) ++run;
        else { slot += run + 1; run = 0; }
    }
    const int end = slot + count;
    if (end > region->totalSectors) {
        if (!seekSector(region->file, region->totalSectors)) return false;
        for (int i = region->totalSectors; i < end; ++i)
            if (fwrite(emptyChunk, sizeof(int), SectorInts, region->file) != SectorInts) return false;
        region->totalSectors = end;
    }
    if (!write(region, slot, chunkData) || !syncFile(region->file)) return false;
    const unsigned int next = (unsigned(slot) << 8) | unsigned(count);
    if (fseek(region->file, index * sizeof(unsigned int), SEEK_SET) || fwrite(&next, sizeof(next), 1, region->file) != 1 || !syncFile(region->file)) return false;
    region->offsets[index] = next;
    for (int i = 0; i < count; ++i) region->sectorFree[slot + i] = false;
    const int oldFirst = int(previous >> 8), oldCount = int(previous & 255);
    for (int i = 0; oldFirst > 0 && i < oldCount; ++i) region->sectorFree[oldFirst + i] = true;
    region->corruptChunks.erase(index);
    return true;
}

bool RegionFile::write(RegionState* region, int sector, RakNet::BitStream& chunkData) {
    if (!seekSector(region->file, sector)) return false;
    const int size = int(chunkData.GetNumberOfBytesUsed()) + sizeof(int);
    return fwrite(&size, sizeof(size), 1, region->file) == 1 &&
        fwrite(chunkData.GetData(), 1, chunkData.GetNumberOfBytesUsed(), region->file) == chunkData.GetNumberOfBytesUsed();
}
