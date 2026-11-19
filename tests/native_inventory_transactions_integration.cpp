// Independent integration regression for the negotiated native inventory protocol.
// Exercises actual server handlers; packets are serialized through a recording
// transport so test assertions inspect the same acknowledgement a client sees.
#include "client/Minecraft.h"
#include "client/gamemode/SurvivalMode.h"
#include "client/User.h"
#include "client/player/LocalPlayer.h"
#include "client/player/input/KeyboardInput.h"
#include "network/ClientSideNetworkHandler.h"
#include "server/ServerPlayer.h"
#include "network/ServerSideNetworkHandler.h"
#include "network/RakNetInstance.h"
#include "network/packet/SendInventoryPacket.h"
#include "network/packet/ContainerSetContentPacket.h"
#include "network/packet/LoginPacket.h"
#include "network/packet/RemoveBlockPacket.h"
#include "network/packet/TakeItemEntityPacket.h"
#include "network/packet/PlayerActionPacket.h"
#include "world/entity/projectile/Arrow.h"
#include "network/packet/NativeInventoryTransaction.h"
#include "client/player/NativeInventoryClientState.h"
#include <cstdlib>
#include "world/level/Level.h"
#include "world/level/chunk/ChunkSource.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/storage/ExternalFileLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/level/tile/entity/ChestTileEntity.h"
#include "world/level/tile/entity/FurnaceTileEntity.h"
#include "world/inventory/ContainerMenu.h"
#include "world/inventory/FurnaceMenu.h"
#include "world/entity/MobCategory.h"
#include "world/entity/item/ItemEntity.h"
#include "world/item/Item.h"
#include "world/item/crafting/Recipes.h"
#include "world/level/biome/Biome.h"
#include "util/Mth.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {
struct Game : Minecraft {};
struct RecordingWire : RakNetInstance {
    std::vector<ContainerSetContentPacket> snapshots;
    void send(const RakNet::RakNetGUID&, Packet& packet) override {
        auto* content = dynamic_cast<ContainerSetContentPacket*>(&packet);
        if (!content || content->containerId != 0) return;
        RakNet::BitStream bytes;
        content->write(&bytes);
        RakNet::MessageID id;
        assert(bytes.Read(id));
        ContainerSetContentPacket parsed;
        parsed.read(&bytes);
        assert(parsed.valid);
        snapshots.push_back(parsed);
    }
    void send(Packet&) override {}
    void send(Packet* packet) override { delete packet; }
    const ContainerSetContentPacket& latest() const { assert(!snapshots.empty()); return snapshots.back(); }
};
int count(Player* player, int id, int aux=0) {
    int result=0;
    for (int slot=Inventory::MAX_SELECTION_SIZE;slot<player->inventory->getContainerSize();++slot) {
        const ItemInstance* item=player->inventory->getItem(slot);
        if (item && item->id==id && item->getAuxValue()==aux) result+=item->count;
    }
    return result;
}
int recipeFor(int resultId, int resultCount, int aux=0) {
    const auto& recipes=Recipes::getInstance()->getRecipes();
    for (size_t i=0;i<recipes.size();++i) {
        const ItemInstance item=recipes[i]->getResultItem();
        if (item.id==resultId && item.count==resultCount && item.getAuxValue()==aux) return static_cast<int>(i);
    }
    assert(false && "fixture recipe missing");
    return -1;
}
struct Fixture {
    Game* game=new Game();
    RecordingWire wire;
    Level* level;
    ServerSideNetworkHandler* server;
    ServerPlayer* player;
    Fixture(LevelStorage* storage=nullptr) {
        level=new Level(storage?storage:new MemoryLevelStorage(),"transaction-fixture",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);
        level->setUpdateLights(false);
        game->level=level; game->raknetInstance=&wire; game->gameMode=new SurvivalMode(game);
        level->raknetInstance=&wire;
        server=new ServerSideNetworkHandler(game,&wire); server->levelGenerated(level);
        player=new ServerPlayer(game,level); player->owner=RakNet::RakNetGUID(123456);player->name="TransactionProbe";
        player->inventory->clearInventory(); level->addEntity(player);
        level->getChunkSource()->create(0,0);player->moveTo(1.5f,70.f,1.5f,0,0);
    }
    SendInventoryPacket request(int type,uint64_t sequence) {
        SendInventoryPacket packet(player,false);
        packet.action.present=true; packet.action.type=type;
        packet.action.nonce=player->inventoryNonce; packet.action.sequence=sequence;
        packet.action.baseRevision=player->inventoryRevision;
        return packet;
    }
    void send(const SendInventoryPacket& original,const RakNet::RakNetGUID& source) {
        RakNet::BitStream bytes; SendInventoryPacket outgoing=original; outgoing.write(&bytes);
        RakNet::MessageID id; assert(bytes.Read(id));
        SendInventoryPacket parsed; parsed.read(&bytes); assert(parsed.valid);
        server->handle(source,&parsed);
    }
    void send(const SendInventoryPacket& packet) { send(packet,player->owner); }
    void put(int slot,int id,int amount,int aux=0) { ItemInstance item(id,amount,aux);player->inventory->setItem(slot,&item); }
};
struct ClientWire : RakNetInstance {
    std::vector<SendInventoryPacket> requests;
    int legacyUploads=0;
    void send(Packet& packet) override {
        auto* inventory=dynamic_cast<SendInventoryPacket*>(&packet);
        if(!inventory)return;
        RakNet::BitStream bytes;inventory->write(&bytes);RakNet::MessageID id;assert(bytes.Read(id));
        SendInventoryPacket parsed;parsed.read(&bytes);assert(parsed.valid);
        if(!parsed.action.present)++legacyUploads;
        requests.push_back(parsed);
    }
};
void localPlayerRoundTrip() {
    using A=NativeInventoryAction;
    Fixture f;f.put(9,Tile::treeTrunk->id,3);f.player->sendInventory();
    auto* game=new Game();ClientWire wire;game->raknetInstance=&wire;game->user=new User("TransactionProbe","");
    auto* level=new Level(new MemoryLevelStorage(),"local-player-transaction",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);
    level->setUpdateLights(false);level->isClientSide=true;level->raknetInstance=&wire;game->level=level;game->gameMode=new SurvivalMode(game);
    auto* local=new LocalPlayer(game,level,game->user,0,false);game->player=local;local->entityId=f.player->entityId;
    local->input=new KeyboardInput(&game->options);
    ClientSideNetworkHandler handler(game,&wire);game->netCallback=&handler;handler.beginJoin();
    const RakNet::RakNetGUID source(888);handler.onConnect(source);handler.levelGenerated(level);
    auto receive=[&]() {auto snapshot=f.wire.latest();handler.handle(source,&snapshot);};
    receive();assert(local->nativeInventoryReady&&local->nativeTransactionsEnabled());
    assert(wire.requests.size()==1&&wire.requests[0].action.type==A::Hotbar&&wire.legacyUploads==0);
    f.send(wire.requests[0]);receive();assert(f.player->inventoryV1&&local->nativeTransactions.pending.empty());
    auto* arrow=new Arrow(level);level->addEntity(arrow);
    TakeItemEntityPacket oldPickup(arrow->entityId,local->entityId);
    handler.handle(source,&oldPickup);assert(count(local,Item::arrow->id)==0);
    const auto craftIndex=wire.requests.size();
    NativeInventoryAction action;action.type=A::Craft;action.recipe=recipeFor(Tile::wood->id,4);
    assert(local->nativeSubmitAction(action));
    assert(wire.requests.size()==craftIndex+1&&wire.requests[craftIndex].action.present&&wire.requests[craftIndex].action.nonce==f.player->inventoryNonce);
    assert(local->nativeTransactions.pending.size()==1&&count(local,Tile::wood->id)==4);
    ItemEntity loot(f.level,0,70,0,ItemInstance(Item::leather,5));loot.playerTouch(f.player);receive();
    assert(count(local,Item::leather->id)==5&&count(local,Tile::wood->id)==4&&local->nativeTransactions.pending.size()==1);
    f.send(wire.requests[craftIndex]);receive();
    assert(local->nativeTransactions.pending.empty()&&count(local,Item::leather->id)==5&&count(local,Tile::wood->id)==4);
    // Local tick may send selection/movement, but must never upload a whole
    // speculative inventory after negotiated authoritative snapshots.
    local->tick();assert(wire.legacyUploads==0);
    // A coherent container snapshot must update native storage and queued
    // predictions from the same inventory revision, without a local slot write.
    f.level->setTile(0,70,0,Tile::chest->id);
    auto* serverChest=dynamic_cast<ChestTileEntity*>(f.level->getTileEntity(0,70,0));assert(serverChest);
    ItemInstance wood(Tile::wood,6);serverChest->setItem(0,&wood);f.player->openContainer(serverChest);
    ChestTileEntity clientChest;ContainerMenu clientMenu(&clientChest);clientMenu.containerId=f.player->containerMenu->containerId;
    local->containerMenu=&clientMenu;f.player->sendInventory();receive();
    assert(clientChest.getItem(0)&&clientChest.getItem(0)->count==6);
    const auto sent=wire.requests.size();
    assert(local->nativeTransfer(clientMenu.containerId,true,0,-1,2));
    assert(wire.requests.size()==sent+1&&wire.requests.back().action.type==A::Transfer);
    assert(clientChest.getItem(0)->count==4&&count(local,Tile::wood->id)==6);
    f.send(wire.requests.back());receive();
    assert(local->nativeTransactions.pending.empty()&&serverChest->getItem(0)->count==4&&clientChest.getItem(0)->count==4);
    assert(count(local,Tile::wood->id)==6&&count(f.player,Tile::wood->id)==6&&wire.legacyUploads==0);
    local->containerMenu=nullptr;
    std::cout << "Actual LocalPlayer/ClientHandler/server serialized round trip: capability, semantic request, pickup-overlap replay, no legacy full upload, and coherent native chest prediction/ack PASS\n";
}
int countItems(const std::vector<ItemInstance>& items,int id) {
    int total=0;for(const auto& item:items)if(item.id==id)total+=item.count;return total;
}
void clientServerOverlap() {
    using A=NativeInventoryAction;
    Fixture f;f.put(9,Tile::treeTrunk->id,3);f.player->sendInventory();
    NativeInventoryClientState client;
    const auto first=f.wire.latest();
    assert(client.accept(first.items,first.hotbar,first.nativeState));
    auto receive=[&]() {const auto& snapshot=f.wire.latest();return client.accept(snapshot.items,snapshot.hotbar,snapshot.nativeState);};
    auto submit=[&](NativeInventoryAction action) {
        assert(client.enqueue(action,client.predictedHotbar));
        auto request=f.request(action.type,action.sequence);request.action=action;return request;
    };
    NativeInventoryAction action;action.type=A::Craft;action.recipe=recipeFor(Tile::wood->id,4);
    auto one=submit(action);
    assert(client.pending.size()==1&&countItems(client.items,Tile::wood->id)==4);
    ItemEntity pickup(f.level,0,70,0,ItemInstance(Item::leather,5));pickup.playerTouch(f.player);
    assert(receive());
    assert(client.pending.size()==1&&countItems(client.items,Item::leather->id)==5&&countItems(client.items,Tile::wood->id)==4);
    f.send(one);assert(receive());
    assert(client.pending.empty()&&countItems(client.items,Tile::wood->id)==4&&countItems(client.items,Item::leather->id)==5);
    assert(!client.accept(first.items,first.hotbar,first.nativeState));
    auto two=submit(action),three=submit(action);
    assert(client.pending.size()==2&&countItems(client.items,Tile::wood->id)==12);
    f.send(three);assert(receive()); // Explicitly reorder delivery, keep pending intents.
    assert(client.pending.size()==2&&countItems(client.items,Tile::wood->id)==12);
    f.send(two);assert(receive());assert(client.pending.size()==1);
    f.send(three);assert(receive());
    assert(client.pending.empty()&&countItems(client.items,Tile::wood->id)==12&&countItems(client.items,Item::leather->id)==5);
    auto rejected=submit(action);f.send(rejected);assert(receive());
    assert(client.pending.empty()&&client.lastStatus==A::Insufficient&&countItems(client.items,Tile::wood->id)==12);
    auto foreign=f.wire.latest();foreign.nativeState.nonce^=0x1234;foreign.nativeState.revision+=100;
    assert(!client.accept(foreign.items,foreign.hotbar,foreign.nativeState));
    auto impossible=f.wire.latest();impossible.nativeState.acknowledged=client.nextSequence+10;
    assert(!client.accept(impossible.items,impossible.hotbar,impossible.nativeState));
    NativeInventoryClientState oldServerClient;
    auto legacy=f.wire.latest();legacy.nativeState.present=false;
    assert(!oldServerClient.accept(legacy.items,legacy.hotbar,legacy.nativeState)&&!oldServerClient.enabled());
    std::cout << "Actual client semantic queue + server round trip: pickup while craft pending, rapid/reordered requests, ack replay, rejection rollback, old revision/foreign nonce/impossible ack rejection and old-server fallback PASS\n";
}
void reconnectCommittedAction() {
    using A=NativeInventoryAction;
    char directory[]="/tmp/tcpe-inventory-reconnect-XXXXXX";
    assert(mkdtemp(directory));
    auto* storage=new ExternalFileLevelStorage("transaction-reconnect",directory);
    Fixture f(storage);
    f.put(9,Tile::treeTrunk->id,3);f.player->sendInventory();
    const uint64_t previousNonce=f.player->inventoryNonce;
    auto craft=f.request(A::Craft,1);craft.action.recipe=recipeFor(Tile::wood->id,4);
    f.send(craft);
    assert(count(f.player,Tile::wood->id)==4&&count(f.player,Tile::treeTrunk->id)==2);
    // The server committed but its acknowledgement never reached the client.
    // Disconnect is the real persistence path, not a test-only inventory copy.
    f.server->onDisconnect(f.player->owner);
    auto* returning=new ServerPlayer(f.game,f.level);
    returning->owner=RakNet::RakNetGUID(777);returning->name="TransactionProbe";
    assert(storage->loadNetworkPlayer(returning)==1);
    assert(returning->inventoryNonce&&returning->inventoryNonce!=previousNonce);
    assert(returning->inventoryAcknowledged==0&&!returning->inventoryV1);
    assert(count(returning,Tile::wood->id)==4&&count(returning,Tile::treeTrunk->id)==2);
    f.level->addEntity(returning);f.player=returning;
    craft.entityId=returning->entityId; // Even correct new owner cannot reuse old-session nonce.
    f.send(craft);
    assert(returning->inventoryAcknowledged==0&&count(returning,Tile::wood->id)==4);
    assert(f.wire.latest().nativeState.status==A::WrongSession);
    auto fresh=f.request(A::Craft,1);fresh.action.recipe=recipeFor(Tile::wood->id,4);
    f.send(fresh);
    assert(returning->inventoryAcknowledged==1&&count(returning,Tile::wood->id)==8&&count(returning,Tile::treeTrunk->id)==1);
    std::cout << "Actual disconnect save/reload: commit-before-ack survives reconnect, old nonce cannot replay, new session starts clean PASS\n";
}
void legacyFallbackAndParsing() {
    using A=NativeInventoryAction;
    Fixture f;
    f.put(9,Tile::wood->id,10);f.player->sendInventory();
    assert(f.wire.latest().nativeState.present&&!f.player->inventoryV1);
    SendInventoryPacket legacy(f.player,false);legacy.items[0].count=9;
    f.send(legacy);
    assert(!f.player->inventoryV1&&count(f.player,Tile::wood->id)==9);
    // A new parser must still accept both a hotbar-only old-server snapshot and
    // the original snapshot with no optional hotbar extension at all.
    ContainerSetContentPacket oldServer(0,std::vector<ItemInstance>(36));
    oldServer.hotbar.capture(f.player->inventory);
    RakNet::BitStream bytes;oldServer.write(&bytes);
    RakNet::MessageID id;assert(bytes.Read(id));
    ContainerSetContentPacket parsed;parsed.read(&bytes);
    assert(parsed.valid&&parsed.hotbar.present&&!parsed.nativeState.present);
    oldServer.hotbar.present=false;RakNet::BitStream older;oldServer.write(&older);assert(older.Read(id));parsed.read(&older);
    assert(parsed.valid&&!parsed.hotbar.present&&!parsed.nativeState.present);
    auto modern=f.request(A::Craft,1);modern.action.recipe=recipeFor(Item::stick->id,4);
    RakNet::BitStream complete;modern.write(&complete);
    auto stripped=modern;stripped.action.present=false;RakNet::BitStream prefix;stripped.write(&prefix);
    for(unsigned int length=prefix.GetNumberOfBytesUsed()+1;length<complete.GetNumberOfBytesUsed();++length) {
        RakNet::BitStream shortPacket(complete.GetData()+1,length-1,false);
        SendInventoryPacket truncated;truncated.read(&shortPacket);
        assert(!truncated.valid);f.server->handle(f.player->owner,&truncated);
        assert(count(f.player,Tile::wood->id)==9&&f.player->inventoryAcknowledged==0&&!f.player->inventoryV1);
    }
    f.send(modern);
    assert(f.player->inventoryV1&&count(f.player,Tile::wood->id)==7&&count(f.player,Item::stick->id)==4);
    std::cout << "Legacy old-client snapshot fallback, old-server snapshot parse, truncated negotiated trailer rejection and activation PASS\n";
}
void storageConservation() {
    using A=NativeInventoryAction;
    Fixture chest;
    chest.put(9,Tile::wood->id,10);
    chest.level->setTile(0,70,0,Tile::chest->id);
    auto* box=dynamic_cast<ChestTileEntity*>(chest.level->getTileEntity(0,70,0));assert(box);
    chest.player->openContainer(box);
    const int window=chest.player->containerMenu->containerId;
    auto deposit=chest.request(A::Transfer,1);
    deposit.action.slot=9;deposit.action.slot2=0;deposit.action.count=4;deposit.action.x=window;
    deposit.action.expectedId=Tile::wood->id;
    chest.send(deposit);
    assert(count(chest.player,Tile::wood->id)==6&&box->getItem(0)&&box->getItem(0)->count==4);
    assert(chest.wire.latest().nativeState.containerId==window);
    assert(chest.wire.latest().nativeState.containerItems[0].count==4);
    chest.send(deposit);
    assert(count(chest.player,Tile::wood->id)==6&&box->getItem(0)->count==4);
    auto withdrawal=chest.request(A::Transfer,2);
    withdrawal.action.flags=1;withdrawal.action.slot=0;withdrawal.action.slot2=9;
    withdrawal.action.count=2;withdrawal.action.x=window;withdrawal.action.expectedId=Tile::wood->id;
    ItemEntity loot(chest.level,0,70,0,ItemInstance(Item::leather,5));loot.playerTouch(chest.player);
    chest.send(withdrawal);
    assert(count(chest.player,Tile::wood->id)==8&&box->getItem(0)->count==2);
    assert(count(chest.player,Item::leather->id)==5);
    auto delayed=chest.request(A::Transfer,3);delayed.action=withdrawal.action;delayed.action.sequence=3;
    chest.player->doCloseContainer();
    chest.level->setTile(2,70,0,Tile::chest->id);
    auto* another=dynamic_cast<ChestTileEntity*>(chest.level->getTileEntity(2,70,0));assert(another);chest.player->openContainer(another);
    chest.send(delayed);
    assert(chest.wire.latest().nativeState.status==A::InvalidContext);
    assert(count(chest.player,Tile::wood->id)==8&&box->getItem(0)->count==2&&!another->getItem(0));

    Fixture furnace;
    furnace.level->setTile(0,70,0,Tile::furnace->id);
    auto* oven=dynamic_cast<FurnaceTileEntity*>(furnace.level->getTileEntity(0,70,0));assert(oven);
    ItemInstance output(Item::goldIngot,3);oven->setItem(2,&output);
    furnace.player->openFurnace(oven);
    auto take=furnace.request(A::Transfer,1);
    take.action.flags=1;take.action.slot=2;take.action.slot2=-1;take.action.count=2;
    take.action.x=furnace.player->containerMenu->containerId;take.action.expectedId=Item::goldIngot->id;
    furnace.send(take);
    assert(count(furnace.player,Item::goldIngot->id)==2&&oven->getItem(2)->count==1);
    furnace.send(take);
    assert(count(furnace.player,Item::goldIngot->id)==2&&oven->getItem(2)->count==1);
    auto full=furnace.request(A::Transfer,2);full.action=take.action;full.action.sequence=2;full.action.count=1;
    for(int slot=9;slot<45;++slot)furnace.put(slot,1,64);
    furnace.send(full);
    assert(furnace.wire.latest().nativeState.status==A::NoSpace&&oven->getItem(2)->count==1);
    assert(furnace.player->inventoryAcknowledged==2);
    std::cout << "Actual chest/furnace transfers: coherent snapshots, exact conservation, pickup overlap, replay, stale window rejection and full-inventory rollback PASS\n";
}
int worldCount(Level* level,int id) {
    int total=0;
    for(auto* entity:level->entities)if(entity&&!entity->removed&&entity->isItemEntity()) {
        auto* item=static_cast<ItemEntity*>(entity);if(item->item.id==id)total+=item->item.count;
    }
    return total;
}
void shiftedCraftOutputActions() {
    using A=NativeInventoryAction;
    Fixture f;f.put(9,Tile::treeTrunk->id,3);f.player->sendInventory();
    auto craft=f.request(A::Craft,1);craft.action.recipe=recipeFor(Tile::wood->id,4);
    auto link=f.request(A::Hotbar,2);
    link.items[0]=ItemInstance(Tile::treeTrunk,2);link.items[1]=ItemInstance(Tile::wood,4);
    for(auto& slot:link.hotbar.slots)slot=-1;link.hotbar.slots[0]=10;link.hotbar.selected=0;
    auto use=f.request(A::Use,3);use.action.slot=10;use.action.expectedId=Tile::wood->id;
    use.action.x=3;use.action.y=70;use.action.z=3;use.action.face=1;
    auto drop=f.request(A::Drop,4);drop.action.slot=10;drop.action.count=1;drop.action.expectedId=Tile::wood->id;
    ItemEntity pickup(f.level,0,70,0,ItemInstance(Item::leather,5));pickup.playerTouch(f.player);
    assert(f.player->inventory->getItem(10)->id==Item::leather->id);
    f.send(craft);f.send(link);
    assert(f.player->inventory->getSelected()&&f.player->inventory->getSelected()->id==Tile::wood->id);
    assert(f.player->inventory->linkedSlots[0].inventorySlot==11);
    f.level->setTile(3,70,3,Tile::rock->id);f.level->setTile(3,71,3,0);
    f.send(use);f.send(drop);
    assert(count(f.player,Tile::wood->id)==2&&count(f.player,Item::leather->id)==5);
    assert(f.level->getTile(3,71,3)==Tile::wood->id&&worldCount(f.level,Tile::wood->id)==1);
    Fixture mine;mine.put(9,Item::pickAxe_wood->id,1);mine.player->inventory->linkSlot(0,9,false);
    mine.level->setTile(3,70,3,Tile::rock->id);mine.level->setTile(4,70,3,Tile::rock->id);
    auto first=mine.request(A::Break,1);first.action.slot=9;first.action.expectedId=Item::pickAxe_wood->id;
    first.action.expectedAux=0;first.action.x=3;first.action.y=70;first.action.z=3;
    auto second=first;second.action.sequence=2;second.action.x=4;
    mine.send(first);mine.send(second);
    assert(mine.wire.latest().nativeState.status==A::Accepted&&mine.player->inventory->getItem(9)->getAuxValue()==2);
    assert(mine.level->getTile(3,70,3)==0&&mine.level->getTile(4,70,3)==0&&worldCount(mine.level,Tile::stoneBrick->id)==2);
    std::cout << "Pickup-shifted predicted crafting output retains queued hotbar/place/drop intent; rapid mining uses current tool wear PASS\n";
}
void foodAndBowAuthority() {
    using A=NativeInventoryAction;
    Fixture food;food.put(9,Item::apple->id,2);food.player->inventory->linkSlot(0,9,false);food.player->health=10;
    for(int x=0;x<16;++x)for(int z=0;z<16;++z)food.level->setTileNoUpdate(x,69,z,Tile::rock->id);
    auto eat=food.request(A::Use,1);eat.action.face=255;eat.action.slot=9;eat.action.expectedId=Item::apple->id;
    food.send(eat);assert(food.player->isUsingItem());
    ItemEntity apples(food.level,0,70,0,ItemInstance(Item::apple,2));apples.playerTouch(food.player);
    assert(count(food.player,Item::apple->id)==4);
    for(int i=0;i<32;++i)food.player->tick();
    assert(!food.player->isUsingItem()&&count(food.player,Item::apple->id)==3);
    food.player->completeUsingItem();assert(count(food.player,Item::apple->id)==3);

    Fixture bow;bow.put(9,Item::bow->id,1);bow.player->inventory->linkSlot(0,9,false);
    for(int x=0;x<16;++x)for(int z=0;z<16;++z)bow.level->setTileNoUpdate(x,69,z,Tile::rock->id);
    auto draw=bow.request(A::Use,1);draw.action.face=255;draw.action.slot=9;draw.action.expectedId=Item::bow->id;draw.action.z=32768;
    bow.send(draw);assert(!bow.player->isUsingItem()&&bow.player->inventory->getItem(9)->getAuxValue()==0);
    bow.put(10,Item::arrow->id,2);
    draw.action.sequence=2;bow.send(draw);assert(bow.player->isUsingItem());
    for(int i=0;i<20;++i)bow.player->tick();
    PlayerActionPacket release(PlayerActionPacket::RELEASE_USE_ITEM,0,0,0,0,bow.player->entityId);
    bow.server->handle(bow.player->owner,&release);
    assert(!bow.player->isUsingItem()&&count(bow.player,Item::arrow->id)==1&&bow.player->inventory->getItem(9)->getAuxValue()==1);
    int arrows=0;for(auto* entity:bow.level->entities)if(dynamic_cast<Arrow*>(entity))++arrows;
    assert(arrows==1);
    bow.server->handle(bow.player->owner,&release);
    int afterReplay=0;for(auto* entity:bow.level->entities)if(dynamic_cast<Arrow*>(entity))++afterReplay;
    assert(afterReplay==1&&count(bow.player,Item::arrow->id)==1&&bow.player->inventory->getItem(9)->getAuxValue()==1);
    std::cout << "Actual food timer consumes once after same-stack pickup; bow requires arrows and release/replay consumes one arrow plus one wear PASS\n";
}
void remainingActionVariants() {
    using A=NativeInventoryAction;
    Fixture hotbar;hotbar.put(9,Tile::wood->id,5);
    auto link=hotbar.request(A::Hotbar,1);link.hotbar.present=true;link.hotbar.selected=3;
    for(auto& slot:link.hotbar.slots)slot=-1;link.hotbar.slots[3]=9;
    hotbar.send(link);assert(hotbar.player->inventory->selected==3&&hotbar.player->inventory->getSelected()->count==5);
    auto invalid=hotbar.request(A::Hotbar,2);invalid.hotbar.selected=9;
    // Direct handler receives invalid semantic fields only after packet validity.
    hotbar.server->handle(hotbar.player->owner,&invalid);
    assert(hotbar.player->inventoryAcknowledged==2&&hotbar.player->inventory->selected==3&&count(hotbar.player,Tile::wood->id)==5);

    Fixture drop;drop.put(9,Item::leather->id,5);
    auto toss=drop.request(A::Drop,1);toss.action.slot=9;toss.action.count=2;toss.action.expectedId=Item::leather->id;
    drop.send(toss);drop.send(toss);
    assert(count(drop.player,Item::leather->id)==3&&worldCount(drop.level,Item::leather->id)==2);
    auto excess=drop.request(A::Drop,2);excess.action=toss.action;excess.action.sequence=2;excess.action.count=4;
    drop.send(excess);assert(drop.wire.latest().nativeState.status==A::Insufficient);
    assert(count(drop.player,Item::leather->id)==3&&worldCount(drop.level,Item::leather->id)==2);

    Fixture armor;armor.put(9,Item::helmet_iron->id,1);
    auto equip=armor.request(A::Armor,1);equip.action.slot=9;equip.action.slot2=0;
    armor.send(equip);armor.send(equip);
    assert(count(armor.player,Item::helmet_iron->id)==0&&armor.player->getArmor(0)->id==Item::helmet_iron->id);
    auto unequip=armor.request(A::Armor,2);unequip.action.flags=1;unequip.action.slot=0;unequip.action.slot2=10;
    armor.send(unequip);assert(count(armor.player,Item::helmet_iron->id)==1&&!armor.player->getArmor(0));
    armor.put(11,Item::stick->id,1);
    auto wrongArmor=armor.request(A::Armor,3);wrongArmor.action.slot=11;wrongArmor.action.slot2=0;
    armor.send(wrongArmor);assert(armor.wire.latest().nativeState.status==A::BadAction);
    assert(count(armor.player,Item::helmet_iron->id)==1&&count(armor.player,Item::stick->id)==1);

    Fixture place;place.put(9,Tile::wood->id,3);place.player->inventory->linkSlot(0,9,false);
    place.level->setTile(3,70,3,Tile::rock->id);place.level->setTile(3,71,3,0);place.level->setTile(3,72,3,0);
    auto use=place.request(A::Use,1);use.action.slot=9;use.action.expectedId=Tile::wood->id;
    use.action.x=3;use.action.y=70;use.action.z=3;use.action.face=1;
    place.send(use);place.send(use);
    assert(place.wire.latest().nativeState.status==A::Accepted&&place.level->getTile(3,71,3)==Tile::wood->id);
    assert(count(place.player,Tile::wood->id)==2&&place.level->getTile(3,72,3)==0);

    Fixture mine;mine.put(9,Item::pickAxe_wood->id,1);mine.player->inventory->linkSlot(0,9,false);
    mine.level->setTile(3,70,3,Tile::rock->id);
    auto breaking=mine.request(A::Break,1);breaking.action.slot=9;breaking.action.expectedId=Item::pickAxe_wood->id;
    breaking.action.x=3;breaking.action.y=70;breaking.action.z=3;
    mine.send(breaking);mine.send(breaking);
    assert(mine.level->getTile(3,70,3)==0&&mine.player->inventory->getItem(9)->getAuxValue()==1);
    assert(worldCount(mine.level,Tile::stoneBrick->id)==1);
    mine.level->setTile(4,70,3,Tile::rock->id);RemoveBlockPacket oldBreak(mine.player,4,70,3);
    mine.server->handle(mine.player->owner,&oldBreak);
    assert(mine.level->getTile(4,70,3)==Tile::rock->id&&mine.player->inventory->getItem(9)->getAuxValue()==1);

    Fixture death;death.put(9,Item::leather->id,5);ItemInstance helmet(Item::helmet_iron,1);death.player->setArmor(0,&helmet);
    auto aliveDeath=death.request(A::Death,1);death.send(aliveDeath);
    assert(death.wire.latest().nativeState.status==A::InvalidContext&&count(death.player,Item::leather->id)==5);
    death.player->health=0;death.player->die(nullptr);
    assert(count(death.player,Item::leather->id)==0&&worldCount(death.level,Item::leather->id)==5&&worldCount(death.level,Item::helmet_iron->id)==1);
    auto finish=death.request(A::Death,2);death.send(finish);death.send(finish);
    assert(worldCount(death.level,Item::leather->id)==5&&worldCount(death.level,Item::helmet_iron->id)==1);
    Fixture keep;keep.put(9,Item::leather->id,5);keep.level->getLevelData()->setKeepInventory(true);
    auto enable=keep.request(A::Hotbar,1);keep.send(enable);keep.player->health=0;keep.player->die(nullptr);
    assert(count(keep.player,Item::leather->id)==5&&worldCount(keep.level,Item::leather->id)==0);
    std::cout << "Actual Hotbar/Drop/Armor/Use/Break/Death: validity, quantity and durability conservation, replay/downgrade guards and keepInventory PASS\n";
}
void initialize() {
    Mth::initMth(); Material::initMaterials(); MobCategory::initMobCategories();
    Tile::initTiles(); Item::initItems(); Biome::initBiomes();
}
void survivalOnlyCapability() {
    Fixture survival;
    survival.player->sendInventory();
    assert(survival.player->inventoryTransactionsSupported&&survival.wire.latest().nativeState.present);
    auto* game=new Game();game->level=survival.level;game->raknetInstance=&survival.wire;
    game->setIsCreativeMode(true);
    auto* palette=new ServerPlayer(game,survival.level);
    palette->owner=RakNet::RakNetGUID(7777);palette->name="CreativePalette";
    survival.level->addEntity(palette);
    assert(!palette->inventoryTransactionsSupported);
    assert(palette->inventory->getContainerSize()==Inventory::CREATIVE_SLOT_COUNT+Inventory::MAX_SELECTION_SIZE);
    palette->sendInventory();
    assert(!survival.wire.latest().nativeState.present);
    SendInventoryPacket before(palette,false);
    ServerSideNetworkHandler server(game,&survival.wire);server.levelGenerated(survival.level);
    auto request=before;request.action.present=true;request.action.type=NativeInventoryAction::Drop;
    request.action.nonce=palette->inventoryNonce;request.action.sequence=1;
    request.action.slot=Inventory::MAX_SELECTION_SIZE;request.action.count=1;
    server.handle(palette->owner,&request);
    SendInventoryPacket after(palette,false);
    assert(!palette->inventoryV1&&palette->inventoryAcknowledged==0&&before.items==after.items);
    std::cout<<"Survival advertises transaction v1; Creative LAN unlimited palette keeps legacy semantics PASS\n";
}
} // namespace

