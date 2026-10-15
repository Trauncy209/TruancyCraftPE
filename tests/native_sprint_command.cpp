#include "network/JavaServerConnection.h"
#include <cassert>
#include <iostream>
#include <limits>
static std::string sent;
extern "C" TcpeJavaSession* __wrap_tcpe_java_connect(const char*,size_t,const char*,size_t,bool){return reinterpret_cast<TcpeJavaSession*>(1);}
extern "C" void __wrap_tcpe_java_close(TcpeJavaSession*){}
extern "C" bool __wrap_tcpe_java_command(TcpeJavaSession*,const char* data,size_t size){sent.assign(data,size);return true;}
int main(){
 JavaServerConnection connection;assert(connection.connect("fixture","NativeTest",false));
 assert(connection.walk(.25f,-.5f,false,false,true));
 assert(sent.find("\"forward\":0.250000")!=std::string::npos);
 assert(sent.find("\"strafe\":-0.500000")!=std::string::npos);
 assert(sent.find("\"sprint\":true")!=std::string::npos);
 assert(connection.walk(0,0,false,false));assert(sent.find("\"sprint\":false")!=std::string::npos);
 const std::string last=sent;assert(!connection.walk(std::numeric_limits<float>::quiet_NaN(),0,false,false,true));assert(sent==last);
 std::cout<<"Native sprint command: analog magnitude, sprint flag, stop/default and invalid-input rejection PASS\n";
}
