#ifndef NET_MINECRAFT_NETWORK_PACKET__SendInventoryPacket_H__
#define NET_MINECRAFT_NETWORK_PACKET__SendInventoryPacket_H__

#include "../Packet.h"
#include "NativeHotbarState.h"

class SendInventoryPacket: public Packet
{
public:
    NativeHotbarState hotbar;
	SendInventoryPacket()
	{
	}

	SendInventoryPacket(Player* player, bool dropItems)
	:	entityId(player->entityId),
		extra(dropItems? ExtraDrop : 0)
	{
        Inventory* inv = player->inventory;
        hotbar.capture(inv);
		numItems = 0;
        for (int i = Inventory::MAX_SELECTION_SIZE; i < inv->getContainerSize(); ++i) {
			++numItems;
			ItemInstance* item = inv->getItem(i);
			items.push_back(item? *item : ItemInstance());
        }
        for (int i = 0; i < NumArmorItems; ++i) {
            ItemInstance* item = player->getArmor(i);
            items.push_back(item? *item : ItemInstance());
        }
	}

	void write(RakNet::BitStream* bitStream)
	{
		bitStream->Write((RakNet::MessageID)(ID_USER_PACKET_ENUM + PACKET_SENDINVENTORY));
		bitStream->Write(entityId);
		bitStream->Write(extra);
		bitStream->Write(numItems);
        // Inventory
		for (int i = 0; i < numItems; ++i)
			PacketUtil::writeItemInstance(items[i], bitStream);
        // Armor
        for (int i = 0; i < NumArmorItems; ++i)
            PacketUtil::writeItemInstance(items[i + numItems], bitStream);
        hotbar.write(bitStream);
	}

	void read(RakNet::BitStream* bitStream)
	{
        valid = false;
        entityId = 0; extra = 0; numItems = 0;
        items.clear(); hotbar.present = false;
        if (!bitStream->Read(entityId) || !bitStream->Read(extra) || !bitStream->Read(numItems)) return;
        if (numItems < 0 || numItems > Inventory::CREATIVE_SLOT_COUNT) return;
        // Every serialized item is exactly a short, a byte and a short.
        if (bitStream->GetNumberOfUnreadBits() < (unsigned int)(numItems + NumArmorItems) * 40) return;
        for (int i = 0; i < numItems + NumArmorItems; ++i)
            items.push_back(PacketUtil::readItemInstance(bitStream));
        if (bitStream->GetNumberOfUnreadBits() > 0) {
            hotbar.read(bitStream);
            if (!hotbar.present) return;
        }
        valid = true;
	}

	void handle(const RakNet::RakNetGUID& source, NetEventCallback* callback)
	{
		callback->handle(source, (SendInventoryPacket*)this);
	}

    bool valid = true;
    int entityId = 0;
	std::vector<ItemInstance> items;
	short numItems = 0;
	unsigned char extra = 0;

	static const int ExtraDrop = 1;
    static const int NumArmorItems = 4;
};

#endif /*NET_MINECRAFT_NETWORK_PACKET__SendInventoryPacket_H__*/
