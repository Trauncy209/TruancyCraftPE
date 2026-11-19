#include "ServerPlayer.h"
#include "../raknet/GetTime.h"
#include "../raknet/RakPeerInterface.h"
#include "../network/packet/PlayerEquipmentPacket.h"
#include "../network/packet/PlayerArmorEquipmentPacket.h"
#include <atomic>
#include "../world/level/Level.h"

#include "../network/RakNetInstance.h"
#include "../network/packet/ContainerOpenPacket.h"
#include "../network/packet/ContainerClosePacket.h"
#include "../network/packet/ContainerSetDataPacket.h"
#include "../network/packet/ContainerSetSlotPacket.h"
#include "../network/packet/ContainerSetContentPacket.h"
#include "../network/packet/ChatPacket.h"
#include "../network/packet/EntityEventPacket.h"
#include "../network/packet/SetHealthPacket.h"
#include "../network/packet/TakeItemEntityPacket.h"
#include "../network/packet/AddItemEntityPacket.h"
#include "../client/Minecraft.h"
#include "../world/level/tile/entity/FurnaceTileEntity.h"
#include "../world/inventory/FurnaceMenu.h"
#include "../world/inventory/FillingContainer.h"
#include "../world/inventory/ContainerMenu.h"
#include "../world/entity/EntityEvent.h"
#include "../network/packet/AnimatePacket.h"
#include "../world/level/tile/entity/ChestTileEntity.h"
#include "../network/packet/HurtArmorPacket.h"

ServerPlayer::ServerPlayer( Minecraft* minecraft, Level* level )
:   super(level, minecraft->isCreativeMode()),
	_mc(minecraft),
	_sentHealth(-999),
	_containerCounter(0)
{
    // Remote clients send MovePlayer; the handler relays it to other peers.
    // Generic Mob movement broadcasts include the owner and fight its input.
    inventoryTransactionsSupported=!minecraft->isCreativeMode();
    static std::atomic<uint32_t> nextNonce(1);
    inventoryNonce = (uint64_t(RakNet::GetTimeUS()) << 16) ^ nextNonce.fetch_add(1);
    if(!inventoryNonce)inventoryNonce=nextNonce.fetch_add(1);
    autoSendPosRot=false;
	// Dedicated/LAN authority must retain picked-up items for player saves.
	hasFakeInventory = false;
    dimension = 0;
	footSize = 0;
}

ServerPlayer::~ServerPlayer() {
	setContainerMenu(NULL);
}

void ServerPlayer::stopSleepInBed(bool forcefulWakeUp, bool updateLevelList, bool saveRespawnPoint) {
	if(isSleeping()) {
		AnimatePacket packet(AnimatePacket::WAKE_UP, this);
		_mc->raknetInstance->send(owner, packet);
	}
	super::stopSleepInBed(forcefulWakeUp, updateLevelList, saveRespawnPoint);
}


void ServerPlayer::aiStep() {
    updateAttackAnim();
    super::aiStep();
}

void ServerPlayer::tick() {
	super::tick();

	if(!useItem.isNull()) {
        if(inventoryV1) {
            ItemInstance* selected=inventory->getSelected();
            if(!selected||selected->id!=useItem.id||selected->getAuxValue()!=useItem.getAuxValue()) {useItem.setNull();stopUsingItem();}
            else useItem.count=selected->count;
        }
        if(!useItem.isNull()) {
            --useItemDuration;
            if(inventoryV1 && useItemDuration<=0)completeUsingItem();
        }
    }

	//LOGI("Server:tick. Cmenu: %p\n", containerMenu);
	if (containerMenu)
		containerMenu->broadcastChanges();

    if(inventoryV1 && inventoryFingerprint()!=lastInventoryFingerprint)sendInventory();

	if (health != _sentHealth) {
		_sentHealth = health;
		SetHealthPacket packet(health);
		_mc->raknetInstance->send(owner, packet);
	}
}

void ServerPlayer::take( Entity* e, int orgCount ) {
	if (e->isItemEntity() && static_cast<ItemEntity*>(e)->item.count > 0) {
		// A partial pickup changes the world stack, not its existence.
		AddItemEntityPacket packet(static_cast<ItemEntity*>(e));
		_mc->raknetInstance->send(packet);
	} else {
		TakeItemEntityPacket packet(e->entityId, entityId);
		_mc->raknetInstance->send(packet);
	}

	super::take(e, orgCount);
	sendInventory();
}

