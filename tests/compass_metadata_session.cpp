#define main retained_dialog_main
#include "dialog_session.cpp"
#undef main
#include <limits>
static void global(const std::string& path,const std::string& dim,int x,int y,int z){strings[path+".dimension"]=dim;numbers[path+".position.0"]=x;numbers[path+".position.1"]=y;numbers[path+".position.2"]=z;}
static void compass(JavaGameSession& s){strings={{"compass.dimension","minecraft:overworld"}};numbers.clear();arrays.clear();booleans.clear();global("compass.default_spawn","minecraft:overworld",-123,64,456);global("compass.last_death","minecraft:the_nether",-2147483647,2147483647,0);eventKind=TCPE_JAVA_COMPASS;pending=true;s.tick();}
static void apply(JavaGameSession& s){pending=true;s.tick();}
int main(){JavaGameSession s;compass(s);assert(s.compass.dimension=="minecraft:overworld"&&s.compass.defaultSpawn.valid&&s.compass.lastDeath.valid&&s.compass.defaultSpawn.position[0]==-123&&s.compass.lastDeath.dimension=="minecraft:the_nether");
 for(int axis=0;axis<3;++axis)for(double bad:{1.5,2147483648.,-2147483649.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){compass(s);numbers["compass.default_spawn.position."+std::to_string(axis)]=bad;apply(s);assert(!s.compass.defaultSpawn.valid&&s.compass.lastDeath.valid);}
 for(const std::string dim:{std::string(),std::string(1025,'x')}){compass(s);strings["compass.default_spawn.dimension"]=dim;apply(s);assert(!s.compass.defaultSpawn.valid);}
 compass(s);strings["compass.default_spawn.dimension"]=std::string(1024,'x');numbers["compass.default_spawn.position.0"]=-2147483648.;apply(s);assert(s.compass.defaultSpawn.valid&&s.compass.defaultSpawn.position[0]==-2147483647-1);numbers.erase("compass.default_spawn.position.2");apply(s);assert(!s.compass.defaultSpawn.valid);
 strings.clear();numbers.clear();apply(s);assert(s.compass.dimension.empty()&&!s.compass.defaultSpawn.valid&&!s.compass.lastDeath.valid);
 // Tracked false still means the component exists; absent/null target stays invalid.
 eventKind=TCPE_JAVA_INVENTORY;strings={{"inventory.items.0.resource_id","minecraft:compass"}};arrays={{"inventory.items",1}};numbers={{"inventory.items.0.count",1}};booleans={{"inventory.items.0.lodestone_tracker.tracked",false}};global("inventory.items.0.lodestone_tracker.target","minecraft:the_end",1,2,3);apply(s);assert(s.inventory.slots[0].hasLodestoneTracker&&!s.inventory.slots[0].lodestoneTracked&&s.inventory.slots[0].lodestoneTarget.valid);
 strings.erase("inventory.items.0.lodestone_tracker.target.dimension");apply(s);assert(s.inventory.slots[0].hasLodestoneTracker&&!s.inventory.slots[0].lodestoneTarget.valid);booleans.clear();apply(s);assert(!s.inventory.slots[0].hasLodestoneTracker&&!s.inventory.slots[0].lodestoneTarget.valid);
 for(uint32_t kind:{uint32_t(TCPE_JAVA_LOGIN),uint32_t(TCPE_JAVA_CONFIGURATION),uint32_t(TCPE_JAVA_DISCONNECTED)}){compass(s);eventKind=kind;apply(s);assert(s.compass.dimension.empty()&&!s.compass.defaultSpawn.valid&&!s.compass.lastDeath.valid);}compass(s);s.disconnect();assert(s.compass.dimension.empty()&&!s.compass.defaultSpawn.valid&&!s.compass.lastDeath.valid);
 std::cout<<"Native compass metadata: dimension/global positions strict finite i32, optional tracker false/null semantics, snapshot/login/config/disconnect clearing PASS\n";
}
