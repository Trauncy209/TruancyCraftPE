#ifndef TCPE_NATIVE_INVENTORY_MODEL_H
#define TCPE_NATIVE_INVENTORY_MODEL_H
#include "packet/NativeInventoryTransaction.h"
#include "packet/NativeHotbarState.h"
#include "../world/entity/player/Inventory.h"
#include "../world/item/crafting/Recipes.h"
#include "../world/item/ArmorItem.h"
#include <algorithm>
// Pure proposal logic shared by authoritative commits and client prediction.
// No world mutation or packets; callers commit the copies only on Accepted.
struct NativeInventoryModel {
    static bool same(const ItemInstance& a,const ItemInstance& b) {return a.id==b.id&&a.getAuxValue()==b.getAuxValue();}
    static bool matchesExpected(const ItemInstance& item,const NativeInventoryAction& a) {
        return !a.expectedId||(item.id==a.expectedId&&(item.isDamageableItem()||item.getAuxValue()==a.expectedAux));
    }
    static bool empty(const ItemInstance& a) {return a.id<=0||a.count<=0;}
    static NativeHotbarState rebaseHotbar(const NativeHotbarState& desired,const std::vector<ItemInstance>& current,const std::vector<ItemInstance>& expected) {
        NativeHotbarState result=desired;
        for(int i=0;i<Inventory::MAX_SELECTION_SIZE;++i) {
            int source=desired.slots[i]-Inventory::MAX_SELECTION_SIZE,found=-1;
            if(source>=0&&source<(int)expected.size()&&!empty(expected[source])) {
                NativeInventoryAction identity;identity.expectedId=expected[source].id;identity.expectedAux=expected[source].getAuxValue();
                if(source<(int)current.size()&&!empty(current[source])&&matchesExpected(current[source],identity))found=source;
                else for(int j=0;j<(int)current.size();++j)if(!empty(current[j])&&matchesExpected(current[j],identity)){found=j;break;}
            }
            result.slots[i]=found<0?-1:found+Inventory::MAX_SELECTION_SIZE;
        }
        return result;
    }
    static bool add(std::vector<ItemInstance>& slots,ItemInstance item,int destination=-1) {
        if(empty(item))return true;
        if(!item.getItem())return false;
        if(destination>=0) {
            if(destination>=(int)slots.size())return false;
            ItemInstance& target=slots[destination];
            if(!empty(target)&&!same(target,item))return false;
            int existing=empty(target)?0:target.count;
            if(existing+item.count>item.getMaxStackSize())return false;
            item.count+=existing;target=item;return true;
        }
        for(ItemInstance& target:slots)if(!empty(target)&&same(target,item)) {
            int moved=std::min(item.count,std::max(0,target.getMaxStackSize()-target.count));
            target.count+=moved;item.count-=moved;if(item.count==0)return true;
        }
        for(ItemInstance& target:slots)if(empty(target)) {
            int moved=std::min(item.count,item.getMaxStackSize());target=item;target.count=moved;item.count-=moved;if(item.count==0)return true;
        }
        return false;
    }
    static int resolveSource(const std::vector<ItemInstance>& slots,int preferred,const NativeInventoryAction& a,int needed=1) {
        if(preferred>=0&&preferred<(int)slots.size()&&!empty(slots[preferred])&&slots[preferred].count>=needed&&matchesExpected(slots[preferred],a))return preferred;
        if(!a.expectedId)return -1;
        for(int i=0;i<(int)slots.size();++i)if(!empty(slots[i])&&slots[i].count>=needed&&matchesExpected(slots[i],a))return i;
        return -1;
    }
    static int apply(std::vector<ItemInstance>& items,ItemInstance armor[4],std::vector<ItemInstance>& container,const NativeInventoryAction& incoming) {
        NativeInventoryAction a=incoming;
        using A=NativeInventoryAction;
        if(a.count<=0||a.count>64||a.slot2< -1||a.slot2>=Inventory::CREATIVE_SLOT_COUNT+Inventory::MAX_SELECTION_SIZE)return A::BadAction;
        if(a.type==A::Transfer||a.type==A::Drop||(a.type==A::Armor&&!(a.flags&1))) {
            bool fromContainer=a.type==A::Transfer&&(a.flags&1);
            if(a.slot<(fromContainer?0:Inventory::MAX_SELECTION_SIZE)||
               a.slot>=(int)(fromContainer?container.size():items.size()+Inventory::MAX_SELECTION_SIZE))return A::BadAction;
            int preferred=fromContainer?a.slot:a.slot-Inventory::MAX_SELECTION_SIZE;
            int source=resolveSource(fromContainer?container:items,preferred,a,a.type==A::Armor?1:a.count);
            if(source<0)return A::Insufficient;
            a.slot=source+(fromContainer?0:Inventory::MAX_SELECTION_SIZE);
        }
        if(a.count<=0||a.count>64)return A::BadAction;
        if(a.type==A::Craft) {
            const auto& recipes=Recipes::getInstance()->getRecipes();
            if(a.recipe<0||a.recipe>=(int)recipes.size())return A::BadAction;
            Recipe* recipe=recipes[a.recipe];
            ItemInstance output=recipe->getResultItem();
            if(a.expectedId&&(output.id!=a.expectedId||output.getAuxValue()!=a.expectedAux))return A::InvalidContext;
            if(recipe->getCraftingSize()==Recipe::SIZE_3X3&&!(a.flags&1))return A::InvalidContext;
            for(ItemInstance req:recipe->getItemPack().getItemInstances()) {
                int needed=req.count*a.count;
                if(needed<=0||needed>4096)return A::BadAction;
                for(ItemInstance& item:items)if(item.id==req.id&&(req.getAuxValue()<0||item.getAuxValue()==req.getAuxValue())) {
                    int take=std::min(needed,std::max(0,item.count));item.count-=take;needed-=take;if(item.count==0)item.setNull();
                }
                if(needed)return A::Insufficient;
            }
            ItemInstance result=recipe->getResultItem();result.count*=a.count;
            return add(items,result)?A::Accepted:A::NoSpace;
        }
        if(a.type==A::Transfer) {
            std::vector<ItemInstance>& from=(a.flags&1)?container:items;
            std::vector<ItemInstance>& to=(a.flags&1)?items:container;
            int source=(a.flags&1)?a.slot:a.slot-Inventory::MAX_SELECTION_SIZE;
            int dest=a.slot2<0?-1:((a.flags&1)?a.slot2-Inventory::MAX_SELECTION_SIZE:a.slot2);
            if(source<0||source>=(int)from.size()||dest< -1)return A::BadAction;
            ItemInstance& item=from[source];
            if(empty(item)||item.count<a.count)return A::Insufficient;
            if(!matchesExpected(item,a))return A::InvalidContext;
            ItemInstance moved=item;moved.count=a.count;
            if(!add(to,moved,dest))return A::NoSpace;
            item.count-=a.count;if(item.count==0)item.setNull();return A::Accepted;
        }
        if(a.type==A::Armor) {
            if(a.flags&1) {
                if(a.slot<0||a.slot>=4||empty(armor[a.slot]))return A::BadAction;
                if(!matchesExpected(armor[a.slot],a))return A::InvalidContext;
                int dest=a.slot2<0?-1:a.slot2-Inventory::MAX_SELECTION_SIZE;
                if(dest< -1||!add(items,armor[a.slot],dest))return A::NoSpace;
                armor[a.slot].setNull();return A::Accepted;
            }
            int source=a.slot-Inventory::MAX_SELECTION_SIZE;
            if(source<0||source>=(int)items.size()||a.slot2<0||a.slot2>=4)return A::BadAction;
            ItemInstance& item=items[source];
            if(empty(item)||item.count!=1||!item.getItem()||!item.getItem()->isArmor()||static_cast<ArmorItem*>(item.getItem())->slot!=a.slot2)return A::BadAction;
            if(!matchesExpected(item,a))return A::InvalidContext;
            std::swap(item,armor[a.slot2]);return A::Accepted;
        }
        if(a.type==A::Drop) {
            int source=a.slot-Inventory::MAX_SELECTION_SIZE;
            if(source<0||source>=(int)items.size())return A::BadAction;
            ItemInstance& item=items[source];
            if(empty(item)||item.count<a.count)return A::Insufficient;
            if(!matchesExpected(item,a))return A::InvalidContext;
            item.count-=a.count;if(item.count==0)item.setNull();return A::Accepted;
        }
        return A::Unsupported;
    }
};
#endif
