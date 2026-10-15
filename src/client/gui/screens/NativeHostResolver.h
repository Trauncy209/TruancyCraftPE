#ifndef TCPE_NATIVE_HOST_RESOLVER_H
#define TCPE_NATIVE_HOST_RESOLVER_H
#include <thread>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <deque>
#include <map>
#include <string>
#include <chrono>
#if !defined(_WIN32)
#include <netdb.h>
#include <arpa/inet.h>
#endif
// Exact native-host A resolution. It never applies Java _minecraft._tcp SRV
// records, and DNS never blocks the menu/render thread. One bounded worker.
class NativeHostResolver{
 struct Entry{std::string ip;bool pending=false;std::chrono::steady_clock::time_point next{};};
 struct State{std::mutex lock;std::condition_variable wake;std::map<std::string,Entry> cache;std::deque<std::string> queue;bool stop=false;};
 std::shared_ptr<State> state=std::make_shared<State>();std::thread worker;
 NativeHostResolver(){auto s=state;worker=std::thread([s]{for(;;){std::string host;{std::unique_lock<std::mutex> g(s->lock);s->wake.wait(g,[&]{return s->stop||!s->queue.empty();});if(s->stop)return;host=s->queue.front();s->queue.pop_front();}std::string ip;
#if !defined(_WIN32)
 addrinfo hints{};hints.ai_family=AF_INET;hints.ai_socktype=SOCK_DGRAM;addrinfo* result=nullptr;if(!getaddrinfo(host.c_str(),nullptr,&hints,&result)){for(auto* p=result;p;p=p->ai_next){char text[INET_ADDRSTRLEN];if(inet_ntop(AF_INET,&reinterpret_cast<sockaddr_in*>(p->ai_addr)->sin_addr,text,sizeof(text))){ip=text;break;}}freeaddrinfo(result);}
#endif
 {std::lock_guard<std::mutex> g(s->lock);if(s->stop)return;auto& e=s->cache[host];e.ip=ip;e.pending=false;e.next=std::chrono::steady_clock::now()+std::chrono::seconds(ip.empty()?5:60);}}});}
 ~NativeHostResolver(){auto s=state;{std::lock_guard<std::mutex> g(s->lock);s->stop=true;s->wake.notify_all();}if(worker.joinable())worker.detach();} // Shared state outlives a slow system DNS call.
public:
 static std::string resolve(const std::string& host){
#if !defined(_WIN32)
 in_addr ip{};if(inet_pton(AF_INET,host.c_str(),&ip)==1)return host;
#endif
 if(host.empty()||host.find(':')!=std::string::npos)return "";static NativeHostResolver resolver;auto s=resolver.state;std::lock_guard<std::mutex> g(s->lock);auto found=s->cache.find(host);if(found==s->cache.end()&&s->cache.size()>=128){for(auto it=s->cache.begin();it!=s->cache.end();++it)if(!it->second.pending){s->cache.erase(it);break;}if(s->cache.size()>=128)return "";}auto& e=s->cache[host];if(!e.pending&&std::chrono::steady_clock::now()>=e.next&&s->queue.size()<32){e.pending=true;s->queue.push_back(host);s->wake.notify_one();}return e.ip;}
};
#endif
