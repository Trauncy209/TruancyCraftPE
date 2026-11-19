#ifndef TCPE_NATIVE_INVENTORY_TRANSACTION_H
#define TCPE_NATIVE_INVENTORY_TRANSACTION_H
#include <cstdint>
#include "../../world/item/ItemInstance.h"
#include "../Packet.h"
#include <vector>

// Optional trailers: legacy peers ignore these after their native hotbar data.
struct NativeInventoryAction {
    enum Type { Hotbar=1, Craft=2, Transfer=3, Armor=4, Drop=5, Use=6, Break=7, Death=8 };
    enum Status { Accepted=0, BadAction=1, InvalidContext=2, Insufficient=3, NoSpace=4, Unsupported=5, SequenceGap=6, WrongSession=7 };
    bool present=false;
    uint64_t nonce=0, sequence=0, baseRevision=0;
    int type=0, slot=-1, slot2=-1, count=1, flags=0, recipe=-1;
    int x=0,y=0,z=0,face=0,expectedId=0,expectedAux=0;
    float clickX=.5f,clickY=.5f,clickZ=.5f;
    void write(RakNet::BitStream* wire) const {
        if(!present)return;
        wire->Write(uint32_t(0x54434931));wire->Write(nonce);wire->Write(sequence);wire->Write(baseRevision);
        wire->Write(type);wire->Write(slot);wire->Write(slot2);wire->Write(count);wire->Write(flags);wire->Write(recipe);
        wire->Write(x);wire->Write(y);wire->Write(z);wire->Write(face);wire->Write(expectedId);wire->Write(expectedAux);
        wire->Write(clickX);wire->Write(clickY);wire->Write(clickZ);
    }
    bool read(RakNet::BitStream* wire) {
        present=false; if(!wire->GetNumberOfUnreadBits())return true;
        uint32_t magic=0;
        if(!wire->Read(magic)||magic!=0x54434931||!wire->Read(nonce)||!wire->Read(sequence)||!wire->Read(baseRevision)||
           !wire->Read(type)||!wire->Read(slot)||!wire->Read(slot2)||!wire->Read(count)||!wire->Read(flags)||!wire->Read(recipe)||
           !wire->Read(x)||!wire->Read(y)||!wire->Read(z)||!wire->Read(face)||!wire->Read(expectedId)||!wire->Read(expectedAux)||
           !wire->Read(clickX)||!wire->Read(clickY)||!wire->Read(clickZ)||wire->GetNumberOfUnreadBits())return false;
        present=true;return true;
    }
};
struct NativeInventorySnapshot {
    bool present=false;
    uint64_t nonce=0,revision=0,acknowledged=0;
    int status=0;
    ItemInstance armor[4];
    int containerId=-1;
    std::vector<ItemInstance> containerItems;
    void write(RakNet::BitStream* wire) const {
        if(!present)return;
        wire->Write(uint32_t(0x54435331));wire->Write(nonce);wire->Write(revision);wire->Write(acknowledged);wire->Write(status);
        for(int i=0;i<4;++i)PacketUtil::writeItemInstance(armor[i],wire);
        wire->Write(containerId); wire->Write((short)containerItems.size());
        for(const ItemInstance& item:containerItems)PacketUtil::writeItemInstance(item,wire);
    }
    bool read(RakNet::BitStream* wire) {
        present=false;if(!wire->GetNumberOfUnreadBits())return true;
        uint32_t magic=0;
        if(!wire->Read(magic)||magic!=0x54435331||!wire->Read(nonce)||!wire->Read(revision)||!wire->Read(acknowledged)||!wire->Read(status)||wire->GetNumberOfUnreadBits()<160)return false;
        for(int i=0;i<4;++i)armor[i]=PacketUtil::readItemInstance(wire);
        short size=0;
        if(!wire->Read(containerId)||!wire->Read(size)||size<0||size>129||wire->GetNumberOfUnreadBits()<(unsigned int)size*40)return false;
        containerItems.clear();
        for(int i=0;i<size;++i)containerItems.push_back(PacketUtil::readItemInstance(wire));
        if(wire->GetNumberOfUnreadBits())return false;
        present=true;return true;
    }
};
#endif
