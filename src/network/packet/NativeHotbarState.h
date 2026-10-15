#ifndef TCPE_NATIVE_HOTBAR_STATE_H
#define TCPE_NATIVE_HOTBAR_STATE_H
#include "../../world/entity/player/Inventory.h"
// Optional trailing data preserves compatibility with earlier native packets.
struct NativeHotbarState {
    bool present = false;
    short slots[Inventory::MAX_SELECTION_SIZE];
    unsigned char selected = 0;
    void capture(Inventory* inventory) {
        present = true; selected = inventory->selected;
        for (int i=0;i<Inventory::MAX_SELECTION_SIZE;++i) slots[i]=inventory->linkedSlots[i].inventorySlot;
    }
    void write(RakNet::BitStream* wire) const {
        if (!present) return;
        wire->Write((unsigned int)0x54434831);
        wire->Write(selected);
        for (int i=0;i<Inventory::MAX_SELECTION_SIZE;++i) wire->Write(slots[i]);
    }
    void read(RakNet::BitStream* wire) {
        present=false;
        if (wire->GetNumberOfUnreadBits() < (4+1+2*Inventory::MAX_SELECTION_SIZE)*8) return;
        unsigned int magic=0; if (!wire->Read(magic) || magic!=0x54434831 || !wire->Read(selected)) return;
        if (selected>=Inventory::MAX_SELECTION_SIZE) return;
        for (int i=0;i<Inventory::MAX_SELECTION_SIZE;++i) {
            if (!wire->Read(slots[i]) || (slots[i]!=-1 && (slots[i]<Inventory::MAX_SELECTION_SIZE || slots[i]>=45))) return;
        }
        present=true;
    }
    void apply(Inventory* inventory) const {
        if (!present) return;
        for (int i=0;i<Inventory::MAX_SELECTION_SIZE;++i)
            inventory->linkedSlots[i].inventorySlot=slots[i]<inventory->getContainerSize()?slots[i]:-1;
        inventory->selectSlot(selected);
    }
};
#endif
