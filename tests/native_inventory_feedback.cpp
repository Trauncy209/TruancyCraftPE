#include "client/Minecraft.h"
#include "client/User.h"
#include "client/player/LocalPlayer.h"
#include "client/player/input/KeyboardInput.h"
#include "client/gamemode/SurvivalMode.h"
#include "server/ServerPlayer.h"
#include "network/ServerSideNetworkHandler.h"
#include "network/ClientSideNetworkHandler.h"
#include "network/RakNetInstance.h"
#include "raknet/RakPeerInterface.h"
#include "network/packet/SendInventoryPacket.h"
#include "network/packet/ContainerSetContentPacket.h"
#include "network/packet/EntityEventPacket.h"
#include "world/entity/EntityEvent.h"
#include "world/level/Level.h"
#include "world/level/LevelListener.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/entity/MobCategory.h"
#include "world/entity/item/ItemEntity.h"
#include "world/item/Item.h"
#include "world/item/crafting/Recipes.h"
#include "world/level/biome/Biome.h"
#include "util/Mth.h"
#include <cassert>
#include <iostream>
#include <vector>

struct Game : Minecraft {};
struct ServerWire : RakNetInstance {
    std::vector<ContainerSetContentPacket> snapshots;
    std::vector<EntityEventPacket> events;
    std::vector<char> order;
    void send(const RakNet::RakNetGUID&,Packet& packet) override {
        RakNet::BitStream bytes;packet.write(&bytes);RakNet::MessageID id;assert(bytes.Read(id));
        if(dynamic_cast<ContainerSetContentPacket*>(&packet)) {
            ContainerSetContentPacket decoded;decoded.read(&bytes);
            if(decoded.containerId==0){assert(decoded.valid);snapshots.push_back(decoded);order.push_back('i');}
        }
        if(dynamic_cast<EntityEventPacket*>(&packet)) {EntityEventPacket decoded;decoded.read(&bytes);events.push_back(decoded);order.push_back('e');}
    }
    void send(Packet&) override {}
    void send(Packet* packet) override {delete packet;}
};
struct ClientWire : RakNetInstance {
    std::vector<SendInventoryPacket> requests;
    void send(Packet& packet) override {
        if(auto* inventory=dynamic_cast<SendInventoryPacket*>(&packet)) {
            RakNet::BitStream bytes;inventory->write(&bytes);RakNet::MessageID id;assert(bytes.Read(id));
            SendInventoryPacket decoded;decoded.read(&bytes);assert(decoded.valid);requests.push_back(decoded);
        }
    }
};
struct Local : LocalPlayer {
    using LocalPlayer::LocalPlayer;
    std::vector<std::string> notices;
    void displayClientMessage(const std::string& message) override {notices.push_back(message);}
};
struct Effects : LevelListener {
    int particles=0,burps=0;
    void addParticle(ParticleType::Id,float,float,float,float,float,float,int) override {++particles;}
    void addParticle(const std::string& name,float,float,float,float,float,float,int) override {if(name=="iconcrack")++particles;}
    void playSound(const std::string& name,float,float,float,float,float) override {if(name=="random.burp")++burps;}
};
struct Fixture {
    Game *serverGame=new Game(),*clientGame=new Game();
    ServerWire serverWire;ClientWire clientWire;
    Level *serverLevel,*clientLevel;
    ServerSideNetworkHandler* server;ClientSideNetworkHandler* client;
    ServerPlayer* remote;Local* local;Effects effects;
    const RakNet::RakNetGUID source=RakNet::RakNetGUID(888);
    Fixture() {
        serverLevel=new Level(new MemoryLevelStorage(),"feedback-server",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);
        serverLevel->setUpdateLights(false);serverLevel->raknetInstance=&serverWire;
        serverGame->level=serverLevel;serverGame->raknetInstance=&serverWire;serverGame->gameMode=new SurvivalMode(serverGame);
        server=new ServerSideNetworkHandler(serverGame,&serverWire);server->levelGenerated(serverLevel);
        remote=new ServerPlayer(serverGame,serverLevel);remote->owner=RakNet::RakNetGUID(12345);remote->inventory->clearInventoryEmpty();
        remote->moveTo(8,70,8,0,0);serverLevel->addEntity(remote);
        for(int x=0;x<16;++x)for(int z=0;z<16;++z)serverLevel->setTileNoUpdate(x,69,z,Tile::rock->id);
        clientLevel=new Level(new MemoryLevelStorage(),"feedback-client",LevelSettings(1234,GameType::Survival,LGV_INFINITE),LGV_INFINITE);
        clientLevel->setUpdateLights(false);clientLevel->isClientSide=true;clientLevel->raknetInstance=&clientWire;
        clientGame->level=clientLevel;clientGame->raknetInstance=&clientWire;clientGame->gameMode=new SurvivalMode(clientGame);
        clientGame->user=new User("FeedbackProbe","");
        local=new Local(clientGame,clientLevel,clientGame->user,0,false);clientGame->player=local;local->entityId=remote->entityId;
        local->owner=clientWire.getPeer()->GetMyGUID();local->input=new KeyboardInput(&clientGame->options);
        local->moveTo(8,70,8,0,0);clientLevel->addEntity(local);clientLevel->addListener(&effects);
        client=new ClientSideNetworkHandler(clientGame,&clientWire);clientGame->netCallback=client;
        client->beginJoin();client->onConnect(source);client->levelGenerated(clientLevel);
    }
    void put(int slot,int id,int count,int aux=0) {ItemInstance item(id,count,aux);remote->inventory->setItem(slot,&item);}
    void receive() {assert(!serverWire.snapshots.empty());auto packet=serverWire.snapshots.back();client->handle(source,&packet);}
    void commitLast() {assert(!clientWire.requests.empty());auto packet=clientWire.requests.back();server->handle(remote->owner,&packet);receive();}
    void negotiate() {remote->sendInventory();receive();commitLast();assert(remote->inventoryV1&&local->nativeTransactions.pending.empty());}
};
int recipeFor(int id) {
    const auto& recipes=Recipes::getInstance()->getRecipes();
    for(size_t i=0;i<recipes.size();++i)if(recipes[i]->getResultItem().id==id)return i;
    assert(false);return -1;
}
int main() {
    Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
    using A=NativeInventoryAction;
    Fixture feedback;feedback.negotiate();
    A craft;craft.type=A::Craft;craft.recipe=recipeFor(Tile::wood->id);
    assert(feedback.local->nativeSubmitAction(craft));feedback.commitLast();
    assert(feedback.local->notices.size()==1&&feedback.local->notices.back()=="Not enough items.");
    feedback.receive();feedback.remote->sendInventory();feedback.receive();assert(feedback.local->notices.size()==1);
    A transfer;transfer.type=A::Transfer;transfer.x=77;transfer.flags=1;transfer.slot=0;transfer.count=1;
    assert(feedback.local->nativeSubmitAction(transfer));feedback.commitLast();
    assert(feedback.local->notices.size()==2&&feedback.local->notices.back()=="That item or container changed. Try again.");
    for(int slot=9;slot<45;++slot)feedback.put(slot,Tile::rock->id,64);
    feedback.put(9,Tile::treeTrunk->id,2);feedback.remote->sendInventory();feedback.receive();
    assert(feedback.local->nativeSubmitAction(craft));feedback.commitLast();
    assert(feedback.local->notices.size()==3&&feedback.local->notices.back()=="Inventory is full.");
    feedback.remote->sendInventory();feedback.receive();feedback.receive();assert(feedback.local->notices.size()==3);
    std::cout<<"Actual rejected craft/transfer acknowledgments show one native notice; replay/async snapshots do not repeat PASS\n";

    Fixture food;food.put(9,Item::apple->id,2);food.put(10,Item::arrow->id,3);food.put(11,Item::bow->id,1,7);
    food.remote->inventory->linkSlot(0,9,false);food.remote->health=10;food.local->health=10;food.negotiate();
    assert(food.clientGame->gameMode->useItem(food.local,food.clientLevel,food.local->inventory->getSelected()));
    food.commitLast();assert(food.local->isUsingItem()&&food.remote->isUsingItem());
    for(int tick=0;tick<8;++tick){food.local->tick();food.remote->tick();}
    const int remaining=food.local->getUseItemDuration();
    ItemEntity apples(food.serverLevel,8,70,8,ItemInstance(Item::apple,3));apples.playerTouch(food.remote);food.receive();
    food.local->tick();food.remote->tick();
    assert(food.local->isUsingItem()&&food.local->getUseItemDuration()==remaining-1);
    assert(food.local->inventory->getItem(9)->count==5);
    for(int tick=9;tick<32;++tick){food.local->tick();food.remote->tick();}
    assert(food.remote->inventory->getItem(9)->count==4&&food.local->inventory->getItem(9)->count==5);
    assert(food.local->isUsingItem()&&food.local->getUseItemDuration()==0);
    assert(food.serverWire.events.size()==1&&food.serverWire.events[0].eventId==EntityEvent::USE_ITEM_COMPLETE);
    assert(food.serverWire.order.size()>=2&&food.serverWire.order[food.serverWire.order.size()-2]=='e'&&food.serverWire.order.back()=='i');
    const int before=food.effects.particles;
    auto completion=food.serverWire.events[0];food.client->handle(food.source,&completion);
    assert(!food.local->isUsingItem()&&food.effects.particles==before+10&&food.effects.burps==1);
    assert(food.local->inventory->getItem(9)->count==5); // Visual event never consumes.
    food.client->handle(food.source,&completion);
    assert(food.effects.particles==before+10&&food.effects.burps==1);
    food.receive();food.receive();food.remote->sendInventory();food.receive();
    assert(food.local->inventory->getItem(9)->count==4&&food.remote->inventory->getItem(9)->count==4);
    assert(food.local->inventory->getItem(10)->count==3&&food.remote->inventory->getItem(10)->count==3);
    assert(food.local->inventory->getItem(11)->getAuxValue()==7&&food.remote->inventory->getItem(11)->getAuxValue()==7);
    assert(food.effects.particles==before+10&&food.effects.burps==1);
    food.remote->completeUsingItem();assert(food.serverWire.events.size()==1);
    // A missing/no-op completion must not leave the hand pose stuck forever.
    assert(food.clientGame->gameMode->useItem(food.local,food.clientLevel,food.local->inventory->getSelected()));
    food.commitLast();
    for(int tick=0;tick<132;++tick)food.local->tick();
    assert(!food.local->isUsingItem()&&food.local->inventory->getItem(9)->count==4);
    food.remote->releaseNativeUsingItem();food.receive();
    assert(food.remote->inventory->getItem(9)->count==4&&food.effects.burps==1);
    food.local->health=20;
    const auto requests=food.clientWire.requests.size();
    assert(!food.clientGame->gameMode->useItem(food.local,food.clientLevel,food.local->inventory->getSelected()));
    assert(food.clientWire.requests.size()==requests&&!food.local->isUsingItem());
    Fixture last;last.put(9,Item::apple->id,1);last.remote->inventory->linkSlot(0,9,false);
    last.remote->health=10;last.local->health=10;last.negotiate();
    assert(last.clientGame->gameMode->useItem(last.local,last.clientLevel,last.local->inventory->getSelected()));last.commitLast();
    for(int tick=0;tick<32;++tick){last.local->tick();last.remote->tick();}
    assert(last.serverWire.events.size()==1);
    const int lastParticles=last.effects.particles;
    auto lastEvent=last.serverWire.events.front();last.client->handle(last.source,&lastEvent);last.receive();last.client->handle(last.source,&lastEvent);
    assert(!last.local->inventory->getSelected()&&!last.local->isUsingItem());
    assert(last.effects.particles==lastParticles+10&&last.effects.burps==1);
    Fixture legacy;ItemInstance apple(Item::apple,2);
    legacy.local->inventory->setItem(9,&apple);legacy.local->inventory->linkSlot(0,9,false);
    legacy.local->startUsingItem(apple,32);legacy.local->completeUsingItem();legacy.local->completeUsingItem();
    assert(legacy.local->inventory->getItem(9)->count==1);
    std::cout<<"Same-stack pickup preserves native eating; authoritative event plays particles/burp once without food/arrow/wear mutation PASS\n";
}