void ServerPlayer::sendInventory() {
    // Exhausted stacks must become actual empty backing slots, not invisible
    // zero-count items that permanently occupy an inventory position.
    for(int i=Inventory::MAX_SELECTION_SIZE;i<inventory->getContainerSize();++i) {
        ItemInstance* item=inventory->getItem(i);
        if(item&&item->count<=0){ItemInstance empty;inventory->setItem(i,&empty);}
    }
	std::vector<ItemInstance> items;
	for (int i = Inventory::MAX_SELECTION_SIZE; i < inventory->getContainerSize(); ++i)
		items.push_back(ItemInstance::cloneSafe(inventory->getItem(i)));
	ContainerSetContentPacket packet(0, items);
	packet.hotbar.capture(inventory);
    packet.nativeState.present=inventoryTransactionsSupported;
    packet.nativeState.nonce=inventoryNonce;
    packet.nativeState.revision=++inventoryRevision;
    packet.nativeState.acknowledged=inventoryAcknowledged;
    packet.nativeState.status=inventoryStatus;
    for(int i=0;i<4;++i)packet.nativeState.armor[i]=ItemInstance::cloneSafe(getArmor(i));
    if(containerMenu){packet.nativeState.containerId=containerMenu->containerId;packet.nativeState.containerItems=containerMenu->getItems();}
    lastInventoryFingerprint=inventoryFingerprint();
	_mc->raknetInstance->send(owner, packet);
    if(inventoryV1) {
        // Other players still receive the legacy visual equipment packets.
        ItemInstance* selected=inventory->getSelected();
        PlayerEquipmentPacket held(entityId,selected?selected->id:0,selected?selected->getAuxValue():0);
        PlayerArmorEquipmentPacket armor(this);
        RakNet::BitStream signature;held.write(&signature);armor.write(&signature);
        std::string state((const char*)signature.GetData(),signature.GetNumberOfBytesUsed());
        if(state!=lastVisualFingerprint) {
            lastVisualFingerprint=state;
            for(Packet* visual:{static_cast<Packet*>(&held),static_cast<Packet*>(&armor)}) {
                RakNet::BitStream wire;visual->write(&wire);
                _mc->raknetInstance->getPeer()->Send(&wire,visual->priority,visual->reliability,0,owner,true);
            }
        }
    }
}

std::string ServerPlayer::inventoryFingerprint() {
    RakNet::BitStream wire;
    for(int i=Inventory::MAX_SELECTION_SIZE;i<inventory->getContainerSize();++i)PacketUtil::writeItemInstance(ItemInstance::cloneSafe(inventory->getItem(i)),&wire);
    NativeHotbarState hotbar;hotbar.capture(inventory);hotbar.write(&wire);
    for(int i=0;i<4;++i)PacketUtil::writeItemInstance(ItemInstance::cloneSafe(getArmor(i)),&wire);
    wire.Write(containerMenu?containerMenu->containerId:-1);
    if(containerMenu)for(const ItemInstance& item:containerMenu->getItems())PacketUtil::writeItemInstance(item,&wire);
    return std::string((const char*)wire.GetData(),wire.GetNumberOfBytesUsed());
}
void ServerPlayer::die(Entity* source) {
    super::die(source);
    finishNativeDeath();
}
void ServerPlayer::finishNativeDeath() {
    if(inventoryV1&&!inventoryDeathDropped) {
        inventoryDeathDropped=true;
        if(!level->getLevelData()->getKeepInventory()) {
            inventory->dropAll(false);
            for(int i=0;i<4;++i){ItemInstance item=ItemInstance::cloneSafe(getArmor(i));if(item.count>0)drop(new ItemInstance(item),true);setArmor(i,NULL);}
        }
        sendInventory();
    }
}
void ServerPlayer::reset() {super::reset();inventoryDeathDropped=false;}

void ServerPlayer::hurtArmor(int dmg) {
    super::hurtArmor(dmg);
    if(inventoryV1)sendInventory();
    else {HurtArmorPacket packet(dmg);_mc->raknetInstance->send(owner, packet);}
}