int main() {
    initialize();
    using A=NativeInventoryAction;
    Fixture f;
    f.put(9,Tile::treeTrunk->id,3);
    f.player->inventory->linkSlot(0,9,false);
    f.player->sendInventory();
    assert(f.wire.latest().nativeState.present&&f.wire.latest().nativeState.nonce!=0);
    assert(!f.player->inventoryV1); // Advertisement does not break old clients.
    const int planksRecipe=recipeFor(Tile::wood->id,4);
    auto craft=f.request(A::Craft,1);craft.action.recipe=planksRecipe;
    const uint64_t beforePickup=f.player->inventoryRevision;
    ItemEntity pickup(f.level,0,70,0,ItemInstance(Item::leather,5));
    pickup.playerTouch(f.player);
    assert(pickup.removed&&count(f.player,Item::leather->id)==5);
    assert(f.player->inventoryRevision>beforePickup);
    // Deliver the action constructed before the pickup. Its obsolete packet
    // inventory must not overwrite the five newly collected items.
    f.send(craft);
    assert(f.player->inventoryV1&&count(f.player,Tile::treeTrunk->id)==2);
    assert(count(f.player,Tile::wood->id)==4&&count(f.player,Item::leather->id)==5);
    assert(f.wire.latest().nativeState.acknowledged==1&&f.wire.latest().nativeState.status==A::Accepted);
    const uint64_t afterCommit=f.player->inventoryRevision;
    f.send(craft); // Ack can be lost; retransmission must not craft again.
    assert(count(f.player,Tile::wood->id)==4&&f.player->inventoryRevision>=afterCommit);
    auto third=f.request(A::Craft,3);third.action.recipe=planksRecipe;
    f.send(third); // Gap must not consume sequence 2 or mutate inventory.
    assert(f.wire.latest().nativeState.acknowledged==1&&f.wire.latest().nativeState.status==A::SequenceGap);
    assert(count(f.player,Tile::wood->id)==4);
    auto second=f.request(A::Craft,2);second.action.recipe=planksRecipe;
    second.action.baseRevision=beforePickup;
    f.send(second);f.send(third); // Rapid queued actions retain old base revisions.
    assert(count(f.player,Tile::treeTrunk->id)==0&&count(f.player,Tile::wood->id)==12);
    assert(count(f.player,Item::leather->id)==5&&f.player->inventoryAcknowledged==3);
    auto insufficient=f.request(A::Craft,4);insufficient.action.recipe=planksRecipe;
    f.send(insufficient);
    assert(f.wire.latest().nativeState.acknowledged==4&&f.wire.latest().nativeState.status==A::Insufficient);
    assert(count(f.player,Tile::wood->id)==12&&count(f.player,Item::leather->id)==5);
    auto stick=f.request(A::Craft,5);stick.action.recipe=recipeFor(Item::stick->id,4);
    auto wrong=stick;wrong.action.nonce^=0x1234;f.send(wrong);
    assert(f.player->inventoryAcknowledged==4&&count(f.player,Item::stick->id)==0);
    const auto snapshots=f.wire.snapshots.size();
    f.send(stick,RakNet::RakNetGUID(9999));assert(f.wire.snapshots.size()==snapshots&&f.player->inventoryAcknowledged==4);
    f.send(stick);
    assert(f.player->inventoryAcknowledged==5&&count(f.player,Item::stick->id)==4&&count(f.player,Tile::wood->id)==10);
    // A negotiated client cannot fall back to arbitrary whole-inventory writes.
    SendInventoryPacket stale(f.player,false);stale.items.assign(stale.items.size(),ItemInstance());
    f.send(stale);
    assert(count(f.player,Tile::wood->id)==10&&count(f.player,Item::leather->id)==5&&count(f.player,Item::stick->id)==4);
    localPlayerRoundTrip();
    shiftedCraftOutputActions();
    foodAndBowAuthority();
    remainingActionVariants();
    clientServerOverlap();
    storageConservation();
    reconnectCommittedAction();
    legacyFallbackAndParsing();
    survivalOnlyCapability();
    std::cout << "Actual negotiated handler: pickup during pending craft, rapid crafts, stale-base semantic merge, dedup, sequence gap, rejection ack, foreign/wrong nonce and snapshot downgrade rejection PASS\n";
}
