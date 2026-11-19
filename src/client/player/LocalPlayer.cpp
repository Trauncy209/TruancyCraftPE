#include "LocalPlayer.h"
#include "../../platform/input/Keyboard.h"
#include "../../network/java/JavaGameSession.h"
#include "../Minecraft.h"
#include "../../ErrorCodes.h"
#include "../../world/entity/EntityEvent.h"
#include "../../world/entity/player/Player.h"
#include "../../world/inventory/BaseContainerMenu.h"
#include "../../world/item/BowItem.h"
#include "../../world/level/Level.h"
#include "../../world/level/LevelConstants.h"
#include "../../util/Mth.h"
#include "../../world/level/tile/Tile.h"
#include "../../world/level/tile/entity/TileEntity.h"
#include "../../world/level/material/Material.h"
#include "../../network/packet/ContainerClosePacket.h"
#include "../../network/packet/MovePlayerPacket.h"
#include "../../network/packet/RequestChunkPacket.h"
#include "../../network/packet/PlayerEquipmentPacket.h"
#include "../../network/RakNetInstance.h"
#include "../../network/ClientSideNetworkHandler.h"
#include "../../network/packet/DropItemPacket.h"
#include "../../network/packet/SetHealthPacket.h"
#include "../../network/packet/SendInventoryPacket.h"
#include "../../network/packet/EntityEventPacket.h"
#include "../../network/packet/PlayerActionPacket.h"
#ifndef STANDALONE_SERVER
#include "../gui/Screen.h"
#include "../gui/screens/FurnaceScreen.h"
#include "../gui/screens/JavaInventoryScreen.h"
#include "../gui/screens/ChestScreen.h"
#include "../gui/screens/crafting/WorkbenchScreen.h"
#include "../gui/screens/crafting/StonecutterScreen.h"
#include "../gui/screens/InBedScreen.h"
#include "../gui/screens/TextEditScreen.h"
#include "../particle/TakeAnimationParticle.h"
#endif
#include "../../network/packet/AnimatePacket.h"
#include "../../world/item/ArmorItem.h"
#include "../../network/packet/PlayerArmorEquipmentPacket.h"

//@note: doesn't work completely, since it doesn't care about stairs rotation
static bool isJumpable(int tileId) {
	return tileId != Tile::fence->id
		&& tileId != Tile::fenceGate->id
		&& tileId != Tile::stoneSlabHalf->id
		&& tileId != Tile::trapdoor->id
        && tileId != Tile::sign->id
        && tileId != Tile::wallSign->id
		&& (Tile::tiles[tileId] != NULL && Tile::tiles[tileId]->getRenderShape() != Tile::SHAPE_STAIRS);
}

LocalPlayer::LocalPlayer(Minecraft* minecraft, Level* level, User* user, int dimension, bool isCreative)
:	Player(level, isCreative),
	minecraft(minecraft),
	input(NULL),
	sentInventoryItemId(-1),
	sentInventoryItemData(-1),
	autoJumpEnabled(true),
	armorTypeHash(0),
	sprinting(false),
	sprintDoubleTapTimer(0),
	prevForwardHeld(false)
{
	this->dimension = dimension;
	_init();

	if (user != NULL) {
		if (user->name.length() > 0)
			//customTextureUrl = "http://s3.amazonaws.com/MinecraftSkins/" + user.name + ".png";
			this->name = user->name;
	}
}

LocalPlayer::~LocalPlayer() {
	//delete input;
	//input = NULL;
}

/*private*/
void LocalPlayer::calculateFlight(float xa, float ya, float za) {

    xa = xa * minecraft->options.flySpeed;
    ya = 0;
    za = za * minecraft->options.flySpeed;

#ifdef ANDROID
    if (Keyboard::isKeyDown(103)) ya = .2f * minecraft->options.flySpeed;
    if (Keyboard::isKeyDown(102)) ya = -.2f * minecraft->options.flySpeed;
#else
    if (Keyboard::isKeyDown(Keyboard::KEY_E)) ya = .2f * minecraft->options.flySpeed;
    if (Keyboard::isKeyDown(Keyboard::KEY_Q)) ya = -.2f * minecraft->options.flySpeed;
#endif

    flyX = 10 * smoothFlyX.getNewDeltaValue(xa, .35f * minecraft->options.sensitivity);
    flyY = 10 * smoothFlyY.getNewDeltaValue(ya, .35f * minecraft->options.sensitivity);
    flyZ = 10 * smoothFlyZ.getNewDeltaValue(za, .35f * minecraft->options.sensitivity);
}

