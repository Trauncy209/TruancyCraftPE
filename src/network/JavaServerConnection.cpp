#include "JavaServerConnection.h"
JavaServerConnection::JavaServerConnection():session(nullptr){}
JavaServerConnection::~JavaServerConnection(){disconnect();}
bool JavaServerConnection::connect(const std::string& server,const std::string& identity,bool online){
 disconnect();session=tcpe_java_connect(server.data(),server.size(),identity.data(),identity.size(),online);return session!=nullptr;
}
void JavaServerConnection::disconnect(){if(session){tcpe_java_close(session);session=nullptr;}}
bool JavaServerConnection::send(const std::string& json){return session&&tcpe_java_command(session,json.data(),json.size());}
int JavaServerConnection::pump(const std::function<void(const TcpeJavaEvent&)>& consume,int limit){
 if(!session||limit<=0)return 0;int count=0;
 while(count<limit){TcpeJavaEvent event{};int result=tcpe_java_poll(session,&event);if(result==0)break;if(result<0){disconnect();break;}consume(event);++count;}
 return count;
}

#include <cmath>
#include <sstream>
#include <iomanip>
namespace {
std::string jsonString(const std::string& input){
 std::ostringstream out;out<<'"';
 for(unsigned char c:input){if(c=='"'||c=='\\')out<<'\\'<<static_cast<char>(c);else if(c<32)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<unsigned>(c)<<std::dec;else out<<static_cast<char>(c);}out<<'"';return out.str();
}
}
bool JavaServerConnection::chat(const std::string& text){return send("{\"type\":\"chat\",\"text\":"+jsonString(text)+"}");}
bool JavaServerConnection::look(float yaw,float pitch){if(!std::isfinite(yaw)||!std::isfinite(pitch)||pitch< -90.f||pitch>90.f)return false;return send("{\"type\":\"look\",\"yaw\":"+std::to_string(yaw)+",\"pitch\":"+std::to_string(pitch)+"}");}
bool JavaServerConnection::walk(float forward,float strafe,bool jump,bool sneak,bool sprint){if(!std::isfinite(forward)||!std::isfinite(strafe)||forward< -1||forward>1||strafe< -1||strafe>1)return false;return send("{\"type\":\"walk\",\"forward\":"+std::to_string(forward)+",\"strafe\":"+std::to_string(strafe)+",\"jump\":"+(jump?"true":"false")+",\"sneak\":"+(sneak?"true":"false")+",\"sprint\":"+(sprint?"true":"false")+"}");}
bool JavaServerConnection::selectHotbar(unsigned slot){return slot<9&&send("{\"type\":\"select_hotbar\",\"slot\":"+std::to_string(slot)+"}");}

bool JavaServerConnection::number(const std::string& path,double& value)const{return session&&tcpe_java_number(session,path.data(),path.size(),&value);}
bool JavaServerConnection::boolean(const std::string& path,bool& value)const{return session&&tcpe_java_boolean(session,path.data(),path.size(),&value);}
bool JavaServerConnection::text(const std::string& path,std::string& value)const{TcpeJavaText text{};if(!session||!tcpe_java_text(session,path.data(),path.size(),&text))return false;value.assign(reinterpret_cast<const char*>(text.data),text.len);return true;}
bool JavaServerConnection::arraySize(const std::string& path,size_t& value)const{return session&&tcpe_java_array_size(session,path.data(),path.size(),&value);}
