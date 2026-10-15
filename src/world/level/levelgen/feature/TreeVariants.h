#ifndef TCPE_TREE_VARIANTS_H
#define TCPE_TREE_VARIANTS_H
#include "BasicTree.h"
#include "TreeFeature.h"

// Keep small oaks common while giving large, branched silhouettes their own
// minimum heights. Population's init(1,1,1) preserves these dimensions.
inline Feature* createOakVariant(Random* random, bool update, bool denseForest = false) {
    const int variant = random->nextInt(denseForest ? 10 : 16);
    if (variant == 0) return new BasicTree(update, 11, 7, 1.1f, 1.0f);
    if (variant == 1) return new BasicTree(update, 9, 6, 1.5f, 1.15f);
    return new TreeFeature(update);
}
#endif
