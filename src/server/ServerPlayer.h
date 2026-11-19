#ifndef ServerPlayer_H__
#define ServerPlayer_H__

#include "../world/entity/player/Player.h"
#include "../world/inventory/BaseContainerMenu.h"
#include <cstdint>
#include <string>

class Minecraft;
class FurnaceTileEntity;
class ItemInstance;
class FillingContainer;
class ChestTileEntity;

class ServerPlayer: public Player,
					public IContainerListener
{
    typedef Player super;
public:
    ServerPlayer(Minecraft* minecraft, Level* level);

	~ServerPlayer();

	void aiStep();
	void tick();
	void take(Entity* e, int orgCount);
	void sendInventory();
    uint64_t inventoryNonce=0, inventoryRevision=0, inventoryAcknowledged=0;
    int inventoryStatus=0;
    bool inventoryV1=false;
    bool inventoryTransactionsSupported=true; // Survival backing stacks, never Creative palettes.
    bool inventoryDeathDropped=false;
    void die(Entity* source) override;
    void finishNativeDeath();
    void releaseNativeUsingItem();
    void reset() override;

	void hurtArmor(int dmg);

	void displayClientMessage(const std::string& messageId);

	void openContainer(ChestTileEntity* furnace);
	void openFurnace(FurnaceTileEntity* furnace);
	void closeContainer();
	void doCloseContainer();

	bool hasResource( int id );
	//
	// IContainerListener
	//
	void setContainerData(BaseContainerMenu* menu, int id, int value);
	void slotChanged(BaseContainerMenu* menu, int slot, const ItemInstance& item, bool isResultSlot);
	void refreshContainer(BaseContainerMenu* menu, const std::vector<ItemInstance>& items);

	virtual void stopSleepInBed(bool forcefulWakeUp, bool updateLevelList, bool saveRespawnPoint);

	void completeUsingItem();
private:
	std::string inventoryFingerprint();
    std::string lastInventoryFingerprint;
    std::string lastVisualFingerprint;
	void nextContainerCounter();
	void setContainerMenu( BaseContainerMenu* menu );

	Minecraft* _mc;
    int _sentHealth;
	int _containerCounter;
};

#endif /*ServerPlayer_H__*/
