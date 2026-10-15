#ifndef TCPE_JAVA_CONNECT_SCREEN_H
#define TCPE_JAVA_CONNECT_SCREEN_H
#include "../Screen.h"
#include "../Font.h"
#include "../components/Button.h"
#include "../../Minecraft.h"
#include "JoinGameScreen.h"
#include "../../../network/java/JavaGameSession.h"
class JavaConnectScreen:public Screen {
public:
 JavaConnectScreen(const std::string& failure=""):cancel(NULL),failure(failure){}
 static std::string connectionFailure(const std::string& identity,bool online){
  if(!online){
   if(identity.empty()||identity.size()>16)return "Use a username with 1 to 16 characters.";
   for(unsigned char c:identity)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'))return "Use letters, numbers or _ in your username.";
  }
  return "Please check the server address and try again.";
 }
 ~JavaConnectScreen(){delete cancel;}
 void init(){cancel=minecraft->useTouchscreen()?static_cast<Button*>(new Touch::TButton(1,failure.empty()?"Cancel":"Back")):new Button(1,failure.empty()?"Cancel":"Back");buttons.push_back(cancel);tabButtons.push_back(cancel);cancel->width=128;cancel->x=(width-128)/2;cancel->y=height/2+16;}
 void render(int x,int y,float partial){renderBackground();Screen::render(x,y,partial);const std::string text=!failure.empty()?"Could not connect":minecraft->javaSession&&minecraft->javaSession->spawned?"Preparing world":"Connecting to server";drawCenteredString(minecraft->font,text,width/2,height/2-32,0xffffffff);if(!failure.empty()){
   std::string line;int y=height/2-12;size_t begin=0;
   while(begin<failure.size()){
    size_t end=failure.find(' ',begin);if(end==std::string::npos)end=failure.size();
    const std::string word=failure.substr(begin,end-begin),candidate=line.empty()?word:line+" "+word;
    if(!line.empty()&&minecraft->font->width(candidate)>width-24){drawCenteredString(minecraft->font,line,width/2,y,0xffffffff);y+=12;line=word;}else line=candidate;
    begin=end+1;
   }
   if(!line.empty())drawCenteredString(minecraft->font,line,width/2,y,0xffffffff);
  }}
 void buttonClicked(Button* button){if(button==cancel){minecraft->stopJavaServer();minecraft->setScreen(new JoinGameScreen());}}
 bool handleBackEvent(bool down){if(down){minecraft->stopJavaServer();minecraft->setScreen(new JoinGameScreen());}return true;}
 bool isInGameScreen(){return false;}
private:Button* cancel;std::string failure;
};
#endif
