#define main retained_dialog_main
#include "dialog_session.cpp"
#undef main
#include <limits>
static void setMob(){eventKind=TCPE_JAVA_ENTITIES;strings={{"entities.0.kind","IronGolem"}};arrays={{"entities",1}};numbers={{"entities.0.id",42},{"entities.0.position.0",1},{"entities.0.position.1",64},{"entities.0.position.2",2},{"entities.0.hurt_ticks",8},{"entities.0.mob_attack_ticks",10},{"entities.0.mob_flower_ticks",400},{"entities.0.living_health",37.5},{"entities.0.living_max_health",200}};}
static void updateMob(JavaGameSession& s){pending=true;s.tick();}
int main(){JavaGameSession s;setMob();updateMob(s);auto entity=s.entities.at(42);assert(entity.mobAttackTicks==10&&entity.mobFlowerTicks==400&&entity.hasLivingHealth&&entity.livingHealth==37.5f&&entity.hurtTicks==8&&entity.hasLivingMaxHealth&&entity.livingMaxHealth==200);
 for(double invalid:{-1.,11.,1.5,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){setMob();numbers["entities.0.mob_attack_ticks"]=invalid;updateMob(s);assert(s.entities.at(42).mobAttackTicks==0&&s.entities.at(42).hurtTicks==8);}
 for(double invalid:{-1.,401.,1.5,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){setMob();numbers["entities.0.mob_flower_ticks"]=invalid;updateMob(s);assert(s.entities.at(42).mobFlowerTicks==0);}
 for(double invalid:{std::numeric_limits<double>::max(),std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){setMob();numbers["entities.0.living_health"]=invalid;updateMob(s);assert(!s.entities.at(42).hasLivingHealth);}
 for(double invalid:{0.,-1.,std::numeric_limits<double>::max(),std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){setMob();numbers["entities.0.living_max_health"]=invalid;updateMob(s);assert(!s.entities.at(42).hasLivingMaxHealth);}
 setMob();numbers.erase("entities.0.living_max_health");updateMob(s);assert(!s.entities.at(42).hasLivingMaxHealth);
 setMob();numbers["entities.0.living_max_health"]=std::numeric_limits<double>::denorm_min();updateMob(s);assert(!s.entities.at(42).hasLivingMaxHealth);
 setMob();numbers.erase("entities.0.mob_attack_ticks");numbers.erase("entities.0.mob_flower_ticks");numbers.erase("entities.0.living_health");updateMob(s);assert(s.entities.at(42).mobAttackTicks==0&&s.entities.at(42).mobFlowerTicks==0&&!s.entities.at(42).hasLivingHealth&&s.entities.at(42).hurtTicks==8);
 setMob();numbers["entities.0.mob_attack_ticks"]=0;numbers["entities.0.mob_flower_ticks"]=0;numbers["entities.0.living_health"]=0;updateMob(s);assert(s.entities.at(42).hasLivingHealth&&s.entities.at(42).livingHealth==0);
 std::cout<<"Mob metadata handoff: authoritative attack/flower counters, optional finite health, malformed fallback and no hurt-as-attack PASS\n";
}
