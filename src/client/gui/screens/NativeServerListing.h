#ifndef TCPE_NATIVE_SERVER_LISTING_H
#define TCPE_NATIVE_SERVER_LISTING_H
#include "../../SavedServerList.h"
#include "NativeHostResolver.h"
#include "../../Minecraft.h"
#include "../../../network/RakNetInstance.h"
#include <set>
#include <algorithm>
#include <cctype>
#include <chrono>
inline void refreshNativeServerDiscovery(Minecraft* game,const std::vector<SavedServerEntry>& saved){
 static std::chrono::steady_clock::time_point next{};auto now=std::chrono::steady_clock::now();if(now<next)return;next=now+std::chrono::seconds(1);for(const auto& s:saved)if(s.native){auto ip=NativeHostResolver::resolve(s.address);if(!ip.empty())game->raknetInstance->pingForHost(ip.c_str(),s.port);}
}
template<class Entry>std::vector<Entry> visibleServerEntries(const std::vector<SavedServerEntry>& saved,const ServerList& lan){
 std::vector<Entry> out;Entry featured;featured.title="Zeus";featured.subtitle="Featured server";featured.savedHost="zeuspvp.net";featured.statusHost="zeuspvp.net";featured.savedPort=25565;featured.javaAddress="zeuspvp.net:25565";featured.isSpecial=true;featured.isSaved=false;out.push_back(featured);
 std::set<std::string> native;
 for(const auto& s:saved){std::string lower=s.address;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return std::tolower(c);});if(!s.native&&lower=="zeuspvp.net"&&s.port==25565)continue;Entry e;e.savedHost=s.address;e.statusHost=s.native?NativeHostResolver::resolve(s.address):s.address;e.savedPort=s.port;e.isSaved=true;e.isSpecial=false;e.isNative=s.native;e.javaAddress=(s.native?"tcpe://":"")+(s.address.find(':')!=std::string::npos&&s.address.front()!='['?"["+s.address+"]":s.address)+":"+std::to_string(s.port);e.title=s.name.empty()?s.address:s.name;e.subtitle=e.javaAddress;if(s.native){native.insert("tcpe://"+e.statusHost+":"+std::to_string(s.port));for(const auto& l:lan)if(e.statusHost==l.address.ToString(false)&&s.port==l.address.GetPort())e.isLan=true;}out.push_back(e);}
 for(const auto& l:lan){if(l.name.IsEmpty())continue;Entry e;e.savedHost=l.address.ToString(false);e.statusHost=e.savedHost;e.savedPort=l.address.GetPort();e.javaAddress="tcpe://"+e.savedHost+":"+std::to_string(e.savedPort);if(native.count(e.javaAddress))continue;e.title=l.name.C_String();e.subtitle="TCPE LAN world";e.isSpecial=false;e.isSaved=false;e.isNative=true;e.isLan=true;e.address=l.address;out.push_back(e);native.insert(e.javaAddress);}
 return out;
}
#endif
