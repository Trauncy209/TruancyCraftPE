#ifndef TCPE_JAVA_STORAGE_SCREEN_H
#define TCPE_JAVA_STORAGE_SCREEN_H
#include "../components/InventoryPane.h"
#include "../components/NinePatch.h"
// The original PE chest drawer, backed by authoritative Java slots. Changing
// presentation sends no close/open/click packet and never classifies a plugin.
class JavaStorageScreen:public Screen,public Touch::IInventoryPaneCallback {
 class Pane:public Touch::InventoryPane {
  JavaStorageScreen* owner;bool bag;
 public:Pane(JavaStorageScreen* o,bool b,const IntRectangle& r,int cell,int count):Touch::InventoryPane(o,o->minecraft,r,r.w,0,count,cell,4),owner(o),bag(b){fillMarginX=fillMarginY=0;}
  bool isBag()const{return bag;}
  void renderBatch(std::vector<GridItem>& cells,float alpha)override{
   Touch::InventoryPane::renderBatch(cells,alpha);
   glEnable2(GL_SCISSOR_TEST);glScissor(int(screenScale*bbox.x),mc->height-int(screenScale*(bbox.y+bbox.h)),int(screenScale*bbox.w),int(screenScale*bbox.h));
   for(const auto& cell:cells){int slot=owner->slotAt(bag,cell.id);if(slot>=0){const auto& item=mc->javaSession->inventory.slots[slot];if(item.count>0)owner->drawItem(item,int(cell.xf)+(itemBbox.w-16)/2,int(cell.yf)+(itemBbox.h-16)/2);}}
   glDisable2(GL_SCISSOR_TEST);
  }
 };
 int windowId;uint64_t boundGeneration=0;Pane *bagPane=nullptr,*storagePane=nullptr;
 Touch::THeader bagHeader,storageHeader;ImageButton close;Touch::TButton menu,transfer;
 NinePatchLayer *background=nullptr,*frame=nullptr;std::vector<int> bagSlots,containerSlots;int transferMode=0;
 bool valid()const{return minecraft&&minecraft->javaSession&&minecraft->javaSession->inventory.windowId==windowId&&minecraft->javaSession->inventory.menuGeneration==boundGeneration&&JavaInventoryScreen::storageSupported(minecraft->javaSession->inventory);}
 void drawItem(const JavaGameSession::Item& item,int x,int y){
  const auto model=item.itemModel.empty()?item.resourceId:item.itemModel;
  if(!JavaPackItemModels::render(minecraft->textures,model,x,y,16,&item)&&!JavaItemIcons::render(minecraft->textures,item.resourceId,x,y,16,&item)){auto native=JavaInventoryScreen::mapItem(item);if(!native.isNull())ItemRenderer::renderGuiItem(minecraft->font,minecraft->textures,&native,x,y,true);else drawString(minecraft->font,"?",x+4,y+2,0xffffcc55);}
  glEnable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);if(item.count>1){auto n=std::to_string(item.count);minecraft->font->drawShadow(n,x+16-minecraft->font->width(n),y+9,0xffffffff);}
 }