bool LocalPlayer::isSolidTile(int x, int y, int z) {
	if (level->isClientSide && !level->hasChunkAtNow(x, y, z))
		return false;
	int tileId = level->getTile(x, y, z);
	return tileId > 0 && Tile::tiles[tileId]->material->isSolid();
}

bool LocalPlayer::nativeTransactionsEnabled() const {
    return nativeTransactions.enabled() && level && level->isClientSide &&
           !level->isJavaServerLevel() && !minecraft->javaSession;
}

bool LocalPlayer::nativeApplySnapshot(const std::vector<ItemInstance>& items, const NativeHotbarState& hotbar, const NativeInventorySnapshot& state) {
    if (items.size() != static_cast<size_t>(inventory->getContainerSize()-Inventory::MAX_SELECTION_SIZE)) return false;
    const bool negotiating=!nativeTransactions.enabled();
    const uint64_t previousAcknowledged=nativeTransactions.acknowledged;
    bool rejectedUse=false;
    if(!negotiating && state.acknowledged>previousAcknowledged && state.status!=NativeInventoryAction::Accepted)
        for(const auto& pending:nativeTransactions.pending)
            if(pending.action.sequence==state.acknowledged && pending.action.type==NativeInventoryAction::Use)rejectedUse=true;
    if(nativeTransactionsEnabled() && state.nonce==nativeTransactions.nonce)nativeFlushHotbar();
    if (!nativeTransactions.accept(items,hotbar,state)) return false;
    nativeInventoryReady=true;
    nativeApplyPrediction();
    if(!negotiating && state.acknowledged>previousAcknowledged) {
        const char* notice=NULL;
        switch(state.status) {
        case NativeInventoryAction::NoSpace: notice="Inventory is full."; break;
        case NativeInventoryAction::Insufficient: notice="Not enough items."; break;
        case NativeInventoryAction::InvalidContext: notice="That item or container changed. Try again."; break;
        case NativeInventoryAction::BadAction:
        case NativeInventoryAction::Unsupported: notice="That action could not be completed."; break;
        default: break;
        }
        if(rejectedUse)nativeStopUsingItem();
        if(notice)displayClientMessage(notice);
    }
    if(negotiating) {
        // Explicitly opt into the advertised protocol before any legacy path
        // can mutate server-owned resources (including death while idle).
        NativeInventoryAction hello;hello.type=NativeInventoryAction::Hotbar;
        nativeSubmitAction(hello,&nativeTransactions.predictedHotbar);
    }
    return true;
}

void LocalPlayer::nativeApplyPrediction() {
    for (unsigned int i=0;i<nativeTransactions.items.size();++i)
        inventory->setItem(Inventory::MAX_SELECTION_SIZE+i,&nativeTransactions.items[i]);
    nativeTransactions.predictedHotbar.apply(inventory);
    for(int i=0;i<NUM_ARMOR;++i)setArmor(i,&nativeTransactions.armor[i]);
    if(containerMenu && nativeTransactions.base.containerId==containerMenu->containerId &&
       containerMenu->getItems().size()==nativeTransactions.container.size())
        for(unsigned int i=0;i<nativeTransactions.container.size();++i)
            containerMenu->setSlot(i,&nativeTransactions.container[i]);
}

void LocalPlayer::nativeStopUsingItem() {
    useItem.setNull();useItemDuration=0;nativeUseCompletionWaitTicks=0;
}

