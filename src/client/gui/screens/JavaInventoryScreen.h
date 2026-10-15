#ifndef TCPE_JAVA_INVENTORY_SCREEN_H
#define TCPE_JAVA_INVENTORY_SCREEN_H
#include "../Screen.h"
#include "../Font.h"
#include "../components/Button.h"
#include "../components/TextBox.h"
#include "../../../platform/input/Keyboard.h"
#include "../../../platform/input/ControllerActionKeys.h"
#include "../../Minecraft.h"
#include "../../player/LocalPlayer.h"
#include "../../renderer/entity/ItemRenderer.h"
#include "../../../world/item/Item.h"
#include "../../../world/entity/player/Inventory.h"
#include "../../../network/java/JavaGameSession.h"
#include "../../../network/java/JavaItemIcons.h"
#include "../../../network/java/JavaPackItemModels.h"
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include "../../../network/java/JavaRichText.h"
#include "JavaInventoryViewPreference.h"

Screen* makeNativeJavaStorage(int menuId);
// Native popup choices retain exact server slot indices and authoritative clicks.
class JavaPopupChoiceButton:public Touch::TButton{
public:explicit JavaPopupChoiceButton(int id):Touch::TButton(id,""){}
protected:void renderBg(Minecraft* minecraft,int xm,int ym)override{
 Touch::TButton::renderBg(minecraft,xm,ym);
}
};
class JavaInventoryScreen:public Screen{
public:
 explicit JavaInventoryScreen(int id):windowId(id),page(0),half(false),selection(-1),choices(id!=0),details(false),detailPage(0){}
 ~JavaInventoryScreen(){for(Button* b:owned)delete b;}
 int window()const{return windowId;}
 uint64_t generation()const{return boundGeneration;}
 bool semanticActive()const{return height>=148&&semanticMode&&!choices&&!details&&semanticSupported();}
 int semanticSection()const{return section;}
 int mappedSlotForButton(size_t i)const{const size_t count=minecraft&&minecraft->javaSession?minecraft->javaSession->inventory.slots.size():0;if(semanticActive()){size_t at=firstIndex()+i;return at<semanticSlots.size()?int(semanticSlots[at]):-1;}size_t at=firstIndex()+i;return choices?(at<slotMap.size()?int(slotMap[at]):-1):(at<count?int(at):-1);}

