#ifndef TCPE_JAVA_TITLE_STATE_H
#define TCPE_JAVA_TITLE_STATE_H
#include <string>
#include <cstdint>
#include <algorithm>
#include <cmath>
struct JavaTitleState {
 std::string title,subtitle;bool hasTitle=false;int fadeIn=10,stay=70,fadeOut=20;int64_t remaining=0;
 int64_t duration()const{return int64_t(fadeIn)+stay+fadeOut;}
 void clear(bool reset){title.clear();subtitle.clear();hasTitle=false;remaining=0;if(reset){fadeIn=10;stay=70;fadeOut=20;}}
 void setTitle(const std::string& text){title=text;hasTitle=true;remaining=duration();}
 void setTimes(int in,int hold,int out){if(in>=0)fadeIn=in;if(hold>=0)stay=hold;if(out>=0)fadeOut=out;if(remaining>0)remaining=duration();}
 void tick(){if(remaining>0&&--remaining==0){title.clear();subtitle.clear();hasTitle=false;}}
 int alpha(float partial=0)const{if(!hasTitle||remaining<=0)return 0;const double p=std::isfinite(partial)?std::max(0.f,std::min(1.f,partial)):0.;const double left=remaining-p;double value=255.;if(left>int64_t(stay)+fadeOut&&fadeIn>0)value=255.*(duration()-left)/fadeIn;else if(left<=fadeOut&&fadeOut>0)value=255.*left/fadeOut;return int(std::max(0.,std::min(255.,value)));}
};
#endif