void LocalPlayer::completeUsingItem() {
    if(!nativeTransactionsEnabled()) {super::completeUsingItem();return;}
    if(useItem.isNull())return;
    // The server already consumed the food. This event finishes only the local
    // animation; clear first so repeated events cannot play it twice.
    ItemInstance completed=useItem;
    nativeStopUsingItem();
    const auto animation=completed.getUseAnimation();
    if(animation==UseAnim::eat || animation==UseAnim::drink) {
        spawnEatParticles(&completed,10);
        if(animation==UseAnim::eat)
            level->playSound(this,"random.burp",.5f,1.f);
    }
}

void LocalPlayer::nativeFlushHotbar() {
    if(!nativeTransactionsEnabled() || !nativeInventoryReady)return;
    NativeHotbarState desired;desired.capture(inventory);
    if(NativeInventoryClientState::sameHotbar(desired,nativeTransactions.predictedHotbar))return;
    NativeInventoryAction action;action.type=NativeInventoryAction::Hotbar;
    nativeSubmitAction(action,&desired);
}

bool LocalPlayer::nativeSubmitAction(NativeInventoryAction action,const NativeHotbarState* hotbar) {
    if(!nativeTransactionsEnabled() || !nativeInventoryReady)return false;
    if(action.type==NativeInventoryAction::Use && action.face==255) {
        ItemInstance* selected=inventory->getSelected();
        if(selected && selected->getUseAnimation()==UseAnim::eat && !isHurt())return false;
    }
    if(action.type!=NativeInventoryAction::Hotbar)nativeFlushHotbar();
    NativeHotbarState desired=hotbar?*hotbar:nativeTransactions.predictedHotbar;
    if(!nativeTransactions.enqueue(action,desired))return false;
    if(action.type==NativeInventoryAction::Use)nativeUseCompletionWaitTicks=0;
    SendInventoryPacket packet(this,false);packet.action=action;packet.hotbar=desired;
    minecraft->raknetInstance->send(packet);
    nativeApplyPrediction();
    return true;
}

int LocalPlayer::nativeBackingSlot(const ItemInstance* item) const {
    if(!item)return -1;
    for(int i=Inventory::MAX_SELECTION_SIZE;i<inventory->getContainerSize();++i)
        if(inventory->getItem(i)==item)return i;
    return -1;
}

bool LocalPlayer::nativeDropSlot(int slot,int count,bool randomly) {
    if(slot>=0 && slot<Inventory::MAX_SELECTION_SIZE)slot=inventory->linkedSlots[slot].inventorySlot;
    ItemInstance* item=inventory->getItem(slot);
    if(!item || item->count<=0)return false;
    int remaining=count<0?item->count:count;
    if(remaining<=0||remaining>item->count||nativeTransactions.pending.size()+(remaining+63)/64+1>128)return false;
    const int id=item->id,aux=item->getAuxValue();
    while(remaining>0) {
        NativeInventoryAction action;action.type=NativeInventoryAction::Drop;action.slot=slot;
        action.count=std::min(remaining,64);action.flags=randomly?1:0;
        action.expectedId=id;action.expectedAux=aux;
        if(!nativeSubmitAction(action))return false;
        remaining-=action.count;
    }
    return true;
}

bool LocalPlayer::nativeTransfer(int containerId,bool fromContainer,int slot,int destination,int count) {
    if(!nativeTransactionsEnabled() || containerId!=nativeTransactions.base.containerId)return false;
    nativeFlushHotbar();
    const auto& source=fromContainer?nativeTransactions.container:nativeTransactions.items;
    const int index=fromContainer?slot:slot-Inventory::MAX_SELECTION_SIZE;
    if(index<0||index>=int(source.size())||source[index].count<=0)return false;
    NativeInventoryAction action;action.type=NativeInventoryAction::Transfer;action.flags=fromContainer?1:0;
    action.x=containerId;action.slot=slot;action.slot2=destination;
    action.expectedId=source[index].id;action.expectedAux=source[index].getAuxValue();
    for(action.count=std::min(64,std::min(count,source[index].count));action.count>0;--action.count) {
        auto items=nativeTransactions.items,container=nativeTransactions.container;
        ItemInstance armor[4];for(int i=0;i<4;++i)armor[i]=nativeTransactions.armor[i];
        const int status=NativeInventoryModel::apply(items,armor,container,action);
        if(status==NativeInventoryAction::Accepted)return nativeSubmitAction(action);
        if(status!=NativeInventoryAction::NoSpace)return false;
    }
    return false;
}

