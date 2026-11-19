#ifndef TCPE_SAVED_ENTITY_TRANSFORM_H
#define TCPE_SAVED_ENTITY_TRANSFORM_H
#include "../../nbt/CompoundTag.h"
#include <cmath>
namespace SavedEntityTransform {
inline bool valid(const CompoundTag* tag,float maxCoordinate) {
    if (!tag || !tag->contains("Pos", Tag::TAG_List) || !tag->contains("Motion", Tag::TAG_List) ||
        !tag->contains("Rotation", Tag::TAG_List)) return false;
    const char* names[] = { "Pos", "Motion", "Rotation" };
    for (int group=0;group<3;++group) {
        const ListTag* values=tag->getList(names[group]);
        const int count=group==2?2:3;
        if(values->size()!=count)return false;
        for(int i=0;i<count;++i) {
            Tag* value=values->get(i);
            if(!value||value->getId()!=Tag::TAG_Float)return false;
            const float number=static_cast<FloatTag*>(value)->data;
            if(!std::isfinite(number)||(group==0&&std::fabs(number)>maxCoordinate))return false;
        }
    }
    return true;
}
}
#endif
