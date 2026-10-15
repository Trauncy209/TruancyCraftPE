#define main retained_dialog_main
#include "dialog_session.cpp"
#undef main
#include <limits>
void setup(){strings={{"inventory.items.0.resource_id","minecraft:paper"},{"inventory.items.0.item_model","oraxen:sword"},{"inventory.items.0.custom_model_data.strings.0","first"},{"inventory.items.0.custom_model_data.strings.1","second"}};arrays={{"inventory.items",1},{"inventory.items.0.custom_model_data.floats",2},{"inventory.items.0.custom_model_data.flags",2},{"inventory.items.0.custom_model_data.strings",2},{"inventory.items.0.custom_model_data.colors",2}};numbers={{"inventory.items.0.count",1},{"inventory.items.0.custom_model_data.floats.0",0},{"inventory.items.0.custom_model_data.floats.1",123.5},{"inventory.items.0.custom_model_data.colors.0",-1},{"inventory.items.0.custom_model_data.colors.1",16777215}};booleans={{"inventory.items.0.custom_model_data.flags.0",false},{"inventory.items.0.custom_model_data.flags.1",true}};eventKind=TCPE_JAVA_INVENTORY;}
void apply(JavaGameSession& s){pending=true;s.tick();}
int main(){JavaGameSession s;setup();apply(s);auto item=s.inventory.slots[0];assert(item.resourceId=="minecraft:paper"&&item.itemModel=="oraxen:sword"&&item.customModelData.present);assert(item.customModelData.floats[0]==0&&item.customModelData.floats[1]==123.5f);assert(!item.customModelData.flags[0]&&item.customModelData.flags[1]);assert(item.customModelData.strings[1]=="second"&&item.customModelData.colors[0]==-1);
 setup();numbers["inventory.items.0.custom_model_data.floats.1"]=std::numeric_limits<double>::infinity();apply(s);assert(!s.inventory.slots[0].customModelData.present&&s.inventory.slots[0].resourceId=="minecraft:paper");
 setup();arrays["inventory.items.0.custom_model_data.flags"]=1025;apply(s);assert(!s.inventory.slots[0].customModelData.present);
 setup();strings["inventory.items.0.custom_model_data.strings.0"]=std::string(2049,'x');apply(s);assert(!s.inventory.slots[0].customModelData.present);
 setup();numbers["inventory.items.0.custom_model_data.colors.0"]=1.5;apply(s);assert(!s.inventory.slots[0].customModelData.present);
 setup();arrays.erase("inventory.items.0.custom_model_data.flags");apply(s);assert(!s.inventory.slots[0].customModelData.present);
 setup();strings["inventory.items.0.item_model"]="Bad:model";apply(s);assert(s.inventory.slots[0].itemModel.empty()&&s.inventory.slots[0].customModelData.present);
 std::cout<<"Native item model metadata: typed ordered arrays, exact indices, malformed component rejection and fallback preservation PASS\n";
}