void LocalPlayer::nativeInventoryRestored() {
    nativeInventoryReady = true;
    SendInventoryPacket snapshot(this, false);
    RakNet::BitStream wire; snapshot.write(&wire);
    nativeInventoryState.assign((const char*)wire.GetData(), wire.GetNumberOfBytesUsed());
}

void LocalPlayer::tick() {

	super::tick();
	if(!useItem.isNull()) {
        const bool native=nativeTransactionsEnabled();
        ItemInstance* item=inventory->getSelected();
        const bool sameNativeItem=item && item->count>0 && item->id==useItem.id && item->getAuxValue()==useItem.getAuxValue();
        if(item && (native?sameNativeItem:*item==useItem)) {
            // A pickup can increase the held stack while eating. Its identity
            // remains the same; keep the animation and the server's new count.
            if(native)useItem.count=item->count;
            if(!native || useItemDuration>0) {
                if(useItemDuration<=25 && useItemDuration%4==0)spawnEatParticles(item,5);
                if(--useItemDuration==0) {
                    if(native) {
                        // Keep the visual item until the authoritative completion
                        // event arrives. Never consume from this client copy.
                    } else if(minecraft->javaSession) {
                        // Java owns its completion and inventory consumption.
                    } else if(!level->isClientSide)completeUsingItem();
                    else {
                        EntityEventPacket packet(entityId,EntityEvent::USE_ITEM_COMPLETE);
                        level->raknetInstance->send(packet);
                    }
                }
            } else if(native && ++nativeUseCompletionWaitTicks>=100) {
                // A rejected/no-op use or interrupted connection must not leave
                // the hand pose stuck while waiting for a completion visual.
                nativeStopUsingItem();
            }
        } else {
            if(native)nativeStopUsingItem();else stopUsingItem();
        }
    }
	if (minecraft->isOnline() && !minecraft->javaSession)
	{
		if (std::abs(x - sentX) > .1f || std::abs(y - sentY) > .01f || std::abs(z - sentZ) > .1f || std::abs(sentRotX - xRot) > 1 || std::abs(sentRotY - yRot) > 1)
		{
			MovePlayerPacket packet(entityId, x, y - heightOffset, z, xRot, yRot);
			minecraft->raknetInstance->send(packet);
			sentX = x;
			sentY = y;
			sentZ = z;
			sentRotX = xRot;
			sentRotY = yRot;
		}

        // Native crafting, consumption and hotbar arrangement happen on the client.
        // Sync changed state only, after the initial server inventory was received.
        if (nativeTransactionsEnabled()) {
            nativeFlushHotbar();
        } else if (nativeInventoryReady) {
            SendInventoryPacket snapshot(this, false);
            RakNet::BitStream wire; snapshot.write(&wire);
            std::string state((const char*)wire.GetData(), wire.GetNumberOfBytesUsed());
            if (state != nativeInventoryState) {
                minecraft->raknetInstance->send(snapshot);
                nativeInventoryState.swap(state);
            }
        }
		ItemInstance* item = inventory->getSelected();
		int newItemId   = (item && item->count > 0) ? item->id : 0;
		int newItemData = (item && item->count > 0) ? item->getAuxValue() : 0;

		if (!nativeTransactionsEnabled() && (sentInventoryItemId != newItemId
		||  sentInventoryItemData != newItemData))
		{
			sentInventoryItemId   = newItemId;
			sentInventoryItemData = newItemData;
			PlayerEquipmentPacket packet(entityId, newItemId, newItemData);
			minecraft->raknetInstance->send(packet);
		}
	}
/*
    for (int i = 0; i < 4; ++i) {
        ItemInstance* a = getArmor(i);
        if (!a) continue;

        ArmorItem* item = (ArmorItem*) a->getItem();

        printf("armor %d: %d\n", i, a->getAuxValue());
    }
/**/

	updateArmorTypeHash();
#ifndef STANDALONE_SERVER
	if (!minecraft->screen && containerMenu) {
		static bool hasPostedError = false;
		if (!hasPostedError) {
			minecraft->gui.postError( ErrorCodes::ContainerRefStillExistsAfterDestruction );
			hasPostedError = true;
		}
	}
#endif
	//LOGI("biome: %s\n", level->getBiomeSource()->getBiome((int)x >> 4, (int)z >> 4)->name.c_str());
}

