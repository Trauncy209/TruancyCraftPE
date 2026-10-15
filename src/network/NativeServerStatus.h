#ifndef TCPE_NATIVE_SERVER_STATUS_H
#define TCPE_NATIVE_SERVER_STATUS_H
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <string>
#include <vector>
#include <fstream>
#include <cstdint>
#include <cstring>
#if defined(_WIN32)
// The current native server release targets Android/Linux.
class NativeServerStatus{public:void start(int,const std::string&,int){}void update(int){}void stop(){}bool active()const{return false;}};
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
// Listing metadata only. Gameplay continues to use the original UDP RakNet
// protocol. TCP status uses the existing status client/icon decoder, on the same
// numeric port, without sending a login or translating any gameplay packets.
class NativeServerStatus{
 std::thread worker;std::atomic<bool> running{false};std::atomic<int> online{
#ifdef STANDALONE_SERVER
 0
#else
 1
#endif
 };int listener=-1,port=0,maxPlayers=1;std::string title,icon;
 static void varint(std::string& out,unsigned value){do{unsigned char b=value&127;value>>=7;out+=char(b|(value?128:0));}while(value);}
 static std::chrono::steady_clock::time_point& deadline(){static thread_local std::chrono::steady_clock::time_point value;return value;}
 static bool wait(int fd,short events){auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(deadline()-std::chrono::steady_clock::now()).count();if(ms<=0)return false;pollfd p{fd,events,0};return poll(&p,1,int(ms))==1;}
 static bool bytes(int fd,char* data,size_t n){while(n){if(!wait(fd,POLLIN))return false;int got=recv(fd,data,n,0);if(got<=0)return false;data+=got;n-=got;}return true;}
 static bool number(int fd,unsigned& n){n=0;for(unsigned i=0;i<5;++i){unsigned char b;if(!bytes(fd,reinterpret_cast<char*>(&b),1))return false;if(i==4&&(b&0xf0))return false;n|=unsigned(b&127)<<(7*i);if(!(b&128))return true;}return false;}
 static bool packet(int fd,std::string& out){unsigned n;if(!number(fd,n)||!n||n>512)return false;out.resize(n);return bytes(fd,&out[0],n);}
 static bool sendPacket(int fd,const std::string& data){std::string wire;varint(wire,data.size());wire+=data;const char* p=wire.data();size_t left=wire.size();while(left){if(!wait(fd,POLLOUT))return false;int n=send(fd,p,left,MSG_NOSIGNAL);if(n<=0)return false;p+=n;left-=n;}return true;}
 static std::string quoted(const std::string& in){std::string out="\"";for(unsigned char c:in){if(c=='"'||c=='\\'){out+='\\';out+=c;}else if(c<32){const char* h="0123456789abcdef";out+="\\u00";out+=h[c>>4];out+=h[c&15];}else out+=c;}return out+'"';}
 static std::string base64(const std::string& in){const char* chars="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::string out;for(size_t i=0;i<in.size();i+=3){unsigned a=(unsigned char)in[i],b=i+1<in.size()?(unsigned char)in[i+1]:0,c=i+2<in.size()?(unsigned char)in[i+2]:0;out+=chars[a>>2];out+=chars[((a&3)<<4)|(b>>4)];out+=i+1<in.size()?chars[((b&15)<<2)|(c>>6)]:'=';out+=i+2<in.size()?chars[c&63]:'=';}return out;}
 static std::string readIcon(){for(const char* path:{"server-icon.png","data/images/gui/tcpe-server.png"}){std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)continue;auto size=f.tellg();if(size<24||size>65536)continue;f.seekg(0);std::string data(size,'\0');if(!f.read(&data[0],size))continue;const unsigned char png[]={137,80,78,71,13,10,26,10};if(std::memcmp(data.data(),png,8)||std::memcmp(data.data()+12,"IHDR",4))continue;const unsigned char* p=reinterpret_cast<const unsigned char*>(data.data());auto be=[&](int at){return unsigned(p[at])<<24|unsigned(p[at+1])<<16|unsigned(p[at+2])<<8|p[at+3];};if(be(16)==64&&be(20)==64)return "data:image/png;base64,"+base64(data);}return "";}
 void serve(int fd){deadline()=std::chrono::steady_clock::now()+std::chrono::milliseconds(700);std::string in;if(!packet(fd,in)||in[0]!=0||in.back()!=1)return;if(!packet(fd,in)||in.size()!=1||in[0]!=0)return;std::string json="{\"version\":{\"name\":\"TCPE native\",\"protocol\":-1},\"players\":{\"online\":"+std::to_string(online.load())+",\"max\":"+std::to_string(maxPlayers)+"},\"description\":{\"text\":"+quoted(title)+"}";if(!icon.empty())json+=",\"favicon\":"+quoted(icon);json+='}';std::string response(1,'\0');varint(response,json.size());response+=json;if(!sendPacket(fd,response))return;if(packet(fd,in)&&in.size()==9&&in[0]==1)sendPacket(fd,in);}
 void loop(){while(running){pollfd p{listener,POLLIN,0};if(poll(&p,1,100)!=1)continue;int fd=accept(listener,nullptr,nullptr);if(fd<0)continue;int flags=fcntl(fd,F_GETFL,0);if(flags<0||fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0){close(fd);continue;}serve(fd);close(fd);}}
public:
 ~NativeServerStatus(){stop();}
 void start(int value,const std::string& name,int maximum){if(running&&port==value&&title==name)return;stop();if(name.empty())return;listener=socket(AF_INET,SOCK_STREAM,0);if(listener<0)return;int reuse=1;setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_ANY);addr.sin_port=htons(value);if(bind(listener,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))||listen(listener,4)){close(listener);listener=-1;return;}port=value;title=name;maxPlayers=maximum;icon=readIcon();running=true;worker=std::thread([this]{loop();});}
 void update(int count){online=count;}
 void stop(){running=false;if(worker.joinable())worker.join();if(listener>=0){close(listener);listener=-1;}}
 bool active()const{return running;}
};
#endif
#endif
