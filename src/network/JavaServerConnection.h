#ifndef TCPE_JAVA_SERVER_CONNECTION_H
#define TCPE_JAVA_SERVER_CONNECTION_H
#include <string>
#include <functional>
#include "tcpe_java_network.h"
// Owned by the TCPE game thread; Rust handles the direct server socket.
class JavaServerConnection {
public:
 JavaServerConnection(); ~JavaServerConnection();
 bool connect(const std::string& server,const std::string& identity,bool online);
 void disconnect(); bool send(const std::string& commandJson);
 bool chat(const std::string& text);
 bool look(float yaw,float pitch);
 bool walk(float forward,float strafe,bool jump,bool sneak,bool sprint=false);
 bool selectHotbar(unsigned slot);
 bool number(const std::string& path,double& result)const;
 bool boolean(const std::string& path,bool& result)const;
 bool text(const std::string& path,std::string& result)const;
 bool arraySize(const std::string& path,size_t& result)const;
 // Consumers must copy retained values before this method polls another event.
 int pump(const std::function<void(const TcpeJavaEvent&)>& consume,int limit=128);
 bool connected()const{return session!=nullptr;}
 bool usesAccount()const{return session&&tcpe_java_uses_account(session); }
private:
 JavaServerConnection(const JavaServerConnection&)=delete;
 JavaServerConnection& operator=(const JavaServerConnection&)=delete;
 TcpeJavaSession* session;
};
#endif