/*public*/
void LocalPlayer::aiStep() {
	jumpTriggerTime--;
	ascendTriggerTime--;
	descendTriggerTime--;

	bool wasJumping = input->jumping;
#ifndef STANDALONE_SERVER
    bool screenCovering = minecraft->screen && !minecraft->screen->passEvents;
	if (!screenCovering)
		input->tick(this);

	// Sprint: detect W double-tap
	{
		bool forwardHeld = (input->ya > 0);
		if (forwardHeld && !prevForwardHeld) {
			// leading edge of W press
			if (sprintDoubleTapTimer > 0)
				sprinting = true;
			else
				sprintDoubleTapTimer = 7;
		}
		if (!screenCovering && forwardHeld && minecraft->options.keySprint.key && Keyboard::isKeyDown(minecraft->options.keySprint.key))
            sprinting = true;
        if (!forwardHeld || screenCovering) {
            sprinting = false;
		}
		if (sprintDoubleTapTimer > 0) sprintDoubleTapTimer--;
		prevForwardHeld = forwardHeld;
	}
	if (input->sneaking || abilities.flying)
		sprinting = false;

    if (input->sneaking) {
        if (ySlideOffset < 0.2f) ySlideOffset = 0.2f;
    }
#endif
	if (abilities.mayfly) {
		// Check for flight toggle
		if (!wasJumping && input->jumping) {
			if (jumpTriggerTime <= 0) jumpTriggerTime = 7;
			else {
				abilities.flying = !abilities.flying;
				jumpTriggerTime = 0;
			}
		}
		if (abilities.flying) {
			if (input->wantUp) {
				yd += 0.15f;
				//xd = zd = 0;
			}
			if (input->wantDown) {
				yd -= 0.15f;
			}
		}
	}

	if(isUsingItem()) {
		const float k = 0.35f;
		input->xa *= k;
		input->ya *= k;
	}

	Mob::aiStep();
    super::aiStep();

	//if (onGround && abilities.flying)
	//	abilities.flying = false;

	if (interpolateOnly())
		updateAi();
}

/*public*/
void LocalPlayer::closeContainer() {
	if (minecraft->javaSession) {
		minecraft->javaSession->connection.send("{\"type\":\"close_container\",\"window_id\":" + std::to_string(minecraft->javaSession->inventory.windowId) + "}");
		minecraft->setScreen(NULL);
		return;
	}
	if (level->isClientSide) {
		ContainerClosePacket packet(containerMenu->containerId);
		minecraft->raknetInstance->send(packet);
	}
	super::closeContainer();
    minecraft->setScreen(NULL);
}

