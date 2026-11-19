#ifndef TCPE_CHUNK_COLLECTION_GATE_H
#define TCPE_CHUNK_COLLECTION_GATE_H
namespace NativeChunkLifetime {
inline bool mayCollectAtFrameEnd(bool generating,bool workerPresent,bool generationSignaled) {
    return !generating&&!workerPresent&&generationSignaled;
}
}
#endif
