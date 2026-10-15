#ifndef TCPE_NATIVE_OPERATORS_H
#define TCPE_NATIVE_OPERATORS_H
#include <fstream>
#include <set>
#include <string>
#include <cstdio>
#include <cctype>
// Console-owned, default-empty list. Never populated from client packets.
class NativeOperators {
 static std::set<std::string> load(){std::set<std::string> names;std::ifstream f("operators.txt");std::string s;while(std::getline(f,s))if(valid(s))names.insert(s);return names;}
public:
 static bool valid(const std::string& s){if(s.empty()||s.size()>16)return false;for(unsigned char c:s)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return false;return true;}
 static bool allowed(const std::string& name){return load().count(name)!=0;}
 static bool change(const std::string& name,bool grant){if(!valid(name))return false;auto names=load();if(grant)names.insert(name);else names.erase(name);std::ofstream f("operators.txt.new",std::ios::trunc);if(!f)return false;for(auto& n:names)f<<n<<'\n';f.close();return f.good()&&std::rename("operators.txt.new","operators.txt")==0;}
};
#endif
