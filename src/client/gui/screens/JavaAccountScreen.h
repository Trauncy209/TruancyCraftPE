#ifndef TCPE_JAVA_ACCOUNT_SCREEN_H
#define TCPE_JAVA_ACCOUNT_SCREEN_H
#include "../Screen.h"
#include "../Font.h"
#include "../components/Button.h"
#include "../../Minecraft.h"
#include "OptionsScreen.h"
#include "../../../network/java/JavaGameSession.h"
#include "tcpe_java_network.h"
#include <algorithm>
#include "../../../platform/input/Keyboard.h"
#include "../../../platform/input/ControllerActionKeys.h"
namespace JavaAccountSelection {
inline bool& selected(){static bool value=false;return value;}
inline std::string& username(){static std::string value;return value;}
}
class JavaAccountScreen:public Screen {
 bool returnToPause;TcpeJavaAccountStatus status{};std::string localMessage;
 Touch::THeader header;Touch::TButton signIn,importExisting,useProfile,useUsername,signOut,openPage,cancel,back;
 std::string field(const unsigned char* p)const{return std::string(reinterpret_cast<const char*>(p));}
public:
 explicit JavaAccountScreen(bool pause=false):returnToPause(pause),header(0,"Minecraft Account"),signIn(1,"Microsoft sign-in"),importExisting(2,"Use Prism session"),useProfile(3,"Choose profile"),useUsername(4,"Use username"),signOut(5,"Sign out"),openPage(6,"Open Microsoft page"),cancel(7,"Cancel sign-in"),back(8,"Back"){}
 void init()override{buttons={&header,&signIn,&importExisting,&useProfile,&useUsername,&signOut,&openPage,&cancel,&back};tabButtons={&signIn,&importExisting,&useProfile,&useUsername,&signOut,&openPage,&cancel,&back};tcpe_java_account_status(&status);setupPositions();}
 void setupPositions()override{header.x=0;header.y=0;header.width=width;header.xText=width/2;const bool waiting=status.phase==1;std::vector<Button*> actions=waiting?std::vector<Button*>{&openPage,&cancel}:std::vector<Button*>{&signIn,&importExisting,&useProfile,&useUsername,&signOut};
#if defined(__ANDROID__) || defined(ANDROID)
 actions.erase(std::remove(actions.begin(),actions.end(),&importExisting),actions.end());importExisting.active=false;
#endif
for(Button* b:std::vector<Button*>{&signIn,&importExisting,&useProfile,&useUsername,&signOut,&openPage,&cancel})b->visible=false;const int cols=!waiting&&height<148?3:2;const int w=std::max(32,std::min(150,(width-20-(cols-1)*4)/cols)),left=(width-cols*w-(cols-1)*4)/2,start=height-23-int((actions.size()+cols-1)/cols)*22;for(size_t i=0;i<actions.size();++i){auto* b=actions[i];b->x=left+int(i%cols)*(w+4);b->y=start+int(i/cols)*22;b->width=w;b->height=20;b->visible=true;}back.x=left;back.y=height-23;back.width=cols*w+(cols-1)*4;back.height=20;openPage.active=openPage.visible&&status.verification_uri[0]!=0;cancel.active=cancel.visible&&waiting;importExisting.active=importExisting.visible&&!waiting;useUsername.active=useUsername.visible&&!waiting;useProfile.active=useProfile.visible&&status.available;signOut.active=signOut.visible&&status.username[0]!=0;signIn.active=signIn.visible&&!waiting;useProfile.msg=width<260?"Choose profile":status.username[0]?"Use "+field(status.username):"Choose profile";signIn.msg=width<260?"Sign in":"Microsoft sign-in";openPage.msg=width<260?"Open page":"Microsoft page";useUsername.msg="Use username";importExisting.msg="Prism session";for(auto* b:actions)b->msg=minecraft->font->fitJava(b->msg,b->width-8);useProfile.selected=JavaAccountSelection::selected();useUsername.selected=!JavaAccountSelection::selected();}

 void tick()override{TcpeJavaAccountStatus next{};if(tcpe_java_account_status(&next)&&next.revision!=status.revision){status=next;localMessage.clear();setupPositions();}}
 void render(int x,int y,float a)override{renderBackground();Screen::render(x,y,a);Font& font=*minecraft->font;int yy=27;auto text=[&](const std::string& line){for(const auto& row:font.wrapJava(line,width-24)){const int bottom=(status.phase==1?openPage.y:signIn.y)-4;if(yy+9>bottom)break;const float xx=(width-font.widthJava(row))/2.0f;font.drawJava(row,xx+1,yy+1,0xffffffff,true);font.drawJava(row,xx,yy,0xffffffff);yy+=10;}};text(JavaAccountSelection::selected()?"Selected: "+JavaAccountSelection::username():"Username: "+minecraft->options.username);if(status.username[0]&&!status.user_code[0])text("Profile: "+field(status.username));text(localMessage.empty()?field(status.message):localMessage);if(status.user_code[0]){text("Code: "+field(status.user_code));text(field(status.verification_uri));}}
 void buttonClicked(Button* b)override{if(!b||!b->visible||!b->active)return;if(b==&back){minecraft->setScreen(new OptionsScreen(returnToPause));return;}if(b==&signIn)tcpe_java_account_action(1);else if(b==&cancel)tcpe_java_account_action(0);else if(b==&importExisting){if(!tcpe_java_account_action(3))localMessage="No valid Prism session available. Sign in with Microsoft";}else if(b==&signOut){tcpe_java_account_action(2);if(minecraft->javaSession&&minecraft->javaSession->connection.usesAccount())minecraft->javaSession->connection.send("{\"type\":\"disconnect\"}");JavaAccountSelection::selected()=false;JavaAccountSelection::username().clear();}else if(b==&useProfile&&status.available){JavaAccountSelection::selected()=true;JavaAccountSelection::username()=field(status.username);localMessage="Servers will use this Minecraft profile";}else if(b==&useUsername){JavaAccountSelection::selected()=false;JavaAccountSelection::username().clear();localMessage="Servers will use your TCPE username";}else if(b==&openPage){const auto url=field(status.verification_uri);if(url.find("https://")==0)minecraft->platform()->openURL(url);}tcpe_java_account_status(&status);setupPositions();}
 void keyPressed(int key)override{if(key==Keyboard::KEY_ESCAPE||key==ControllerKeys::MenuCancel||(key&&key==minecraft->options.keyMenuCancel.key)){buttonClicked(&back);return;}Screen::keyPressed(key);}
 bool handleBackEvent(bool down)override{if(!down)buttonClicked(&back);return true;}
 bool isPauseScreen()override{return false;}bool isInGameScreen()override{return returnToPause;}
};
#endif
