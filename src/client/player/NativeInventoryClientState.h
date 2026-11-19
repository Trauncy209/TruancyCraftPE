#ifndef TCPE_NATIVE_INVENTORY_CLIENT_STATE_H
#define TCPE_NATIVE_INVENTORY_CLIENT_STATE_H
#include "../../network/Packet.h"
#include "../../network/packet/NativeHotbarState.h"
#include "../../network/NativeInventoryModel.h"
#include <deque>
#include <limits>

// Only semantic requests cross the network. Each authoritative snapshot is a
// fresh base; unacknowledged commands are replayed without replacing new loot.
class NativeInventoryClientState {
public:
    struct Pending { NativeInventoryAction action; NativeHotbarState hotbar; std::vector<ItemInstance> expectedItems; };
    uint64_t nonce=0, revision=0, acknowledged=0, nextSequence=1, viewRevision=0;
    int lastStatus=0;
    NativeInventorySnapshot base;
    NativeHotbarState baseHotbar, predictedHotbar;
    std::vector<ItemInstance> baseItems, items, container;
    ItemInstance armor[4];
    std::deque<Pending> pending;

    NativeInventoryClientState() { reset(); }
    void reset() {
        nonce=revision=acknowledged=viewRevision=0; nextSequence=1; lastStatus=0;
        base=NativeInventorySnapshot(); baseHotbar=NativeHotbarState();
        baseHotbar.present=true;
        for(int i=0;i<Inventory::MAX_SELECTION_SIZE;++i)baseHotbar.slots[i]=-1;
        predictedHotbar=baseHotbar; baseItems.clear();items.clear();container.clear();pending.clear();
        for(auto& item:armor)item.setNull();
    }
    bool enabled() const { return nonce!=0; }
    static bool sameHotbar(const NativeHotbarState& a,const NativeHotbarState& b) {
        if(a.selected!=b.selected)return false;
        for(int i=0;i<Inventory::MAX_SELECTION_SIZE;++i)if(a.slots[i]!=b.slots[i])return false;
        return true;
    }
    bool accept(const std::vector<ItemInstance>& newItems,const NativeHotbarState& hotbar,const NativeInventorySnapshot& state) {
        if(!state.present||!state.nonce||!hotbar.present)return false;
        if(enabled()) {
            if(state.nonce!=nonce||state.revision<revision||state.acknowledged<acknowledged||state.acknowledged>=nextSequence)return false;
        } else {
            if(state.acknowledged!=0)return false;
            nonce=state.nonce;
        }
        revision=state.revision;acknowledged=state.acknowledged;lastStatus=state.status;
        base=state;baseItems=newItems;baseHotbar=hotbar;
        while(!pending.empty()&&pending.front().action.sequence<=acknowledged)pending.pop_front();
        rebuild();return true;
    }
    bool enqueue(NativeInventoryAction& action,const NativeHotbarState& hotbar) {
        if(!enabled()||pending.size()>=128||nextSequence==std::numeric_limits<uint64_t>::max())return false;
        action.present=true;action.nonce=nonce;action.sequence=nextSequence++;action.baseRevision=revision;
        pending.push_back(Pending{action,hotbar,items});rebuild();return true;
    }
    void rebuild() {
        items=baseItems;container=base.containerItems;predictedHotbar=baseHotbar;
        for(int i=0;i<4;++i)armor[i]=base.armor[i];
        for(const Pending& command:pending) {
            const auto& action=command.action;
            if(action.type==NativeInventoryAction::Hotbar) { predictedHotbar=NativeInventoryModel::rebaseHotbar(command.hotbar,items,command.expectedItems);continue; }
            if(action.type==NativeInventoryAction::Transfer && action.x!=base.containerId)continue;
            auto proposedItems=items, proposedContainer=container;
            ItemInstance proposedArmor[4];for(int i=0;i<4;++i)proposedArmor[i]=armor[i];
            if(NativeInventoryModel::apply(proposedItems,proposedArmor,proposedContainer,action)!=NativeInventoryAction::Accepted)continue;
            items.swap(proposedItems);container.swap(proposedContainer);
            for(int i=0;i<4;++i)armor[i]=proposedArmor[i];
            autoLink();
        }
        normalizeHotbar();++viewRevision;
    }
private:
    void normalizeHotbar() {
        predictedHotbar.present=true;
        if(predictedHotbar.selected>=Inventory::MAX_SELECTION_SIZE)predictedHotbar.selected=0;
        for(auto& slot:predictedHotbar.slots)
            if(slot<Inventory::MAX_SELECTION_SIZE||slot>=int(items.size())+Inventory::MAX_SELECTION_SIZE)slot=-1;
    }
    void autoLink() {
        normalizeHotbar();
        for(int slot=0;slot<int(items.size());++slot) {
            if(NativeInventoryModel::empty(items[slot]))continue;
            const int backing=slot+Inventory::MAX_SELECTION_SIZE;bool linked=false;
            for(auto link:predictedHotbar.slots)if(link==backing)linked=true;
            if(linked)continue;
            for(auto& link:predictedHotbar.slots) {
                const int current=link-Inventory::MAX_SELECTION_SIZE;
                if(current<0||current>=int(items.size())||NativeInventoryModel::empty(items[current])) { link=backing;break; }
            }
        }
    }
};
#endif
