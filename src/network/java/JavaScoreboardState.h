#ifndef TCPE_JAVA_SCOREBOARD_STATE_H
#define TCPE_JAVA_SCOREBOARD_STATE_H
#include <map>
#include <set>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
struct JavaScoreNumber {std::string kind,text,nbt;};
struct JavaScoreEntry {int32_t score=0;std::string display;bool hasDisplay=false;JavaScoreNumber number;};
struct JavaScoreObjective {std::string text;int renderType=0;JavaScoreNumber number;std::map<std::string,JavaScoreEntry> scores;};
struct JavaScoreTeam {std::string text,prefix,suffix;int color=21,options=0,visibility=0,collision=0;std::set<std::string> members;};
struct JavaScoreboardState {
 std::map<std::string,JavaScoreObjective> objectives;
 std::map<int,std::string> displays;
 std::map<std::string,JavaScoreTeam> teams;
 std::map<std::string,std::string> memberTeam;

 const JavaScoreObjective* sidebar(const std::string& player)const{int slot=1;auto member=memberTeam.find(player);if(member!=memberTeam.end()){auto team=teams.find(member->second);if(team!=teams.end()&&team->second.color>=0&&team->second.color<16&&displays.count(team->second.color+3))slot=team->second.color+3;}auto binding=displays.find(slot);if(binding==displays.end())return nullptr;auto objective=objectives.find(binding->second);return objective==objectives.end()?nullptr:&objective->second;}
 struct Row{std::string owner,label,number;int32_t score;};
 std::vector<Row> rows(const JavaScoreObjective& objective)const{
  std::vector<Row> out;for(const auto& entry:objective.scores){if(!entry.first.empty()&&entry.first[0]=='#')continue;const auto& value=entry.second;std::string label=value.hasDisplay?value.display:entry.first;
   if(!value.hasDisplay){auto member=memberTeam.find(entry.first);if(member!=memberTeam.end()){auto team=teams.find(member->second);if(team!=teams.end()){std::string color="§r";if(team->second.color>=0&&team->second.color<16)color="§"+std::string(1,"0123456789abcdef"[team->second.color]);label=team->second.prefix+color+entry.first+"§r"+team->second.suffix;}}}
   const auto& format=value.number.kind.empty()?objective.number:value.number;std::string number=format.kind=="blank"?"":format.kind=="fixed"?format.text:format.kind=="styled"?format.text+std::to_string(value.score):"§c"+std::to_string(value.score);out.push_back({entry.first,label,number,value.score});
  }
  auto folded=[](std::string text){for(char& c:text)if(c>='A'&&c<='Z')c+=32;return text;};
  std::stable_sort(out.begin(),out.end(),[&](const Row& a,const Row& b){return a.score!=b.score?a.score>b.score:folded(a.owner)<folded(b.owner);});if(out.size()>15)out.resize(15);return out;
 }
 void clear(){objectives.clear();displays.clear();teams.clear();memberTeam.clear();}
 void removeObjective(const std::string& name){objectives.erase(name);for(auto i=displays.begin();i!=displays.end();)if(i->second==name)i=displays.erase(i);else ++i;}
 void resetScore(const std::string& owner,const std::string* objective){if(objective){auto i=objectives.find(*objective);if(i!=objectives.end())i->second.scores.erase(owner);}else for(auto& entry:objectives)entry.second.scores.erase(owner);}
 void leave(const std::string& team,const std::string& member){auto i=teams.find(team);if(i!=teams.end())i->second.members.erase(member);auto owner=memberTeam.find(member);if(owner!=memberTeam.end()&&owner->second==team)memberTeam.erase(owner);}
 void join(const std::string& team,const std::string& member){auto i=teams.find(team);if(i==teams.end())return;auto old=memberTeam.find(member);if(old!=memberTeam.end()&&old->second!=team)leave(old->second,member);i->second.members.insert(member);memberTeam[member]=team;}
 void removeTeam(const std::string& name){auto i=teams.find(name);if(i==teams.end())return;const auto members=i->second.members;for(const auto& member:members)leave(name,member);teams.erase(name);}
};
#endif