//@Override
void LocalPlayer::move(float xa, float ya, float za) {
    // Native server terrain is usable only after its authoritative snapshot.
    // Hold at the loaded frontier instead of falling through pending chunks.
    if (minecraft && level && level->isClientSide && !level->isJavaServerLevel()
        && minecraft->netCallback) {
        ClientSideNetworkHandler* handler = static_cast<ClientSideNetworkHandler*>(minecraft->netCallback);
        const int x0 = Mth::floor(std::min(bb.x0, bb.x0 + xa) / CHUNK_WIDTH);
        const int x1 = Mth::floor((std::max(bb.x1, bb.x1 + xa) - 0.0001f) / CHUNK_WIDTH);
        const int z0 = Mth::floor(std::min(bb.z0, bb.z0 + za) / CHUNK_DEPTH);
        const int z1 = Mth::floor((std::max(bb.z1, bb.z1 + za) - 0.0001f) / CHUNK_DEPTH);
        for (int cx = x0; cx <= x1; ++cx) {
            for (int cz = z0; cz <= z1; ++cz) {
                if (!handler->isChunkLoaded(cx, cz)) {
                    xd = yd = zd = 0;
                    return;
                }
            }
        }
    }
    //@note: why is this == minecraft->player needed?
    if (this == minecraft->player && minecraft->options.isFlying) {
        noPhysics = true;
        float tmp = walkDist; // update
        calculateFlight((float) xa, (float) ya, (float) za);
        fallDistance = 0;
        yd = 0;
        super::move(flyX, flyY, flyZ);
        onGround = true;
        walkDist = tmp;
    } else {
		if (autoJumpTime > 0) {
			autoJumpTime--;
			input->jumping = true;
		}
		float prevX = x, prevZ = z;
		const int prevChunkX = Mth::floor(prevX / (float)CHUNK_WIDTH);
		const int prevChunkZ = Mth::floor(prevZ / (float)CHUNK_DEPTH);

        super::move(xa, ya, za);

		float newX = x, newZ = z;
		const int newChunkX = Mth::floor(newX / (float)CHUNK_WIDTH);
		const int newChunkZ = Mth::floor(newZ / (float)CHUNK_DEPTH);

		if (minecraft && minecraft->isOnlineClient() && minecraft->netCallback
			&& (prevChunkX != newChunkX || prevChunkZ != newChunkZ)) {
			ClientSideNetworkHandler* handler = static_cast<ClientSideNetworkHandler*>(minecraft->netCallback);
			handler->recenterChunkRequests();
		}

		if (autoJumpTime <= 0 && autoJumpEnabled)
		{
			// auto-jump when crossing the middle of a tile, and the tile in the front is blocked
			bool jump = false;
			if (Mth::floor(prevX * 2.0f) != Mth::floor(newX * 2.0f) || Mth::floor(prevZ * 2.0f) != Mth::floor(newZ * 2.0f))
			{
				float dist = Mth::sqrt(xa * xa + za * za);
				const int xx = Mth::floor(x + xa / dist);
				const int zz = Mth::floor(z + za / dist);
				const bool areaReady = level->isClientSide
					? (level->hasChunkAtNow(xx, (int)(y-1), zz)
						&& level->hasChunkAtNow(xx, (int)y, zz)
						&& level->hasChunkAtNow(xx, (int)(y+1), zz))
					: (level->hasChunkAt(xx, (int)(y-1), zz)
						&& level->hasChunkAt(xx, (int)y, zz)
						&& level->hasChunkAt(xx, (int)(y+1), zz));
				if (areaReady) {
					const int tileId = level->getTile(xx, (int)(y-1), zz);
					jump = (isSolidTile(xx, (int)(y-1), zz) // Solid block to jump up on
						&& !isSolidTile(xx, (int)y, zz) && !isSolidTile(xx, (int)(y+1), zz)) // Enough space
						&& isJumpable(tileId);
				}
			}
			if (jump)
			{
				autoJumpTime = 1;
			}
		}
    }
}

void LocalPlayer::updateAi() {
    super::updateAi();
    this->xxa = input->xa;
    this->yya = input->ya;
    this->jumping = input->jumping || autoJumpTime > 0;
}

void LocalPlayer::take( Entity* e, int orgCount )
{
#ifndef STANDALONE_SERVER
	if (e->isItemEntity())
		minecraft->particleEngine->add(new TakeAnimationParticle(minecraft->level, (ItemEntity*)e, this, -0.5f));
#endif
}

void LocalPlayer::setKey( int eventKey, bool eventKeyState )
{
	input->setKey(eventKey, eventKeyState);
}

void LocalPlayer::releaseAllKeys()
{
	if (input) input->releaseAllKeys();
}

float LocalPlayer::getWalkingSpeedModifier() {
	return sprinting ? 1.3f : 1.0f;
}

float LocalPlayer::getFieldOfViewModifier() {
	float targetFov = 1.0f;
	if(abilities.flying) targetFov *= 1.1f;
	targetFov *= ((walkingSpeed * getWalkingSpeedModifier()) / DEFAULT_WALK_SPEED +1) / 2;

	if(isUsingItem() && getUseItem()->id == Item::bow->id) {
		float ticksHeld = (float)getTicksUsingItem();
		float scale = ticksHeld / BowItem::MAX_DRAW_DURATION;
		if(scale > 1) {
			scale = 1;
		}
		else {
			scale *= scale;
		}
		targetFov *= 1.0f - scale * 0.15f;
	}
	return targetFov;
}
void LocalPlayer::addAdditonalSaveData( CompoundTag* entityTag )
{
	super::addAdditonalSaveData(entityTag);
	entityTag->putInt("Score", score);
}