void ServerPlayer::openContainer( ChestTileEntity* container) {
	LOGI("Client is opening a container\n");
	nextContainerCounter();
	ContainerOpenPacket packet(_containerCounter, ContainerType::CONTAINER, container->getName(), container->getContainerSize());
	_mc->raknetInstance->send(owner, packet);
	setContainerMenu(new ContainerMenu(container, container->runningId));
}

void ServerPlayer::openFurnace( FurnaceTileEntity* furnace ) {
	LOGI("Client is opening a furnace\n");
	nextContainerCounter();
	ContainerOpenPacket packet(_containerCounter, ContainerType::FURNACE, furnace->getName(), furnace->getContainerSize());
	_mc->raknetInstance->send(owner, packet);
	setContainerMenu(new FurnaceMenu(furnace));
}

void ServerPlayer::closeContainer() {
	LOGI("Client is closing a container\n");
	ContainerClosePacket packet(containerMenu->containerId);
	_mc->raknetInstance->send(owner, packet);
	doCloseContainer();
}

void ServerPlayer::doCloseContainer() {
	if (!containerMenu) {
		LOGE("Container is missing @ doCloseContainer!\n");
	}
	setContainerMenu(NULL);
}

bool ServerPlayer::hasResource( int id ) {
    return inventoryV1?super::hasResource(id):true;
}

//
// IContainerListener
//
void ServerPlayer::setContainerData( BaseContainerMenu* menu, int id, int value ) {
	ContainerSetDataPacket p(menu->containerId, id, value);
	_mc->raknetInstance->send(owner, p);
	//LOGI("Setting container data for id %d: %d\n", id, value);
}

void ServerPlayer::slotChanged( BaseContainerMenu* menu, int slot, const ItemInstance& item, bool isResultSlot ) {
	if (isResultSlot) return;
	ContainerSetSlotPacket p(menu->containerId, slot, item);
	_mc->raknetInstance->send(owner, p);
	//LOGI("Slot %d changed\n", slot);
}

void ServerPlayer::refreshContainer( BaseContainerMenu* menu, const std::vector<ItemInstance>& items ) {
	ContainerSetContentPacket p(menu->containerId, menu->getItems());
	_mc->raknetInstance->send(owner, p);
	//LOGI("Refreshing container with %d items\n", items.size());
}

void ServerPlayer::nextContainerCounter() {
	if (++_containerCounter >= 100)
		_containerCounter = 0;
}

void ServerPlayer::setContainerMenu( BaseContainerMenu* menu ) {
	if (containerMenu == menu)
		return;

	if (containerMenu)
		delete containerMenu;

	containerMenu = menu;
	if (containerMenu) {
		containerMenu->containerId = _containerCounter;
		containerMenu->setListener(this);
	}
}

void ServerPlayer::releaseNativeUsingItem() {
    ItemInstance* selected=inventory->getSelected();
    if(useItem.isNull()||!selected||selected->id!=useItem.id||selected->getAuxValue()!=useItem.getAuxValue()||
       (useItem.getItem()->getUseAnimation()==UseAnim::bow&&!abilities.instabuild&&!inventory->hasResource(Item::arrow->id))) {
        useItem.setNull();stopUsingItem();sendInventory();return;
    }
    useItem.count=selected->count;
    useItem.releaseUsing(level,this,useItemDuration);
    *selected=useItem;
    if(selected->count<=0)inventory->clearSlot(inventory->selected);
    useItem.setNull();stopUsingItem();sendInventory();
}

void ServerPlayer::completeUsingItem() {
    if(inventoryV1) {
        ItemInstance* selected=inventory->getSelected();
        if(!selected||selected->id!=useItem.id||selected->getAuxValue()!=useItem.getAuxValue()) {useItem.setNull();stopUsingItem();return;}
        useItem.count=selected->count;
    }
	EntityEventPacket p(entityId, EntityEvent::USE_ITEM_COMPLETE);
    if(!inventoryV1)_mc->raknetInstance->send(owner, p);
	super::completeUsingItem();
    if(inventoryV1) {
        // Negotiated clients handle this as visual-only completion.
        _mc->raknetInstance->send(owner,p);
        sendInventory();
    }
}

void ServerPlayer::displayClientMessage( const std::string& messageId ) {
	ChatPacket package(messageId);
	_mc->raknetInstance->send(owner, package);
}
