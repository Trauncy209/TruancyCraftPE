#define main retained_dialog_main
#include "dialog_session.cpp"
#undef main
#include <limits>
static void gust(JavaGameSession& s,const char* kind="gust",double count=1,double x=1,double offset=0){eventKind=TCPE_JAVA_VISUAL_EFFECT;strings={{"visual_effect.kind",kind}};numbers={{"visual_effect.position.0",x},{"visual_effect.position.1",2},{"visual_effect.position.2",3},{"visual_effect.count",count},{"visual_effect.offset.0",offset},{"visual_effect.offset.1",0},{"visual_effect.offset.2",0}};pending=true;s.tick();}
int main(){JavaGameSession s;assert(s.connect("local","QA",false));gust(s,"gust",4,1,2);assert(s.pendingVisualEffects.size()==1&&s.pendingVisualEffects[0].count==4&&s.pendingVisualEffects[0].offset[0]==2);s.pendingVisualEffects.clear();
for(double count:{0.,257.,1.5,std::numeric_limits<double>::quiet_NaN()})gust(s,"gust",count);gust(s,"unsupported");gust(s,"gust",1,std::numeric_limits<double>::infinity());gust(s,"gust",1,1,1025);assert(s.pendingVisualEffects.empty());
for(int i=0;i<300;++i)gust(s);assert(s.pendingVisualEffects.size()==256);eventKind=TCPE_JAVA_LOGIN;pending=true;s.tick();assert(s.pendingVisualEffects.empty());
for(auto kind:{TCPE_JAVA_ERROR,TCPE_JAVA_DISCONNECTED}){gust(s);assert(!s.pendingVisualEffects.empty());eventKind=kind;pending=true;s.tick();assert(s.pendingVisualEffects.empty());}
s.connect("local","QA",false);gust(s);s.disconnect();assert(s.pendingVisualEffects.empty());s.connect("local","QA",false);gust(s);s.phase="Game";closeAfterPoll=true;s.tick();assert(s.pendingVisualEffects.empty()&&s.phase=="Error");std::cout<<"Native gust queue: counts/offsets, bounds/malformed rejection, capped queue and login/error/disconnect/transport-close clearing PASS\n";}
