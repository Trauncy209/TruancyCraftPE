#define main retained_dialog_main
#include "dialog_session.cpp"
#undef main
int main(){
 JavaGameSession game;assert(game.connect("local","QA",false));eventKind=TCPE_JAVA_INVENTORY;
 arrays={{"inventory.items",1}};numbers={{"inventory.items.0.count",1}};
 strings={{"inventory.items.0.resource_id","minecraft:crossbow"},{"inventory.items.0.charged_projectile","arrow"}};
 pending=true;game.tick();assert(game.inventory.slots.size()==1&&game.inventory.slots[0].chargedProjectile=="arrow");
 strings["inventory.items.0.charged_projectile"]="rocket";pending=true;game.tick();assert(game.inventory.slots[0].chargedProjectile=="rocket");
 strings["inventory.items.0.charged_projectile"]="unrecognized";pending=true;game.tick();assert(game.inventory.slots[0].chargedProjectile.empty());
 strings.erase("inventory.items.0.charged_projectile");pending=true;game.tick();assert(game.inventory.slots[0].chargedProjectile.empty());
 game.disconnect();assert(game.inventory.slots.empty());
 std::cout<<"Crossbow item snapshot: arrow/rocket, unknown/missing reset and disconnect cleanup PASS\n";
}