 // A server window type identifies its slot geometry, never whether a plugin
 // uses those slots as buttons. Storage is an explicit view of this same menu.
 static bool storageSupported(const JavaGameSession::Inventory& inv){
  int count=0;
  for(int rows=1;rows<=6;++rows)if(inv.menuKind=="generic_9x"+std::to_string(rows))count=rows*9;
  if(inv.menuKind=="generic_3x3")count=9;
  if(inv.menuKind=="hopper")count=5;
  if(inv.menuKind=="shulker_box")count=27;
  auto range=[&](const char* key,int a,int b){auto it=inv.slotRanges.find(key);return it!=inv.slotRanges.end()&&it->second.start==a&&it->second.end==b;};
  return inv.windowId>0&&count>0&&inv.slots.size()==size_t(count+36)&&range("container",0,count)&&range("player_inventory",count,count+27)&&range("hotbar",count+27,count+36);
 }
 static std::string fitTitle(Font& font,const std::string& title,int pixels){if(pixels<=0)return "";if(title.find('\n')==std::string::npos&&font.widthJava(title)<=pixels)return title;const std::string suffix="§r...";const int dots=font.widthJava(suffix);if(pixels<dots)return "";if(pixels==dots)return suffix;auto lines=font.wrapJava(title,pixels-dots);auto first=lines.empty()?"":lines.front();return font.widthJava(first+suffix)<=pixels?first+suffix:suffix;}
 static std::string itemLabel(const JavaGameSession::Item& item){
  if(!item.name.empty()&&item.name!=item.kind)return item.name;
  std::string name=item.resourceId;auto colon=name.find(':');if(colon!=std::string::npos)name=name.substr(colon+1);if(name.empty())name=item.kind;bool upper=true;for(char& c:name){if(c=='_'){c=' ';upper=true;}else if(upper){if(c>='a'&&c<='z')c-=32;upper=false;}}return name;
 }
 static std::string choiceSummary(const JavaGameSession::Item& item){
  std::string result;int count=0;for(const auto& line:item.lore){std::u32string plain;for(const auto& glyph:JavaRichText::parse(line))if(glyph.codepoint>32)plain+=char32_t(glyph.codepoint);if(plain.empty()||plain==U"Kit"||plain==U"Information"||plain==U"Description"||plain==U"ᴅᴇsᴄʀɪᴘᴛɪᴏɴ")continue;if(count++)result+=" §r/ ";result+=line;if(count==2)break;}return result;
 }
 static bool filler(const JavaGameSession::Item& item){
  if(item.resourceId.find("glass_pane")==std::string::npos)return false;
  std::string plain,kind;for(const auto& glyph:JavaRichText::parse(item.name)){unsigned c=glyph.codepoint;if(c>='A'&&c<='Z')c+=32;if((c>='a'&&c<='z')||(c>='0'&&c<='9'))plain+=char(c);}for(unsigned char c:item.kind){if(c>='A'&&c<='Z')c+=32;if((c>='a'&&c<='z')||(c>='0'&&c<='9'))kind+=char(c);}
  if(plain==kind){bool described=false;for(const auto& line:item.lore)for(const auto& glyph:JavaRichText::parse(line))if(glyph.codepoint>32)described=true;if(!described)return true;}
  for(const auto& glyph:JavaRichText::parse(item.name))if(glyph.codepoint!=' '&&glyph.codepoint!='\n'&&glyph.codepoint!='\t')return false;return true;
 }
 void init()override{
  if(!generationBound&&minecraft&&minecraft->javaSession){boundGeneration=minecraft->javaSession->inventory.menuGeneration;generationBound=true;}
  for(Button* b:owned)delete b;owned.clear();buttons.clear();tabButtons.clear();
  previous=make(-1,"Previous");next=make(-2,"Next");toggle=make(-3,half?"Half":"Whole");close=make(-4,"Close");
  for(int i=0;i<54;++i){Button* b=new JavaPopupChoiceButton(i);owned.push_back(b);buttons.push_back(b);tabButtons.push_back(b);}
  for(int i=0;i<9;++i)make(100+i,std::to_string(i+1));drop=make(-5,half?"Drop one":"Drop all");view=make(-6,choices?"Items":"Choices");action=make(-7,"Select");workTab=make(-8,"Craft");bagTab=make(-9,"Bag");gearTab=make(-10,"Gear");hotbarTab=make(-11,"Hotbar");handChoice=make(-12,"Hand: main");setupPositions();
 }
 void setupPositions()override{refreshSlots();layout();refreshSlots();}
 void tick()override{if(!minecraft->javaSession||(minecraft->javaSession->inventory.windowId!=windowId||minecraft->javaSession->inventory.menuGeneration!=boundGeneration)){minecraft->setScreen(NULL);return;}refreshSlots();}
 void render(int x,int y,float partial)override{
  auto* session=minecraft->javaSession;if(!session)return;updateChoiceDrag(y);refreshSlots();layout();
  renderBackground();
  // Menu block icons need a fresh depth buffer, independent of the world behind the popup.
  GLboolean depthWrite=GL_TRUE;glGetBooleanv(GL_DEPTH_WRITEMASK,&depthWrite);glDepthMask(GL_TRUE);glClear(GL_DEPTH_BUFFER_BIT);glDepthMask(depthWrite);
  glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  Font& font=*minecraft->font;const auto& inv=session->inventory;
  const std::string title=inv.title.empty()?"Inventory":inv.title;drawCenteredString(&font,fitTitle(font,title,width-40),width/2,13,0xffffffff);
  if(details&&validSelection()){
   const auto& item=inv.slots[selection];drawItem(item,18,35,24);font.drawShadow(font.fitJava(itemLabel(item),width-76),50,36,0xffffffff);
   const auto lines=description(item);const size_t rows=std::max(1,(height-96)/10);const size_t offset=detailPage*rows;
   for(size_t i=0;i<rows&&offset+i<lines.size();++i)font.drawShadow(lines[offset+i],18,64+i*10,0xffe0e0e0);
  }else if(semanticActive()){
   Screen::render(x,y,partial);glEnable2(GL_BLEND);const size_t cap=capacity();
   const std::string label=section==0?(thermal()?"Smelting":"Crafting"):section==1?"Inventory":section==2?"Armor and offhand":"Hotbar items";drawCenteredString(&font,label,width/2,49,0xffc0c0c0);
   for(size_t i=0;i<cap;++i){int slot=mappedSlotForButton(i);if(slot<0)continue;const Button* b=owned[i+4];if(inv.slots[slot].count>0)drawItem(inv.slots[slot],b->x+1,b->y+1);}
   if(section==0){const auto result=inv.slotRanges.find("result");if(result!=inv.slotRanges.end())for(size_t i=0;i<cap;++i)if(mappedSlotForButton(i)==result->second.start){const Button* b=owned[i+4];font.drawShadow("Result",b->x-4,b->y-11,0xffc0c0c0);font.drawShadow(">",b->x-15,b->y+4,0xffc0c0c0);}
    if(thermal()){for(size_t i=0;i<cap;++i){const Button* b=owned[i+4];if(i<2)font.drawShadow(i==0?"Input":"Fuel",b->x-4,b->y-11,0xffc0c0c0);}auto progress=[&](int value,int total,const char* name,int x){auto a=inv.properties.find(value),b=inv.properties.find(total);if(a==inv.properties.end()||b==inv.properties.end()||a->second<0||b->second<=0)return;int percent=int(std::min(1.,double(a->second)/b->second)*100);font.drawShadow(std::string(name)+" "+std::to_string(percent)+"%",x,height-49,0xffc0c0c0);};progress(0,1,"Burn",12);progress(2,3,"Cook",width-72);}
   }else {int slot=describedSlotAt(x,y);if(slot>=0){const auto& item=inv.slots[slot];std::string name=item.count>0?itemLabel(item):emptyHint(slot);drawCenteredString(&font,font.fitJava(name,width-32),width/2,height-42,0xffe0e0e0);}}
  }else if(choices){
   const size_t cap=capacity();const std::string subtitle=slotMap.empty()?"No choices available":slotMap.size()>cap?"Swipe · Tap to choose · Hold for info":"Tap to choose · Hold for info";drawCenteredString(&font,subtitle,width/2,27,0xffc0c0c0);
   Screen::render(x,y,partial);glEnable2(GL_BLEND);
   for(size_t i=0;i<cap&&firstIndex()+i<slotMap.size();++i){const auto& item=inv.slots[slotMap[firstIndex()+i]];const Button* b=owned[i+4];drawItem(item,b->x+6,b->y+3);const int labelWidth=b->width-34;auto label=font.fitJava(itemLabel(item),labelWidth);font.drawShadow(label,b->x+28,b->y+3,0xffffffff);const auto summary=choiceSummary(item);if(!summary.empty())font.drawShadow(font.fitJava(summary,b->width-12),b->x+6,b->y+17,0xffc0c0c0);}
  }else{
   for(int i=0;i<9;++i)owned[58+i]->selected=inv.selected==unsigned(i);Screen::render(x,y,partial);glEnable2(GL_BLEND);
   const size_t cap=capacity();for(size_t i=0;i<cap&&firstIndex()+i<inv.slots.size();++i){const auto& item=inv.slots[firstIndex()+i];const Button* b=owned[i+4];if(item.count>0)drawItem(item,b->x+(b->width<22?1:3),b->y+(b->height<22?1:3));}
   const int described=describedSlotAt(x,y);if(described>=0&&size_t(described)<inv.slots.size())drawCenteredString(&font,font.fitJava(itemLabel(inv.slots[described]),width-32),width/2,height-42,0xffe0e0e0);
  }
  if(details)Screen::render(x,y,partial);
  if(inv.carried.count>0){glEnable2(GL_BLEND);drawItem(inv.carried,width-32,11);}
 }
 void mouseClicked(int x,int y,int button)override{if(choices&&!details&&button==1&&y>=37&&y<height-35){listDrag=true;dragMoved=false;dragStartY=y;dragStartRow=page;dragStarted=std::chrono::steady_clock::now();}Screen::mouseClicked(x,y,button);}
 void mouseReleased(int x,int y,int button)override{if(listDrag&&button==1){updateChoiceDrag(y);listDrag=false;if(dragMoved||std::chrono::steady_clock::now()-dragStarted>=std::chrono::milliseconds(650)){const int slot=describedSlotAt(x,y);Screen::mouseReleased(-1,-1,button);for(size_t i=0;i<54;++i)owned[i+4]->released(-1,-1);if(!dragMoved&&slot>=0){selection=slot;details=true;detailPage=0;setupPositions();}return;}}Screen::mouseReleased(x,y,button);}
 void buttonClicked(Button* b)override{
  if(!b||!b->visible||!b->active)return;auto* session=minecraft->javaSession;if(!session||session->inventory.windowId!=windowId||session->inventory.menuGeneration!=boundGeneration)return;
  if(b==close){session->closeInventory();minecraft->setScreen(NULL);return;}
  if(b==handChoice){session->useOffHand=!session->useOffHand;setupPositions();return;}
  if(b==drop){session->dropCarried(half);return;}
  if(b==action){if(validSelection())session->clickInventorySlot(selection,false);return;}
  if(b==workTab||b==bagTab||b==gearTab||b==hotbarTab){section=b==workTab?0:b==bagTab?1:b==gearTab?2:3;page=0;selection=-1;setupPositions();return;}
  if(b==view){if(storageSupported(session->inventory)){JavaInventoryViewPreference::setStorage(true);minecraft->setScreen(makeNativeJavaStorage(windowId));return;}if(height>=148&&semanticSupported()){if(semanticMode&&!choices){semanticMode=false;choices=false;}else if(!choices){choices=true;}else{choices=false;semanticMode=true;}}else choices=!choices;details=false;page=0;selection=-1;setupPositions();return;}
  if(b==toggle){half=!half;toggle->msg=half?"Half":"Whole";drop->msg=half?"Drop one":"Drop all";return;}
  if(b==previous){if(details){details=false;detailPage=0;}else if(page)page=choices?(page>capacity()?page-capacity():0):page-1;setupPositions();return;}
  if(b==next){if(details)++detailPage;else page+=choices?capacity():1;refreshSlots();return;}
  if(b->id>=100&&b->id<109){session->connection.selectHotbar(b->id-100);if(minecraft->player)minecraft->player->inventory->selectSlot(b->id-100);return;}
  for(size_t i=0;i<54;++i)if(b==owned[i+4]){if(semanticActive()){const int slot=mappedSlotForButton(i);if(slot>=0){selection=slot;session->clickInventorySlot(slot,half);}return;}const size_t index=firstIndex()+i;if(choices){if(index<slotMap.size()){selection=slotMap[index];session->clickInventorySlot(selection,false);}}else if(index<session->inventory.slots.size()){selection=index;session->clickInventorySlot(index,half);}return;}
 }
 void keyPressed(int key)override{
  bool editing=false;for(auto* box:textBoxes)if(box->focused)editing=true;
  if(key==Keyboard::KEY_ESCAPE||key==ControllerKeys::MenuCancel||(!editing&&key&&key==minecraft->options.keyMenuCancel.key)){buttonClicked(close);return;}
  Screen::keyPressed(key);
 }
 bool handleBackEvent(bool down)override{if(down){if(details){details=false;setupPositions();}else buttonClicked(close);}return true;}
 bool isPauseScreen()override{return false;}bool isInGameScreen()override{return true;}bool closeOnPlayerHurt()override{return false;}
 int describedSlotAt(int x,int y)const{for(size_t i=0;i<capacity();++i){const Button* b=owned[i+4];if(b->visible&&b->active&&x>=b->x&&y>=b->y&&x<b->x+b->width&&y<b->y+b->height)return mappedSlotForButton(i);}return selection;}