void LocalPlayer::readAdditionalSaveData( CompoundTag* entityTag )
{
	super::readAdditionalSaveData(entityTag);
	score = entityTag->getInt("Score");
}

bool LocalPlayer::isSneaking()
{
	return input->sneaking;
}

void LocalPlayer::hurtTo( int newHealth )
{
	int dmg = health - newHealth;
	if (dmg <= 0) {
		this->health = newHealth;
	} else {
		lastHurt = dmg;
		lastHealth = health;
		invulnerableTime = invulnerableDuration;

		minecraft->player->bypassArmor = true;
		actuallyHurt(dmg);
		minecraft->player->bypassArmor = false;

		hurtTime = hurtDuration = 10;
	}
}

void LocalPlayer::actuallyHurt( int dmg )
{
#ifndef STANDALONE_SERVER
    if (minecraft->screen && minecraft->screen->closeOnPlayerHurt()) {
		if (containerMenu) closeContainer();
        else minecraft->setScreen(NULL);
	}
#endif
    super::actuallyHurt(dmg);
}

void LocalPlayer::respawn()
{
	if (minecraft->javaSession) {
		minecraft->javaSession->connection.send("{\"type\":\"respawn\"}");
		return;
	}
	minecraft->respawnPlayer();
}

void LocalPlayer::die(Entity* source)
{
    if(nativeTransactionsEnabled()) {
        NativeInventoryAction action;action.type=NativeInventoryAction::Death;
        nativeSubmitAction(action);
        super::die(source);
        return;
    }
	if (minecraft->javaSession) {
		// Preserve server-owned slots; the server decides drops and respawn.
		super::die(source);
		return;
	}
	bool keepInventory = level && level->getLevelData() && level->getLevelData()->getKeepInventory();
	// If we're an online client, send the inventory to be dropped
	// If we're the server, drop the inventory immediately
	if (!keepInventory && level->isClientSide) {
		SendInventoryPacket packet(this, true);
		minecraft->raknetInstance->send(packet);
	}
	if (!keepInventory)
		inventory->dropAll(level->isClientSide);
	if (!keepInventory) for (int i = 0; i < NUM_ARMOR; ++i) {
		ItemInstance* item = getArmor(i);
		if (!ItemInstance::isArmorItem(item)) return;

		drop(new ItemInstance(*item), true);
		setArmor(i, NULL);
	}

	super::die(source);
}

void LocalPlayer::swing() {
    super::swing();

    if (swingTime == -1 && !minecraft->javaSession) {
        AnimatePacket packet(AnimatePacket::Swing, this);
        packet.reliability = UNRELIABLE;
        packet.priority = MEDIUM_PRIORITY;
        minecraft->raknetInstance->send(packet);
    }
}

void LocalPlayer::reset() {
	super::reset();
	this->_init();
}

void LocalPlayer::_init() {
	autoJumpTime		= 0;
	jumpTriggerTime		= 0;
	ascendTriggerTime	= 0;
	descendTriggerTime	= 0;
	ascending	= false;
	descending	= false;

	ItemInstance* item = inventory->getSelected();
	sentInventoryItemId = item? item->id : 0;
	sentInventoryItemData = item? item->getAuxValue() : 0;
}

void LocalPlayer::startCrafting(int x, int y, int z, int tableSize) {
    nativeCraftX=x;nativeCraftY=y;nativeCraftZ=z;
#ifndef STANDALONE_SERVER
	if (minecraft->javaSession) {
  auto* session=minecraft->javaSession;
  if(session->dead||session->dialog.pending)return;
  if(session->inventory.windowId==0||session->inventory.menuKind=="crafting")minecraft->setScreen(new JavaCraftingScreen(session->inventory.windowId));
  else showJavaInventory(minecraft,true);
  return;
 }
	if (!minecraft->isCreativeMode())
		minecraft->setScreen( new WorkbenchScreen(tableSize) );
#endif
}

