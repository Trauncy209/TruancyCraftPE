#define main retained_dialog_main
#include "dialog_session.cpp"
#undef main
#include "../original-tcpe/src/platform/input/Keyboard.h"
#include <limits>
int Keyboard::_states[256]={};
static void publish(JavaGameSession& s,const char* kind="block",double revision=1,double epoch=2){
 eventKind=TCPE_JAVA_TARGETING;strings={{"targeting.kind",kind}};
 numbers={{"targeting.epoch",epoch},{"targeting.revision",revision},{"targeting.input_revision",double(s.pickInputRevision)},
 {"targeting.eye.0",-1.5},{"targeting.eye.1",65.62},{"targeting.eye.2",-4.},
 {"targeting.location.0",-1.5},{"targeting.location.1",64.0625},{"targeting.location.2",-1.5},
 {"targeting.block.0",-2},{"targeting.block.1",64},{"targeting.block.2",-2},{"targeting.state",27648},{"targeting.face",1},{"targeting.entity",12}};
 arrays={{"targeting.eye",3},{"targeting.location",3},{"targeting.outline.boxes",1},{"targeting.outline.boxes.0",6},{"targeting.outline.edges",1},{"targeting.outline.edges.0",6}};
 const double box[]={0,0,0,1,.0625,1},edge[]={0,.0625,0,1,.0625,0};
 for(int i=0;i<6;++i){numbers["targeting.outline.boxes.0."+std::to_string(i)]=box[i];numbers["targeting.outline.edges.0."+std::to_string(i)]=edge[i];}
 booleans={{"targeting.outline.complete",true}};pending=true;s.tick();
}
int main(){
 JavaGameSession s;assert(s.connect("local","QA",false));publish(s,"clear",0);assert(s.targetingEpoch==2);s.spawned=true;
 assert(s.updatePickRay(true,{{0,0,0}},{{0,0,1}}));assert(sent.find("\"type\":\"pick_ray\"")!=std::string::npos);
 const auto input=s.pickInputRevision;assert(s.updatePickRay(true,{{0,0,0}},{{0,0,1}}));assert(s.pickInputRevision==input);
 std::vector<uint16_t> states(256*16,0);states[(14*16+14)*16]=27648;
 TcpeJavaEvent chunk{};chunk.kind=TCPE_JAVA_CHUNK;chunk.x=chunk.z=-1;chunk.min_y=64;chunk.height=16;chunk.states=states.data();chunk.states_len=states.size();assert(s.chunks.apply(chunk));
 publish(s);assert(s.hasTargeting());assert(s.targeting.edges[0][1]==.0625&&s.targeting.boxes[0][4]==.0625);
 publish(s,"block",1);assert(s.targeting.revision==1);publish(s,"block",2,1);assert(s.targeting.epoch==2&&s.targeting.revision==1);
 publish(s,"entity",2);assert(!s.hasTargeting());s.entities[12]=JavaGameSession::Entity{};assert(s.hasTargeting());s.entities.erase(12);assert(!s.hasTargeting());
 publish(s,"block",3);assert(s.hasTargeting());s.chunks.unload(-1,-1);assert(!s.hasTargeting());assert(s.chunks.apply(chunk));
 publish(s,"block",4);for(int i=0;i<6;++i)s.tick();assert(!s.hasTargeting());
 publish(s,"block",5);numbers["targeting.outline.edges.0.0"]=std::numeric_limits<double>::quiet_NaN();numbers["targeting.revision"]=6;pending=true;s.tick();assert(s.targeting.revision==5);
 assert(!s.updatePickRay(true,{{17,0,0}},{{0,0,1}}));assert(!s.updatePickRay(true,{{0,0,0}},{{0,0,2}}));
 assert(s.updatePickRay(false,{{0,0,0}},{{0,0,1}}));assert(!s.hasTargeting());
 for(auto kind:{TCPE_JAVA_LOGIN,TCPE_JAVA_ERROR,TCPE_JAVA_DISCONNECTED,TCPE_JAVA_DEATH,TCPE_JAVA_CONFIGURATION,TCPE_JAVA_LOOK_CORRECTION}){publish(s,"block",10);eventKind=kind;pending=true;s.tick();assert(s.targeting.kind=="miss"&&!s.pickRaySent);s.spawned=true;}
 {JavaGameSession weapon;assert(weapon.connect("local","QA",false));publish(weapon,"clear",0);weapon.spawned=true;assert(weapon.updatePickRay(true,{{0,0,0}},{{0,0,1}}));publish(weapon,"miss",1);numbers["targeting.attack_entity"]=12;numbers["targeting.revision"]=2;arrays["targeting.attack_location"]=3;for(int i=0;i<3;++i)numbers["targeting.attack_location."+std::to_string(i)]=0;pending=true;weapon.tick();weapon.entities[12]=JavaGameSession::Entity{};assert(!weapon.hasTargeting()&&weapon.hasAttackTargeting());weapon.entities.erase(12);assert(!weapon.hasAttackTargeting());}
 s.disconnect();assert(s.targetingEpoch==0&&!s.hasTargeting());std::cout<<"Modern targeting Session: actual shape data, negative coordinates, action-specific attack/ordinary reach separation, revision/epoch/input/loaded-state/age/entity validation and lifecycle reset PASS\n";
}
