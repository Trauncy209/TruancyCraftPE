#include "JavaServerConnection.h"
#include <cassert>
#include <chrono>
#include <thread>
#include <iostream>
int main(){
 JavaServerConnection connection;
 assert(!connection.connect("127.0.0.1:0","invalid name",false));
 assert(connection.connect("127.0.0.1:0","TcpeAbiTest",false));
 assert(!connection.send("{\"type\":\"select_hotbar\",\"slot\":256}"));
 bool reported=false;
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
 while(!reported&&std::chrono::steady_clock::now()<deadline){
  connection.pump([&](const TcpeJavaEvent& event){
   if(event.kind==TCPE_JAVA_ERROR||event.kind==TCPE_JAVA_DISCONNECTED){assert(event.payload&&event.payload_len>0);std::string payload(reinterpret_cast<const char*>(event.payload),event.payload_len);assert(payload.front()=='{');std::string reason;assert(connection.text(event.kind==TCPE_JAVA_ERROR?"message":"reason",reason));assert(!reason.empty());double missing=0;assert(!connection.number("missing",missing));reported=true;}
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
 }
 assert(reported);connection.disconnect();connection.disconnect();
 assert(!connection.send("{\"type\":\"respawn\"}"));
 std::cout<<"PASS actual C++/Rust ABI connect, event delivery, validation and close lifecycle\n";
}
