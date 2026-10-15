#ifndef TCPE_JAVA_DIALOG_SCREEN_H
#define TCPE_JAVA_DIALOG_SCREEN_H
#include "../Screen.h"
#include "../Font.h"
#include "../components/Button.h"
#include "../../Minecraft.h"
#include "JavaConnectScreen.h"
#include "JoinGameScreen.h"
#include "../../../network/java/JavaGameSession.h"
#include <sstream>
#include <algorithm>
// Copies the current dialog so network polling cannot invalidate displayed text.
class JavaDialogScreen:public Screen {
public:
 explicit JavaDialogScreen(const JavaGameSession::Dialog& dialog):content(dialog),page(0),actionPage(0){}
 ~JavaDialogScreen(){for(Button* button:owned)delete button;}
 unsigned revision()const{return content.revision;}
 void tick(){if(minecraft->javaSession&&!minecraft->javaSession->dialog.pending)minecraft->setScreen(minecraft->level?NULL:new JavaConnectScreen());}
 void init(){
  for(Button* button:owned)delete button;owned.clear();buttons.clear();tabButtons.clear();
  previous=button(-1,"Previous");next=button(-2,"Next");exit=button(-3,"Disconnect");
  for(size_t i=0;i<content.actions.size();i++)button(static_cast<int>(i),content.actions[i].label);
  rebuildLines();setupPositions();
 }
 void setupPositions(){
  const int bottom=height-26;previous->x=8;previous->y=bottom-26;previous->width=72;
  next->x=width-80;next->y=bottom-26;next->width=72;
  exit->x=width-96;exit->y=bottom;exit->width=88;
  const size_t visible=std::max(1,(width-112)/104);const size_t start=actionPage*visible;
  for(size_t i=0;i<content.actions.size();i++){Button* b=owned[i+3];b->visible=i>=start&&i<start+visible;b->active=b->visible;b->width=100;b->x=8+static_cast<int>(i-start)*104;b->y=bottom;}
  previous->active=page>0||actionPage>0;
  next->active=(page+1)*pageSize()<lines.size()||(actionPage+1)*visible<content.actions.size();
 }
 void render(int x,int y,float partial){renderBackground();drawCenteredString(minecraft->font,content.title.empty()?"Server message":content.title,width/2,12,0xffffffff);
  const size_t start=page*pageSize();for(size_t i=0;i<pageSize()&&start+i<lines.size();i++)drawString(minecraft->font,lines[start+i],10,32+static_cast<int>(i)*10,0xffffffff);
  Screen::render(x,y,partial);
 }
 void buttonClicked(Button* b){if(!b||!b->visible||!b->active)return;
  if(b==exit){minecraft->stopJavaServer();minecraft->setScreen(new JoinGameScreen());return;}
  const size_t visible=std::max(1,(width-112)/104);
  if(b==previous){if(page)page--;if(actionPage)actionPage--;setupPositions();return;}
  if(b==next){if((page+1)*pageSize()<lines.size())page++;if((actionPage+1)*visible<content.actions.size())actionPage++;setupPositions();return;}
  for(size_t i=0;i<content.actions.size();i++)if(b==owned[i+3]){if(minecraft->javaSession&&minecraft->javaSession->dialog.revision==content.revision&&minecraft->javaSession->chooseDialogAction(i))minecraft->setScreen(minecraft->level?NULL:new JavaConnectScreen());return;}
 }
 bool handleBackEvent(bool down){if(down)buttonClicked(exit);return true;}
 bool isPauseScreen(){return false;}
 bool isInGameScreen(){return false;}
 bool closeOnPlayerHurt(){return false;}
private:
 JavaGameSession::Dialog content;std::vector<Button*> owned;std::vector<std::string> lines;Button *previous,*next,*exit;size_t page,actionPage;
 Button* button(int id,const std::string& label){Button* b=minecraft->useTouchscreen()?static_cast<Button*>(new Touch::TButton(id,label)):new Button(id,label);owned.push_back(b);buttons.push_back(b);tabButtons.push_back(b);return b;}
 size_t pageSize()const{return static_cast<size_t>(std::max(1,(height-96)/10));}
 void rebuildLines(){lines.clear();std::istringstream paragraphs(content.body);std::string paragraph;while(std::getline(paragraphs,paragraph)){std::istringstream words(paragraph);std::string word,line;while(words>>word){const std::string candidate=line.empty()?word:line+" "+word;if(!line.empty()&&minecraft->font->width(candidate)>width-20){lines.push_back(line);line=word;}else line=candidate;}lines.push_back(line);}}
};
// Invoke after pumping server events, including before the play world is ready.
inline bool showPendingJavaDialog(Minecraft* game){
 if(!game||!game->javaSession||!game->javaSession->dialog.pending)return false;
 JavaDialogScreen* current=dynamic_cast<JavaDialogScreen*>(game->screen);
 if(!current||current->revision()!=game->javaSession->dialog.revision)game->setScreen(new JavaDialogScreen(game->javaSession->dialog));
 return true;
}
#endif