void LocalPlayer::startStonecutting(int x, int y, int z) {
#ifndef STANDALONE_SERVER
	if (!minecraft->isCreativeMode())
		minecraft->setScreen( new StonecutterScreen() );
#endif
}

void LocalPlayer::openFurnace( FurnaceTileEntity* e ) {
#ifndef STANDALONE_SERVER
	if (!minecraft->isCreativeMode())
		minecraft->setScreen( new FurnaceScreen(this, e) );
#endif
}

void LocalPlayer::openContainer( ChestTileEntity* container ) {
#ifndef STANDALONE_SERVER
	// Allow chests in Creative.  This keeps the chest UI as the bridge for
	// players who want to fill a chest while Creative is enabled, then switch
	// back to Survival and use the stored contents normally.
	minecraft->setScreen( new ChestScreen(this, container) );
#endif
}

void LocalPlayer::drop( ItemInstance* item, bool randomly )
{
    if(nativeTransactionsEnabled()) {
        if(!item)return;
        for(int i=Inventory::MAX_SELECTION_SIZE;i<inventory->getContainerSize();++i) {
            ItemInstance* owned=inventory->getItem(i);
            if(owned && owned->id==item->id && owned->getAuxValue()==item->getAuxValue() && owned->count>=item->count) {
                nativeDropSlot(i,item->count,randomly);break;
            }
        }
        delete item;return;
    }
	if (!item)
		return;

	if (level->isClientSide) {
		DropItemPacket packet(entityId, *item);
		minecraft->raknetInstance->send(packet);
		// delete the ItemEntity here, since we don't add it to level
		delete item;
	} else {
		super::drop(item, randomly);
	}
}

void LocalPlayer::causeFallDamage( float distance )
{
	if (minecraft->javaSession) return; // Authoritative Java health.

	int dmg = (int) ceil((distance - 3));
	if (dmg > 0) {
		if (level->isClientSide) {
			SetHealthPacket packet(SetHealthPacket::HEALTH_MODIFY_OFFSET + dmg);
			minecraft->raknetInstance->send(packet);
		}
	}
	super::causeFallDamage(distance);

}

void LocalPlayer::displayClientMessage( const std::string& messageId ) {
#ifndef STANDALONE_SERVER
    if(nativeTransactionsEnabled())minecraft->gui.addMessage(messageId);
    else minecraft->gui.displayClientMessage(messageId);
#endif
}

int LocalPlayer::startSleepInBed( int x, int y, int z ) {
	int startSleepInBedReturnValue = super::startSleepInBed(x, y, z);
#ifndef STANDALONE_SERVER
	if(startSleepInBedReturnValue == BedSleepingResult::OK)
		minecraft->setScreen(new InBedScreen());
#endif
	return startSleepInBedReturnValue;
}

void LocalPlayer::stopSleepInBed( bool forcefulWakeUp, bool updateLevelList, bool saveRespawnPoint ) {
	if(level->isClientSide) {
		PlayerActionPacket packet(PlayerActionPacket::STOP_SLEEPING, 0, 0, 0, 0, entityId);
		minecraft->raknetInstance->send(packet);
	}
#ifndef STANDALONE_SERVER
	minecraft->setScreen(NULL);
#endif
	super::stopSleepInBed(forcefulWakeUp, updateLevelList, saveRespawnPoint);
}

void LocalPlayer::openTextEdit( TileEntity* tileEntity ) {
#if !defined(STANDALONE_SERVER) && !defined(RPI)
	if(tileEntity->type == TileEntityType::Sign)
		minecraft->setScreen(new TextEditScreen((SignTileEntity*) tileEntity));
#endif
}

void LocalPlayer::updateArmorTypeHash() {
	if (minecraft->javaSession || nativeTransactionsEnabled()) return;
    int hash = getArmorTypeHash();
    if (hash != armorTypeHash) {
        PlayerArmorEquipmentPacket p(this);
        minecraft->raknetInstance->send(p);
        armorTypeHash = hash;
    }
}
