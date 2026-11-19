#include "client/player/NativeInventoryClientState.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/entity/MobCategory.h"
#include "world/level/biome/Biome.h"
#include "util/Mth.h"
#include <cassert>
#include <iostream>
static int count(const std::vector<ItemInstance>& items,int id) { int n=0;for(const auto& item:items)if(item.id==id)n+=item.count;return n; }
int main() {
    Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();
    const auto& recipes=Recipes::getInstance()->getRecipes();int recipe=-1;
    for(unsigned int i=0;i<recipes.size();++i) {
        const auto needs=recipes[i]->getItemPack().getItemInstances();
        if(recipes[i]->getResultItem().id==5 && recipes[i]->getResultItem().count==4 && needs.size()==1 && needs[0].id==17 && needs[0].count==1) { recipe=i;break; }
    }
    assert(recipe>=0);
    NativeInventoryClientState client;NativeInventorySnapshot state;state.present=true;state.nonce=44;state.revision=1;
    NativeHotbarState hotbar;hotbar.present=true;hotbar.selected=0;for(auto& slot:hotbar.slots)slot=-1;hotbar.slots[0]=9;
    std::vector<ItemInstance> base(36);base[0]=ItemInstance(17,10,0);
    assert(client.accept(base,hotbar,state));
    NativeInventoryAction craft;craft.type=NativeInventoryAction::Craft;craft.recipe=recipe;
    assert(client.enqueue(craft,hotbar));assert(craft.sequence==1 && craft.baseRevision==1);
    NativeInventoryAction second=craft;assert(client.enqueue(second,hotbar));
    assert(count(client.items,17)==8 && count(client.items,5)==8);
    // Pickup races both queued crafts. Replaying on the fresh base retains it.
    base[0].count=13;state.revision=2;assert(client.accept(base,hotbar,state));
    assert(count(client.items,17)==11 && count(client.items,5)==8 && client.pending.size()==2);
    // First craft acknowledgement, with a newer authoritative pickup, must not
    // roll back the remaining craft or reapply the acknowledged craft.
    base[0].count=12;base[1]=ItemInstance(5,4,0);state.revision=3;state.acknowledged=1;
    assert(client.accept(base,hotbar,state));assert(count(client.items,17)==11 && count(client.items,5)==8 && client.pending.size()==1);
    const auto expected=client.items;
    auto stale=state;stale.revision=2;stale.acknowledged=0;
    assert(!client.accept(std::vector<ItemInstance>(36),hotbar,stale));assert(client.items==expected);
    state.revision=4;state.acknowledged=2;state.status=NativeInventoryAction::Insufficient;
    assert(client.accept(base,hotbar,state));assert(client.pending.empty() && client.items==base);
    auto foreign=state;foreign.nonce=45;assert(!client.accept(base,hotbar,foreign));
    // A failed proposal cannot consume part of its ingredients during replay.
    NativeInventoryAction impossible=craft;impossible.count=64;
    assert(client.enqueue(impossible,hotbar));assert(client.items==base);
    // Menu and owner inventory are one coherent base. Transfer prediction
    // removes resources from exactly one side and replays after new loot.
    client.reset();state=NativeInventorySnapshot();state.present=true;state.nonce=55;state.revision=1;
    state.containerId=7;state.containerItems=std::vector<ItemInstance>(3);state.containerItems[0]=ItemInstance(1,5,0);
    assert(client.accept(base,hotbar,state));
    NativeInventoryAction transfer;transfer.type=NativeInventoryAction::Transfer;transfer.flags=1;transfer.x=7;transfer.slot=0;transfer.count=2;transfer.expectedId=1;
    assert(client.enqueue(transfer,hotbar));assert(client.container[0].count==3 && count(client.items,1)==2);
    base[2]=ItemInstance(4,7,0);state.revision=2;
    assert(client.accept(base,hotbar,state));assert(client.container[0].count==3 && count(client.items,1)==2 && count(client.items,4)==7);
    // A stale source identity never predicts moving a replacement stack.
    state.revision=3;state.containerItems[0]=ItemInstance(3,5,0);
    assert(client.accept(base,hotbar,state));assert(count(client.items,1)==0 && client.container[0].id==3 && client.container[0].count==5);
    // A hotbar intent follows the predicted crafted item when a pickup takes
    // its former slot before the server applies the craft.
    client.reset();state=NativeInventorySnapshot();state.present=true;state.nonce=88;state.revision=1;
    base.assign(36,ItemInstance());base[0]=ItemInstance(17,2,0);assert(client.accept(base,hotbar,state));
    assert(client.enqueue(craft,hotbar));
    NativeHotbarState wanted=client.predictedHotbar;wanted.slots[0]=10;
    NativeInventoryAction select;select.type=NativeInventoryAction::Hotbar;assert(client.enqueue(select,wanted));
    base[1]=ItemInstance(4,3,0);state.revision=2;assert(client.accept(base,hotbar,state));
    assert(client.predictedHotbar.slots[0]==11 && client.items[2].id==5 && count(client.items,4)==3);
    client.reset();state.acknowledged=0;state.nonce=99;assert(client.accept(base,hotbar,state));
    NativeInventoryAction use;use.type=NativeInventoryAction::Use;
    for(int i=0;i<128;++i)assert(client.enqueue(use,hotbar));
    assert(!client.enqueue(use,hotbar) && client.pending.size()==128);
    std::cout<<"PASS: client semantic replay preserves concurrent loot, rapid crafts, partial/rejected acknowledgements, coherent transfers, stale source identity, nonce isolation and bounded queue\n";
}
