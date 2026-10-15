#ifndef TCPE_JAVA_SERVER_ROW_H
#define TCPE_JAVA_SERVER_ROW_H
#include "../GuiComponent.h"
#include "../Font.h"
#include "../../Minecraft.h"
#include "../../renderer/Textures.h"
#include "../../renderer/Tesselator.h"
#include "tcpe_java_network.h"
#include <map>
#include <chrono>
#include <cstring>
#include <functional>
#include <vector>
#include <set>
class JavaServerRow:public GuiComponent{
 struct Status{TcpeJavaStatusRequest* request=nullptr;std::string motd,texture;int online=0,max=0,latency=0,state=0;bool uploaded=false;std::vector<unsigned char> icon;std::chrono::steady_clock::time_point next{};~Status(){if(request)tcpe_java_status_free(request);}};
 static std::map<std::string,Status>& cache(){static std::map<std::string,Status> values;return values;}
public:
 using Endpoint=std::pair<std::string,unsigned short>;
 static void refresh(Minecraft* game,const std::vector<Endpoint>& servers){refreshAt(game,servers,std::chrono::steady_clock::now());}
 static void refreshAt(Minecraft* game,const std::vector<Endpoint>& servers,std::chrono::steady_clock::time_point now){
  auto& values=cache();std::vector<Endpoint> unique;std::set<std::string> live;
  for(const auto& server:servers){const auto key=server.first+":"+std::to_string(server.second);if(live.insert(key).second)unique.push_back(server);}
  // Entries removed from the saved list must not retain workers or textures.
  for(auto it=values.begin();it!=values.end();)if(!live.count(it->first)){if(it->second.uploaded)game->textures->removeTexture(it->second.texture);it=values.erase(it);}else ++it;
  for(const auto& server:unique){const auto key=server.first+":"+std::to_string(server.second);auto& status=values[key];if(!status.request)continue;
   TcpeJavaStatusView view{};const int result=tcpe_java_status_poll(status.request,&view);if(!result)continue;
   status.state=result;if(result>0){status.motd.assign(reinterpret_cast<const char*>(view.motd),view.motd_len);status.online=view.online;status.max=view.max;status.latency=view.latency;
    if(view.icon&&view.icon_len==64*64*4){if(status.icon.size()!=view.icon_len||std::memcmp(status.icon.data(),view.icon,view.icon_len)!=0){status.texture="java-server-icon/"+std::to_string(std::hash<std::string>{}(key));game->textures->removeTexture(status.texture);TextureData data;data.w=data.h=64;data.numBytes=view.icon_len;data.data=new unsigned char[view.icon_len];std::memcpy(data.data,view.icon,view.icon_len);game->textures->assignTexture(status.texture,data);status.icon.assign(view.icon,view.icon+view.icon_len);status.uploaded=true;}}
    else if(status.uploaded){game->textures->removeTexture(status.texture);status.icon.clear();status.uploaded=false;}
   }
   tcpe_java_status_free(status.request);status.request=nullptr;
  }
  // The transport allows four outstanding requests. Rotate across the entire
  // list, including offscreen rows, so a slow server cannot starve later rows.
  static size_t cursor=0;if(unique.empty()){cursor=0;return;}cursor%=unique.size();
  const size_t start=cursor;
  for(size_t step=0;step<unique.size();++step){const size_t index=(start+step)%unique.size();const auto& server=unique[index];auto& status=values[server.first+":"+std::to_string(server.second)];if(status.request||now<status.next)continue;
   cursor=(index+1)%unique.size();status.request=tcpe_java_status_start(server.first.data(),server.first.size(),server.second);if(!status.request)break;
   status.next=now+std::chrono::seconds(1);
  }
 }
 static void render(Minecraft* game,const std::string& host,unsigned short port,const std::string& title,bool featured,int left,int right,int y,bool native=false,bool lanKnown=false){
  JavaServerRow painter;const std::string key=host+":"+std::to_string(port);auto& status=cache()[key];
  glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glEnable2(GL_TEXTURE_2D);
  if(status.uploaded||native){game->textures->loadAndBindTexture(status.uploaded?status.texture:"gui/tcpe-server.png");auto& t=Tesselator::instance;t.begin();t.color(255,255,255,255);t.vertexUV(left,y+35,0,0,1);t.vertexUV(left+32,y+35,0,1,1);t.vertexUV(left+32,y+3,0,1,0);t.vertexUV(left,y+3,0,0,0);t.draw();}else{painter.fill(left,y+3,left+32,y+35,0xff303030);}
  glEnable2(GL_BLEND);Font& font=*game->font;auto draw=[&](const std::string& text,int x,int row,int maxWidth){auto fitted=font.fitJava(text,maxWidth);font.drawJava(fitted,x+1,row+1,0xffffffff,true);font.drawJava(fitted,x,row,0xffffffff);};
  std::string count=status.state>0?std::to_string(status.online)+"/"+std::to_string(status.max):native&&lanKnown?"§aOnline":status.state<0?"§cOffline":"§7Pinging...";const int countWidth=font.widthJava(count);draw(count,right-countWidth-18,y+3,countWidth);
  const int bars=status.state>0?(status.latency<150?5:status.latency<300?4:status.latency<600?3:status.latency<1000?2:1):0;for(int i=0;i<5;++i)painter.fill(right-14+i*3,y+11-i,right-12+i*3,y+12,i<bars?0xff55ff55:0xff555555);glEnable2(GL_BLEND);
  draw((featured?"§e★ §r":"")+title,left+38,y+3,std::max(0,right-left-60-countWidth));
  const std::string description=(native?"§7TCPE · §r":"")+(status.state>0?status.motd:key);auto lines=font.wrapJava(description,1048576);for(size_t i=0;i<std::min(size_t(2),lines.size());++i)draw(lines[i],left+38,y+14+i*10,right-left-38);
 }
};
#endif
