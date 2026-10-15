#ifndef TCPE_COMPRESSED_CHUNK_DATA_PACKET_H
#define TCPE_COMPRESSED_CHUNK_DATA_PACKET_H
#include "ChunkDataPacket.h"
#include <vector>
#include <zlib.h>
// Opt-in native transport. Inflation reconstructs the unchanged authoritative
// snapshot, including air and metadata; no local terrain regeneration is used.
class CompressedChunkDataPacket : public ChunkDataPacket {
public:
    bool valid = false;
    CompressedChunkDataPacket() = default;
    CompressedChunkDataPacket(int x, int z, LevelChunk* chunk)
        : ChunkDataPacket(x, z, chunk, true) {}
    void write(RakNet::BitStream* output) override {
        RakNet::BitStream legacy;
        ChunkDataPacket::write(&legacy);
        const unsigned int size = chunkData.GetNumberOfBytesUsed();
        std::vector<unsigned char> packed(compressBound(size));
        uLongf length = packed.size();
        if (compress2(packed.data(), &length, chunkData.GetData(), size, Z_BEST_SPEED) != Z_OK) { output->Write(legacy); return; }
        output->Write((RakNet::MessageID)(ID_USER_PACKET_ENUM + PACKET_COMPRESSED_CHUNKDATA));
        output->Write(x); output->Write(z); output->Write(size);
        output->Write((unsigned int)length);
        output->Write((const char*)packed.data(), (unsigned int)length);
    }
    void read(RakNet::BitStream* input) override {
        valid = false; fullSnapshot = false; chunkData.Reset();
        unsigned int rawSize = 0, packedSize = 0;
        const unsigned int snapshotSize = CHUNK_COLUMNS * (1 + LEVEL_HEIGHT * 3 / 2);
        if (!input->Read(x) || !input->Read(z) || !input->Read(rawSize) ||
            !input->Read(packedSize) || rawSize != snapshotSize ||
            packedSize == 0 || packedSize > compressBound(snapshotSize) ||
            input->GetNumberOfUnreadBits() != packedSize * 8) return;
        std::vector<unsigned char> packed(packedSize), raw(rawSize);
        if (!input->Read((char*)packed.data(), packedSize)) return;
        uLongf length = rawSize;
        if (uncompress(raw.data(), &length, packed.data(), packedSize) != Z_OK || length != rawSize) return;
        // Full snapshots must contain all eight sections of every column.
        for (unsigned int col = 0; col < CHUNK_COLUMNS; ++col)
            if (raw[col * (1 + LEVEL_HEIGHT * 3 / 2)] != 0xff) return;
        chunkData.Write((const char*)raw.data(), rawSize); fullSnapshot = true; valid = true;
    }
    void handle(const RakNet::RakNetGUID& source, NetEventCallback* callback) override {
        if (valid) callback->handle(source, static_cast<ChunkDataPacket*>(this));
    }
};
#endif
