#ifndef TCPE_JAVA_CHAT_ACTIONS_SCREEN_H
#define TCPE_JAVA_CHAT_ACTIONS_SCREEN_H
#include "../Screen.h"
#include "../Font.h"
#include "../components/Button.h"
#include "../../Minecraft.h"
#include "../../../network/java/JavaGameSession.h"
#include <algorithm>
#include <sstream>
// Server run_command links become native buttons; no action is sent automatically.
class JavaChatActionsScreen:public Screen {
public:
 explicit JavaChatActionsScreen(const JavaGameSession& session):actions(session.chatActions),context(session.chatActionContext),generation(session.chatActionsRevision),page(0){}
 ~JavaChatActionsScreen(){for(Button* b:owned)delete b;}
 unsigned revision()const{return generation;}
 void init(){for(Button* b:owned)delete b;owned.clear();buttons.clear();tabButtons.clear();for(size_t i=0;i<actions.size();i++)make(static_cast<int>(i),actions[i].label);previous=make(-1,"Previous");next=make(-2,"Next");close=make(-3,"Close");setupPositions();wrapContext();}
 void setupPositions(){const int w=std::min(240,width-24);size_t cap=capacity();for(size_t i=0;i<actions.size();i++){Button* b=owned[i];b->visible=i>=page*cap&&i<(page+1)*cap;b->active=b->visible;b->x=(width-w)/2;b->y=86+static_cast<int>(i-page*cap)*24;b->width=w;std::string label=actions[i].label;while(label.size()>1&&minecraft->font->width(label)>w-12){size_t end=label.size()-1;while(end>0&&(static_cast<unsigned char>(label[end])&0xc0)==0x80)--end;label.erase(end);}b->msg=label;}previous->x=8;previous->y=height-26;previous->width=72;next->x=width-80;next->y=height-26;next->width=72;close->x=(width-72)/2;close->y=height-26;close->width=72;previous->active=page>0;next->active=(page+1)*cap<actions.size();}
 void render(int x,int y,float partial){renderBackground();drawCenteredString(minecraft->font,"Server message",width/2,12,0xffffffff);for(size_t i=0;i<lines.size()&&i<5;i++)drawString(minecraft->font,lines[i],12,30+static_cast<int>(i)*10,0xffffffff);Screen::render(x,y,partial);}
 void tick(){if(!minecraft->javaSession||!minecraft->javaSession->chatActionsPending)minecraft->setScreen(NULL);}
 void buttonClicked(Button* b){if(!b||!b->visible||!b->active)return;if(b==previous){if(page)page--;setupPositions();return;}if(b==next){if((page+1)*capacity()<actions.size())page++;setupPositions();return;}JavaGameSession* session=minecraft->javaSession;if(!session)return;if(b==close){if(session->chatActionsRevision==generation)session->chatActionsPending=false;minecraft->setScreen(NULL);return;}for(size_t i=0;i<actions.size();i++)if(b==owned[i]){if(session->chatActionsRevision==generation&&session->chooseChatAction(i))minecraft->setScreen(NULL);return;}}
 bool handleBackEvent(bool down){if(down)buttonClicked(close);return true;}bool isPauseScreen(){return false;}bool isInGameScreen(){return true;}bool closeOnPlayerHurt(){return false;}
private:
 std::vector<JavaGameSession::ChatAction> actions;std::string context;unsigned generation;size_t page;std::vector<std::string> lines;std::vector<Button*> owned;Button *previous,*next,*close;
 size_t capacity()const{return static_cast<size_t>(std::max(1,(height-122)/24));}
 Button* make(int id,const std::string& text){Button* b=minecraft->useTouchscreen()?static_cast<Button*>(new Touch::TButton(id,text)):new Button(id,text);owned.push_back(b);buttons.push_back(b);tabButtons.push_back(b);return b;}
 void wrapContext(){lines.clear();std::istringstream words(context);std::string word,line;while(words>>word){std::string next=line.empty()?word:line+" "+word;if(!line.empty()&&minecraft->font->width(next)>width-24){lines.push_back(line);line=word;}else line=next;if(lines.size()>=5)break;}if(lines.size()<5&&!line.empty())lines.push_back(line);}
};
// Refresh a user-opened action screen; ordinary chat links never interrupt play.
inline bool showPendingJavaChatActions(Minecraft* game){if(!game||!game->javaSession||!game->level||!game->javaSession->chatActionsPending||game->javaSession->dialog.pending)return false;JavaChatActionsScreen* current=dynamic_cast<JavaChatActionsScreen*>(game->screen);if(!current)return false;if(current->revision()!=game->javaSession->chatActionsRevision)game->setScreen(new JavaChatActionsScreen(*game->javaSession));return true;}
#endif