 static ItemInstance mapItem(const JavaGameSession::Item& item){const int id=item.originalId;if(item.count<=0||id<=0||id>=Item::MAX_ITEMS||!Item::items[id])return ItemInstance();return ItemInstance(id,std::min(254,item.count),item.originalData);}
private:
 int windowId;size_t page;bool half;int selection;bool choices,details;size_t detailPage;std::vector<size_t> slotMap;std::vector<Button*> owned;Button *previous,*next,*toggle,*close,*drop,*view,*action;
 uint64_t boundGeneration=0;bool generationBound=false,metadataSeen=false,semanticMode=true;int section=1;std::vector<size_t> semanticSlots;Button *handChoice=nullptr;Button *workTab=nullptr,*bagTab=nullptr,*gearTab=nullptr,*hotbarTab=nullptr;
 bool thermal()const{if(!minecraft||!minecraft->javaSession)return false;const auto& kind=minecraft->javaSession->inventory.menuKind;return kind=="furnace"||kind=="blast_furnace"||kind=="smoker";}
 bool knownSemanticKind()const{if(!minecraft||!minecraft->javaSession)return false;const auto& kind=minecraft->javaSession->inventory.menuKind;return kind=="player"||kind=="crafting"||thermal();}
 bool semanticSupported()const{if(!knownSemanticKind())return false;const auto& inv=minecraft->javaSession->inventory;auto range=[&](const char* role,int start,int end){auto r=inv.slotRanges.find(role);return r!=inv.slotRanges.end()&&r->second.start==start&&r->second.end==end&&size_t(end)<=inv.slots.size();};if(inv.menuKind=="player")return range("crafting",1,5)&&range("result",0,1)&&range("player_inventory",9,36)&&range("hotbar",36,45)&&range("armor",5,9)&&range("offhand",45,46);if(inv.menuKind=="crafting")return range("crafting",1,10)&&range("result",0,1)&&range("player_inventory",10,37)&&range("hotbar",37,46);return range("ingredient",0,1)&&range("fuel",1,2)&&range("result",2,3)&&range("player_inventory",3,30)&&range("hotbar",30,39);}
 std::string emptyHint(int slot)const{if(!minecraft||!minecraft->javaSession)return "Empty slot";const auto& ranges=minecraft->javaSession->inventory.slotRanges;for(const auto& role:ranges)if(slot>=role.second.start&&slot<role.second.end){if(role.first=="fuel")return "Fuel slot";if(role.first=="ingredient")return "Ingredient slot";if(role.first=="result")return "Result slot";if(role.first=="crafting")return "Crafting input";if(role.first=="offhand")return "Offhand slot";}return "Empty slot";}
 bool listDrag=false,dragMoved=false;int dragStartY=0;size_t dragStartRow=0;std::chrono::steady_clock::time_point dragStarted;
 size_t firstIndex()const{return choices?page:page*capacity();}
 void updateChoiceDrag(int y){if(!listDrag||!choices||details||y<0||y>=height)return;const int delta=dragStartY-y;if(std::abs(delta)>=6)dragMoved=true;if(dragMoved){const size_t cap=capacity();const int limit=slotMap.size()>cap?int(slotMap.size()-cap):0;page=std::max(0,std::min(limit,int(dragStartRow)+delta/29));}}
 bool validSelection()const{return minecraft&&minecraft->javaSession&&selection>=0&&size_t(selection)<minecraft->javaSession->inventory.slots.size()&&minecraft->javaSession->inventory.slots[selection].count>0;}
 std::vector<std::string> description(const JavaGameSession::Item& item)const{std::vector<std::string> lines;for(const auto& text:item.lore){auto wrapped=minecraft->font->wrapJava(text,width-40);lines.insert(lines.end(),wrapped.begin(),wrapped.end());}if(lines.empty())lines.push_back("Select this item to continue.");return lines;}
 void drawItem(const JavaGameSession::Item& item,int x,int y,int size=16){
  const std::string model=item.itemModel.empty()?item.resourceId:item.itemModel;
  if(!JavaPackItemModels::render(minecraft->textures,model,x,y,size,&item)&&!JavaItemIcons::render(minecraft->textures,item.resourceId,x,y,size,&item)){auto native=item.hasProfile&&item.resourceId=="minecraft:player_head"?ItemInstance():mapItem(item);if(!native.isNull()){glPushMatrix2();glTranslatef2(x,y,0);glScalef2(size/16.f,size/16.f,1);ItemRenderer::renderGuiItem(minecraft->font,minecraft->textures,&native,0,0,true);glPopMatrix2();}else drawString(minecraft->font,"?",x+4,y+2,0xffffcc55);}
  if(item.count>1){auto count=std::to_string(item.count);minecraft->font->drawShadow(count,x+size-minecraft->font->width(count),y+size-7,0xffffffff);}
 }
 void layout(){
  drop->x=12;drop->y=height-53;drop->width=64;drop->height=20;
  const bool panel=semanticActive();std::vector<Button*> tabs{workTab,bagTab,hotbarTab};if(minecraft&&minecraft->javaSession&&minecraft->javaSession->inventory.menuKind=="player")tabs.push_back(gearTab);const int tw=std::max(1,(width-24-int(tabs.size()-1)*4)/int(tabs.size()));for(size_t i=0;i<tabs.size();++i){tabs[i]->x=12+int(i)*(tw+4);tabs[i]->y=27;tabs[i]->width=tw;tabs[i]->height=20;tabs[i]->selected=(tabs[i]==workTab&&section==0)||(tabs[i]==bagTab&&section==1)||(tabs[i]==gearTab&&section==2)||(tabs[i]==hotbarTab&&section==3);}workTab->msg=thermal()?"Smelt":"Craft";

  std::vector<Button*> footer=details?std::vector<Button*>{previous,next,action,close}:choices?std::vector<Button*>{previous,next,view,close}:std::vector<Button*>{previous,next,toggle,view,close};if(!choices&&!details&&!panel&&height<130)footer={previous,next,close};if(panel&&section==0){footer={toggle,view,close};if(minecraft->javaSession->inventory.carried.count>0)footer.insert(footer.begin(),drop);}if(panel&&section==2&&windowId==0)footer={handChoice,view,close};const int space=6,w=std::max(1,(width-24-space*(int(footer.size())-1))/int(footer.size()));for(size_t i=0;i<footer.size();++i){footer[i]->x=12+i*(w+space);footer[i]->y=height-29;footer[i]->width=w;footer[i]->height=20;}
  handChoice->msg=w<70?(minecraft->javaSession->useOffHand?"Off":"Main"):(minecraft->javaSession->useOffHand?"Hand: off":"Hand: main");handChoice->selected=minecraft->javaSession->useOffHand;
  previous->msg=details?"Back":w<56?"Prev":"Previous";next->msg=details?"More":"Next";view->msg=storageSupported(minecraft->javaSession->inventory)?"Storage":panel?"Items":(height>=148&&semanticSupported())?(choices?"Panel":"Choices"):(choices?"Items":"Choices");
  const int cols=columns();if(choices){const int gap=6,w=(width-32-gap*(cols-1))/cols;for(int i=0;i<54;++i){auto* b=owned[i+4];b->x=16+(i%cols)*(w+gap);b->y=37+(i/cols)*29;b->width=w;b->height=27;}}
  else{const int left=(width-cols*24)/2;for(int i=0;i<54;++i){auto* b=owned[i+4];b->x=left+(i%cols)*24;b->y=58+(i/cols)*24;b->width=b->height=22;}}
  if(panel){const int cols=columns(),left=(width-cols*20)/2;for(int i=0;i<54;++i){auto* b=owned[i+4];b->x=left+(i%cols)*20;b->y=64+(i/cols)*20;b->width=b->height=18;}
   if(section==0){const int cells=thermal()?2:(minecraft->javaSession->inventory.menuKind=="player"?2:3),count=thermal()?2:cells*cells;const int left=(width-(thermal()?110:cells*20+40))/2;for(int i=0;i<count;++i){auto* b=owned[i+4];b->x=left+(thermal()?i*42:(i%cells)*20);b->y=thermal()?76:60+(i/cells)*20;}auto* output=owned[count+4];output->x=left+(thermal()?92:cells*20+22);output->y=thermal()?76:60+(cells*20-18)/2;}
  }
  if(!choices&&!details&&!panel&&height<130){std::vector<Button*> extra{toggle,view};if(minecraft->javaSession->inventory.carried.count>0)extra.push_back(drop);const int cw=(width-24-int(extra.size()-1)*4)/int(extra.size());for(size_t i=0;i<extra.size();++i){extra[i]->x=12+int(i)*(cw+4);extra[i]->y=26;extra[i]->width=cw;extra[i]->height=18;}view->msg="Choices";drop->msg="Drop";const int cols=columns(),left=(width-cols*20)/2;for(int i=0;i<54;++i){auto* b=owned[i+4];b->x=left+(i%cols)*20;b->y=46+(i/cols)*20;b->width=b->height=18;}}
  const int hw=std::min(24,(width-24)/9),left=(width-hw*9)/2;for(int i=0;i<9;++i){auto* b=owned[58+i];b->x=left+i*hw;b->y=32;b->width=hw-2;b->height=20;b->visible=b->active=!choices&&!details&&!panel&&height>=130;}
 }
 void refreshSlots(){
  if(!minecraft||!minecraft->javaSession)return;const auto& inv=minecraft->javaSession->inventory;
  if(!metadataSeen&&knownSemanticKind()){metadataSeen=true;choices=false;semanticMode=semanticSupported();section=inv.menuKind=="player"?1:0;page=0;}
  semanticSlots.clear();auto addRange=[&](const char* role){auto r=inv.slotRanges.find(role);if(r!=inv.slotRanges.end())for(int i=r->second.start;i<r->second.end;++i)if(i>=0&&size_t(i)<inv.slots.size())semanticSlots.push_back(size_t(i));};if(semanticActive()){if(section==0){if(thermal()){addRange("ingredient");addRange("fuel");}else addRange("crafting");addRange("result");}else if(section==1)addRange("player_inventory");else if(section==2){addRange("armor");addRange("offhand");}else addRange("hotbar");}
  slotMap.clear();const size_t end=windowId!=0&&inv.slots.size()>=36?inv.slots.size()-36:inv.slots.size();for(size_t i=0;i<end;++i)if(inv.slots[i].count>0&&!filler(inv.slots[i]))slotMap.push_back(i);
  if(details&&!validSelection())details=false;const size_t cap=capacity(),count=semanticActive()?semanticSlots.size():choices?slotMap.size():inv.slots.size();if(choices)page=std::min(page,count>cap?count-cap:0);else if(firstIndex()>=count)page=count?(count-1)/cap:0;
  const size_t rows=std::max(1,(height-96)/10);const size_t detailCount=details?description(inv.slots[selection]).size():0;if(detailPage*rows>=detailCount)detailPage=detailCount?(detailCount-1)/rows:0;
  previous->visible=next->visible=close->visible=true;previous->active=details||page>0;next->active=details?(detailPage+1)*rows<detailCount:firstIndex()+cap<count;
  toggle->visible=toggle->active=!details&&!choices;view->visible=view->active=!details;action->visible=details;action->active=details&&validSelection();drop->visible=drop->active=!choices&&!details&&inv.carried.count>0;
  const bool panel=semanticActive();handChoice->visible=panel&&section==2&&windowId==0&&!details;handChoice->active=handChoice->visible&&inv.offHand.count>0;workTab->visible=bagTab->visible=hotbarTab->visible=panel;workTab->active=bagTab->active=hotbarTab->active=panel;gearTab->visible=gearTab->active=panel&&inv.menuKind=="player";if(panel&&section==2){previous->visible=next->visible=toggle->visible=false;}if(panel&&section==0){previous->visible=next->visible=false;drop->visible=drop->active=inv.carried.count>0;}
  for(size_t i=0;i<54;++i){auto* b=owned[i+4];b->visible=b->active=!details&&i<cap&&firstIndex()+i<count;}
 }
 int columns()const{if(semanticActive())return std::max(1,std::min(9,(width-24)/20));return choices?1:std::max(1,std::min(9,(width-16)/(height<130?20:24)));}
 size_t capacity()const{if(semanticActive()){if(section==0)return semanticSlots.size();return std::min(54,columns()*std::max(1,(height-110)/20));}return choices?std::min(54,columns()*std::max(1,(height-70)/29)):height<130?std::min(54,columns()*std::max(1,(height-86)/20)):std::min(54,columns()*std::max(1,(height-116)/24));}
 Button* make(int id,const std::string& label){Button* b=minecraft->useTouchscreen()?static_cast<Button*>(new Touch::TButton(id,label)):new Button(id,label);owned.push_back(b);buttons.push_back(b);tabButtons.push_back(b);return b;}
};
#include "JavaCraftingScreen.h"
#include "JavaPlayerInventoryScreen.h"
#include "JavaStorageScreen.h"
inline Screen* makeNativeJavaStorage(int menuId){return new JavaStorageScreen(menuId);}
inline Screen* makeNativeJavaPlayerInventory(bool crafting,int menuId){return new JavaPlayerInventoryScreen(crafting,menuId);}
// Call when a new authoritative snapshot arrives. The source HUD reads linked slots.
inline void applyJavaHotbar(Minecraft* game){if(!game||!game->javaSession||!game->player||!game->player->inventory)return;auto& snapshot=game->javaSession->inventory;Inventory* inventory=game->player->inventory;for(int i=0;i<9;i++){ItemInstance native=JavaInventoryScreen::mapItem(snapshot.hotbar[i]);inventory->replaceSlot(9+i,native.isNull()?NULL:&native);inventory->linkedSlots[i].inventorySlot=9+i;}inventory->selected=std::min(8u,snapshot.selected);for(int i=0;i<4;i++){ItemInstance armor=JavaInventoryScreen::mapItem(snapshot.armor[i]);game->player->setArmor(i,armor.isNull()?NULL:&armor);}}
// Opening window zero is explicit; server-opened windows should use this helper.
inline bool showJavaInventory(Minecraft* game,bool userRequested=false){if(!game||!game->javaSession||!game->level||game->javaSession->dead)return false;const int id=game->javaSession->inventory.windowId;if(id==0&&!userRequested)return false;if(game->javaSession->dialog.pending)return false;if(id==0){auto* crafting=dynamic_cast<JavaCraftingScreen*>(game->screen);if(crafting&&crafting->window()==id&&crafting->generation()==game->javaSession->inventory.menuGeneration)return true;auto* current=dynamic_cast<JavaPlayerInventoryScreen*>(game->screen);if(!current||current->generation()!=game->javaSession->inventory.menuGeneration)game->setScreen(new JavaPlayerInventoryScreen());return true;}if(game->javaSession->inventory.menuKind=="crafting"){auto* manual=dynamic_cast<JavaPlayerInventoryScreen*>(game->screen);if(manual&&manual->window()==id&&manual->generation()==game->javaSession->inventory.menuGeneration)return true;auto* crafting=dynamic_cast<JavaCraftingScreen*>(game->screen);if(!crafting||crafting->window()!=id||crafting->generation()!=game->javaSession->inventory.menuGeneration)game->setScreen(new JavaCraftingScreen(id));return true;}if(JavaInventoryViewPreference::storage()&&JavaInventoryScreen::storageSupported(game->javaSession->inventory)){auto* current=dynamic_cast<JavaStorageScreen*>(game->screen);if(!current||current->window()!=id||current->generation()!=game->javaSession->inventory.menuGeneration)game->setScreen(new JavaStorageScreen(id));return true;}auto* storage=dynamic_cast<JavaStorageScreen*>(game->screen);if(storage&&storage->window()==id&&storage->generation()==game->javaSession->inventory.menuGeneration&&JavaInventoryScreen::storageSupported(game->javaSession->inventory))return true;JavaInventoryScreen* current=dynamic_cast<JavaInventoryScreen*>(game->screen);if(!current||current->window()!=id||current->generation()!=game->javaSession->inventory.menuGeneration)game->setScreen(new JavaInventoryScreen(id));return true;}
#endif
