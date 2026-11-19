#ifndef TCPE_NATIVE_INVENTORY_TRANSACTIONS_H
#define TCPE_NATIVE_INVENTORY_TRANSACTIONS_H
#include "NativeInventoryModel.h"
#include "packet/NativeHotbarState.h"
#include "../client/Minecraft.h"
#include "../client/gamemode/GameMode.h"
#include "../server/ServerPlayer.h"
#include "../world/level/Level.h"
#include "../world/inventory/ContainerMenu.h"
#include "../world/inventory/FurnaceMenu.h"
#include "../world/level/tile/entity/FurnaceTileEntity.h"
#include "../world/level/tile/entity/ChestTileEntity.h"
#include <cmath>
struct NativeInventoryTransactions {
    static bool near(ServerPlayer* p,int x,int y,int z) {
        return p->level->inRange(x,y,z)&&p->distanceToSqr(x+.5f,y+.5f,z+.5f)<=64.f;
    }
    static bool menuValid(ServerPlayer* p) {
        if(auto* menu=dynamic_cast<ContainerMenu*>(p->containerMenu))return menu->container&&menu->container->stillValid(p);
        if(auto* menu=dynamic_cast<FurnaceMenu*>(p->containerMenu))return menu->furnace&&menu->furnace->stillValid(p);
        return false;
    }
    static int apply(Minecraft* game,ServerPlayer* p,const NativeInventoryAction& a,const NativeHotbarState& hotbar,const std::vector<ItemInstance>* expectedItems=NULL) {
        using A=NativeInventoryAction;
        if(a.type==A::Death) {
            if(p->isAlive())return A::InvalidContext;
            p->finishNativeDeath();return A::Accepted;
        }
        if(!p->isAlive())return A::InvalidContext;
        if(a.type==A::Hotbar) {
            if(!hotbar.present||hotbar.selected>=Inventory::MAX_SELECTION_SIZE)return A::BadAction;
            for(int i=0;i<Inventory::MAX_SELECTION_SIZE;++i)if(hotbar.slots[i]!=-1&&(hotbar.slots[i]<Inventory::MAX_SELECTION_SIZE||hotbar.slots[i]>=p->inventory->getContainerSize()))return A::BadAction;
            if(expectedItems) {
                std::vector<ItemInstance> current;
                for(int i=Inventory::MAX_SELECTION_SIZE;i<p->inventory->getContainerSize();++i)current.push_back(ItemInstance::cloneSafe(p->inventory->getItem(i)));
                NativeInventoryModel::rebaseHotbar(hotbar,current,*expectedItems).apply(p->inventory);
            } else hotbar.apply(p->inventory);
            return A::Accepted;
        }
        if(a.type==A::Use||a.type==A::Break) {
            if(a.slot!=-1&&(a.slot<Inventory::MAX_SELECTION_SIZE||a.slot>=p->inventory->getContainerSize()))return A::BadAction;
            if(a.slot==-1&&p->inventory->getSelected())return A::InvalidContext;
            int sourceSlot=a.slot;
            if(a.expectedId) {
                std::vector<ItemInstance> current;
                for(int i=Inventory::MAX_SELECTION_SIZE;i<p->inventory->getContainerSize();++i)current.push_back(ItemInstance::cloneSafe(p->inventory->getItem(i)));
                int resolved=NativeInventoryModel::resolveSource(current,a.slot-Inventory::MAX_SELECTION_SIZE,a);
                if(resolved<0)return A::Insufficient;
                sourceSlot=resolved+Inventory::MAX_SELECTION_SIZE;
            }
            ItemInstance* item=sourceSlot==-1?NULL:p->inventory->getItem(sourceSlot);
            if(item&&item->count<=0)item=NULL;
            if(a.expectedId&&(!item||!NativeInventoryModel::matchesExpected(*item,a)))return A::InvalidContext;
            if(sourceSlot>=Inventory::MAX_SELECTION_SIZE)p->inventory->moveToSelectedSlot(sourceSlot,false);
            if(a.type==A::Use) {
                if(a.face==255) {
                    if(!item)return A::Insufficient;
                    p->aimDirection.set(a.x/32768.f,a.y/32768.f,a.z/32768.f);
                    game->gameMode->useItem(p,p->level,item);
                } else {
                    if(a.face<0||a.face>5||!near(p,a.x,a.y,a.z)||!std::isfinite(a.clickX)||!std::isfinite(a.clickY)||!std::isfinite(a.clickZ)||a.clickX<0||a.clickX>1||a.clickY<0||a.clickY>1||a.clickZ<0||a.clickZ>1)return A::InvalidContext;
                    game->gameMode->useItemOn(p,p->level,item,a.x,a.y,a.z,a.face,Vec3(a.x+a.clickX,a.y+a.clickY,a.z+a.clickZ));
                }
            } else {
                if(!near(p,a.x,a.y,a.z))return A::InvalidContext;
                int id=p->level->getTile(a.x,a.y,a.z),data=p->level->getData(a.x,a.y,a.z);
                Tile* tile=id>0?Tile::tiles[id]:NULL;
                if(!tile||tile==Tile::unbreakable||tile==Tile::invisible_bedrock)return A::InvalidContext;
                bool drops=p->canDestroy(tile);
                if(!p->level->setTile(a.x,a.y,a.z,0))return A::InvalidContext;
                if(!p->abilities.instabuild) {
                    if(item)item->mineBlock(id,a.x,a.y,a.z);
                    if(drops)tile->playerDestroy(p->level,p,a.x,a.y,a.z,data);
                }
                tile->destroy(p->level,a.x,a.y,a.z,data);
            }
            if(item&&item->count<=0)p->inventory->clearSlot(p->inventory->selected);
            return A::Accepted;
        }
        if(a.type==A::Craft) {
            if(a.flags!=0&&a.flags!=1)return A::BadAction;
            if(a.flags==1&&(!near(p,a.x,a.y,a.z)||p->level->getTile(a.x,a.y,a.z)!=Tile::workBench->id))return A::InvalidContext;
        }
        if((a.type==A::Transfer||a.type==A::Armor||a.type==A::Drop)&&(a.flags<0||a.flags>1))return A::BadAction;
        if(a.type==A::Transfer&&(!p->containerMenu||p->containerMenu->containerId!=a.x||!menuValid(p)))return A::InvalidContext;
        std::vector<ItemInstance> items,container;
        ItemInstance armor[4];
        for(int i=Inventory::MAX_SELECTION_SIZE;i<p->inventory->getContainerSize();++i)items.push_back(ItemInstance::cloneSafe(p->inventory->getItem(i)));
        for(int i=0;i<4;++i)armor[i]=ItemInstance::cloneSafe(p->getArmor(i));
        if(p->containerMenu)container=p->containerMenu->getItems();
        ItemInstance dropped;
        if(a.type==A::Drop) {
            if(a.slot<Inventory::MAX_SELECTION_SIZE||a.slot>=p->inventory->getContainerSize()||a.count<=0||a.count>64)return A::BadAction;
            int source=NativeInventoryModel::resolveSource(items,a.slot-Inventory::MAX_SELECTION_SIZE,a,a.count);
            if(source<0)return A::Insufficient;
            dropped=items[source];dropped.count=a.count;
        }
        int status=NativeInventoryModel::apply(items,armor,container,a);
        if(status!=A::Accepted)return status;
        for(size_t i=0;i<items.size();++i)p->inventory->setItem(Inventory::MAX_SELECTION_SIZE+i,&items[i]);
        for(int i=0;i<4;++i)p->setArmor(i,&armor[i]);
        if(a.type==A::Transfer) {
            for(size_t i=0;i<container.size();++i)p->containerMenu->setSlot(i,&container[i]);
            if(auto* menu=dynamic_cast<FurnaceMenu*>(p->containerMenu))menu->furnace->setChanged();
            if(auto* menu=dynamic_cast<ContainerMenu*>(p->containerMenu))if(auto* tile=dynamic_cast<TileEntity*>(menu->container))tile->setChanged();
            p->containerMenu->broadcastChanges();
        }
        for(size_t i=0;i<items.size();++i)if(items[i].count>0)p->inventory->linkEmptySlot(Inventory::MAX_SELECTION_SIZE+i);
        if(a.type==A::Drop)p->drop(new ItemInstance(dropped),a.flags!=0);
        return A::Accepted;
    }
};
#endif