public:
 explicit JavaStorageScreen(int id):windowId(id),bagHeader(1,"Inventory"),storageHeader(2,"Storage"),close(3,""),menu(4,"Menu"),transfer(5,"Whole"){}
 ~JavaStorageScreen(){delete bagPane;delete storagePane;delete background;delete frame;}
 int window()const{return windowId;}uint64_t generation()const{return boundGeneration;}
 const Touch::InventoryPane* paneFor(bool bag)const{return bag?bagPane:storagePane;}
 int slotAt(bool bag,int index)const{const auto& slots=bag?bagSlots:containerSlots;return index>=0&&size_t(index)<slots.size()?slots[index]:-1;}
 void init()override{
  boundGeneration=minecraft->javaSession->inventory.menuGeneration;
  ImageDef image;image.name="gui/spritesheet.png";image.x=0;image.y=1;image.width=image.height=18;image.setSrc(IntRectangle(60,0,18,18));close.setImageDef(image,true);close.scaleWhenPressed=false;close.width=close.height=19;
  buttons={&bagHeader,&storageHeader,&close,&menu,&transfer};tabButtons={&menu,&transfer,&close};
  NinePatchFactory f(minecraft->textures,"gui/spritesheet.png");background=f.createSymmetrical(IntRectangle(0,0,16,16),4,4);frame=f.createSymmetrical(IntRectangle(28,42,4,4),1,1)->exclude(4);setupPositions();
 }
 void setupPositions()override{
  if(!valid())return;
  const auto& inv=minecraft->javaSession->inventory;bagSlots.clear();containerSlots.clear();
  auto add=[&](std::vector<int>& slots,const char* key){const auto range=inv.slotRanges.at(key);for(int slot=range.start;slot<range.end;++slot)slots.push_back(slot);};
  add(bagSlots,"hotbar");add(bagSlots,"player_inventory");add(containerSlots,"container");
  const int half=width/2,cell=std::max(18,std::min(32,(half-10)/3)),columns=std::max(1,(half-10)/cell),paneWidth=columns*cell,left=(half-paneWidth)/2,top=28,bottom=std::max(top+cell,height-32);
  delete bagPane;delete storagePane;bagPane=new Pane(this,true,IntRectangle(left,top,paneWidth,bottom-top),cell,bagSlots.size());storagePane=new Pane(this,false,IntRectangle(half+left,top,paneWidth,bottom-top),cell,containerSlots.size());
  bagHeader.x=0;storageHeader.x=half;bagHeader.y=storageHeader.y=0;bagHeader.width=half;storageHeader.width=width-half;bagHeader.msg="Inventory";storageHeader.msg=JavaInventoryScreen::fitTitle(*minecraft->font,inv.title.empty()?"Storage":inv.title,std::max(1,half-44));
  close.x=width-19;close.y=0;menu.x=8;transfer.x=width/2+8;menu.y=transfer.y=height-24;menu.width=transfer.width=std::max(36,width/2-16);menu.height=transfer.height=20;background->setSize(width,height);
 }
 bool addItem(const Touch::InventoryPane* pane,int index)override{if(!valid()||(pane!=bagPane&&pane!=storagePane))return false;const int slot=slotAt(pane==bagPane,index);return slot>=0&&minecraft->javaSession->clickInventorySlot(slot,transferMode==1,transferMode==2);}
 bool isAllowed(int index)override{return index>=0&&(size_t(index)<bagSlots.size()||size_t(index)<containerSlots.size());}
 std::vector<const ItemInstance*> getItems(const Touch::InventoryPane* pane)override{return std::vector<const ItemInstance*>(pane==bagPane?bagSlots.size():pane==storagePane?containerSlots.size():0,nullptr);}
 void tick()override{if(!valid()){minecraft->setScreen(nullptr);return;}bagPane->tick();storagePane->tick();Screen::tick();}
 void render(int x,int y,float a)override{
  if(!valid())return;GLboolean mask;glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);glDepthMask(GL_TRUE);glClear(GL_DEPTH_BUFFER_BIT);glDepthMask(mask);glDisable2(GL_DEPTH_TEST);glEnable2(GL_BLEND);background->draw(Tesselator::instance,0,0);Screen::render(x,y,a);
  for(Pane* pane:{bagPane,storagePane}){pane->render(x,y,a);frame->setSize(pane->rect.w+2,pane->rect.h+2);frame->draw(Tesselator::instance,pane->rect.x-1,pane->rect.y-1);}
  const auto& carried=minecraft->javaSession->inventory.carried;if(carried.count>0)drawItem(carried,std::min(width-18,std::max(0,x)),std::min(height-30,std::max(24,y)));glEnable2(GL_DEPTH_TEST);
 }
 void buttonClicked(Button* b)override{if(!valid())return;if(b==&close){minecraft->javaSession->closeInventory();minecraft->setScreen(nullptr);}else if(b==&menu){JavaInventoryViewPreference::setStorage(false);minecraft->setScreen(new JavaInventoryScreen(windowId));}else if(b==&transfer){transferMode=(transferMode+1)%3;transfer.msg=transferMode==1?"Half":transferMode==2?"Move":"Whole";}}
 void keyPressed(int key)override{if(key==Keyboard::KEY_ESCAPE||key==ControllerKeys::MenuCancel||(key&&key==minecraft->options.keyMenuCancel.key)){buttonClicked(&close);return;}Screen::keyPressed(key);}
 bool handleBackEvent(bool down)override{if(down)buttonClicked(&close);return true;}
 bool isPauseScreen()override{return false;}bool isInGameScreen()override{return true;}bool closeOnPlayerHurt()override{return false;}
};
#endif
