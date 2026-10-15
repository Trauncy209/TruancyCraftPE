#ifndef TCPE_JAVA_PLAYER_INVENTORY_SCREEN_H
#define TCPE_JAVA_PLAYER_INVENTORY_SCREEN_H
#include "../components/InventoryPane.h"
#include "../components/ImageButton.h"
// Window zero uses the same scrolling drawer, header, frame and close glyph
// as the singleplayer touch inventory. Only its item data/clicks are Java-backed.
class JavaPlayerInventoryScreen : public Screen, public Touch::IInventoryPaneCallback {
 class Pane:public Touch::InventoryPane {
  JavaPlayerInventoryScreen* owner;
 public:Pane(JavaPlayerInventoryScreen* o,Minecraft* mc,const IntRectangle& rect,int width,int count,int cell=32):Touch::InventoryPane(o,mc,rect,width,4,count,cell,4),owner(o){}
  void renderBatch(std::vector<GridItem>& items,float alpha)override{
   Touch::InventoryPane::renderBatch(items,alpha);
   glEnable2(GL_SCISSOR_TEST);glScissor(int(screenScale*bbox.x),mc->height-int(screenScale*(bbox.y+bbox.h)),int(screenScale*bbox.w),int(screenScale*bbox.h));
   for(const auto& cell:items){const int slot=owner->slotAt(cell.id);if(slot>=0){const auto& item=mc->javaSession->inventory.slots[slot];if(item.count>0)owner->drawItem(item,int(cell.xf)+(itemBbox.w-16)/2,int(cell.yf)+(itemBbox.h-16)/2);}}
   glDisable2(GL_SCISSOR_TEST);
  }
 };
 class ResultButton:public Button {
  JavaPlayerInventoryScreen* owner;
 public:ResultButton(JavaPlayerInventoryScreen* o):Button(8,""),owner(o){}
  void render(Minecraft* mc,int,int)override {
   if(!visible)return;
   mc->textures->loadAndBindTexture("gui/gui.png");glColor4f2(1,1,1,1);owner->blit(x,y,200,46,width,height,16,16);
   const int slot=owner->resultSlot();if(slot>=0&&size_t(slot)<mc->javaSession->inventory.slots.size()&&mc->javaSession->inventory.slots[slot].count>0)owner->drawItem(mc->javaSession->inventory.slots[slot],x+(width-16)/2,y+(height-16)/2);
  }
 } result{this};
 int resultSlot()const{auto it=minecraft->javaSession->inventory.slotRanges.find("result");return it==minecraft->javaSession->inventory.slotRanges.end()?-1:it->second.start;}
 Pane* pane=nullptr;Touch::THeader header;ImageButton done;Touch::TButton craft,armor,bag,hotbar,halfButton,hand;int section=0;int windowId=0;bool half=false;bool quick=false;uint64_t boundGeneration=0;std::vector<int> slots;
 void rebuild(){
  slots.clear();auto add=[&](int a,int b){for(int i=a;i<b&&size_t(i)<minecraft->javaSession->inventory.slots.size();++i)slots.push_back(i);};
  auto range=[&](const char* role){auto it=minecraft->javaSession->inventory.slotRanges.find(role);if(it!=minecraft->javaSession->inventory.slotRanges.end())add(it->second.start,it->second.end);};if(section==0){range("hotbar");range("player_inventory");}else if(section==1){range("crafting");}else if(section==2){range("armor");range("offhand");}else range("hotbar");
  delete pane;result.visible=section==1;result.active=result.visible&&resultSlot()>=0;
  if(section==1){const int grid=windowId==0?2:3,cell=std::max(18,std::min(32,(height-58)/grid)),gridWidth=grid*cell,left=(width-gridWidth-24-cell)/2,top=30+std::max(0,(height-58-gridWidth)/2);pane=new Pane(this,minecraft,IntRectangle(left,top,gridWidth,gridWidth),width,slots.size(),cell);pane->fillMarginX=0;result.x=left+gridWidth+24;result.y=top+(gridWidth-cell)/2;result.width=result.height=cell;return;}
  // The first nine slots are the actual Java hotbar, followed by the bag.
  // Keep nine columns on mobile too, so picking up and placing between rows
  // uses one native drawer instead of hiding the destination behind a tab.
  const bool playerRows=section==0||section==3;const int cell=playerRows?std::max(12,std::min(32,(width-20)/9)):32;
  const int columns=playerRows?9:std::max(1,(width-20)/cell),realWidth=columns*cell,left=(width-realWidth)/2;const int top=30,bottom=height-28;pane=new Pane(this,minecraft,IntRectangle(left,top,realWidth,std::max(cell,bottom-top)),width,slots.size(),cell);pane->fillMarginX=left;
 }
 void drawItem(const JavaGameSession::Item& item,int x,int y){
  const std::string model=item.itemModel.empty()?item.resourceId:item.itemModel;
  if(!JavaPackItemModels::render(minecraft->textures,model,x,y,16,&item)&&!JavaItemIcons::render(minecraft->textures,item.resourceId,x,y,16,&item)){auto native=JavaInventoryScreen::mapItem(item);if(!native.isNull())ItemRenderer::renderGuiItem(minecraft->font,minecraft->textures,&native,x,y,true);else drawString(minecraft->font,"?",x+4,y+2,0xffffcc55);}
  glEnable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);
  if(item.count>1){auto count=std::to_string(item.count);minecraft->font->drawShadow(count,x+16-minecraft->font->width(count),y+9,0xffffffff);}
 }
public:
 explicit JavaPlayerInventoryScreen(bool crafting=false,int menuId=0):header(0,"Inventory"),done(3,""),craft(1,"Craft"),armor(2,"Armor"),bag(4,"Bag"),hotbar(5,"Hotbar"),halfButton(6,"Whole"),hand(7,"Main hand"){section=crafting?1:0;windowId=menuId;}
 ~JavaPlayerInventoryScreen(){delete pane;}
 int window()const{return windowId;}uint64_t generation()const{return boundGeneration;}
 void init()override{boundGeneration=minecraft->javaSession->inventory.menuGeneration;ImageDef image;image.name="gui/spritesheet.png";image.x=0;image.y=1;image.width=image.height=18;image.setSrc(IntRectangle(60,0,18,18));done.setImageDef(image,true);done.width=done.height=19;done.scaleWhenPressed=false;buttons={&header,&done,&craft,&armor,&bag,&hotbar,&halfButton,&hand,&result};tabButtons={&craft,&armor,&bag,&hotbar,&halfButton,&hand,&result,&done};setupPositions();}
 void setupPositions()override{
  craft.x=0;armor.x=48;craft.y=armor.y=0;craft.width=armor.width=48;header.x=windowId==0?96:48;header.y=0;header.width=std::max(1,width-header.x);header.xText=header.x+(header.width-done.width)/2;done.x=width-19;done.y=0;
  const int footerCount=section==2?4:3,footerWidth=(width-8)/footerCount;int footerIndex=0;for(Button* b:std::vector<Button*>{&bag,&hotbar,&halfButton,&hand}){b->x=4+footerIndex++*footerWidth;b->width=footerWidth-2;}for(Button* b:std::vector<Button*>{&bag,&hotbar,&halfButton,&hand}){b->y=height-22;b->height=20;}
  armor.visible=windowId==0;armor.active=armor.visible;craft.selected=section==1;armor.selected=section==2;bag.selected=section==0;hotbar.selected=section==3;hand.visible=section==2;hand.active=hand.visible&&minecraft->javaSession->inventory.offHand.count>0;hand.msg=minecraft->javaSession->useOffHand?"Off hand":"Main hand";rebuild();
 }
 int slotAt(int index)const{return index>=0&&size_t(index)<slots.size()?slots[index]:-1;}
 bool addItem(const Touch::InventoryPane*,int index)override{if(!minecraft->javaSession||minecraft->javaSession->inventory.windowId!=windowId||minecraft->javaSession->inventory.menuGeneration!=boundGeneration)return false;int slot=slotAt(index);return slot>=0&&minecraft->javaSession->clickInventorySlot(slot,half,quick);}
 bool isAllowed(int slot)override{return slotAt(slot)>=0;}
 std::vector<const ItemInstance*> getItems(const Touch::InventoryPane*)override{return std::vector<const ItemInstance*>(slots.size(),nullptr);}
 void tick()override{auto* s=minecraft->javaSession;if(!s||s->inventory.windowId!=windowId||s->inventory.menuGeneration!=boundGeneration){minecraft->setScreen(nullptr);return;}hand.active=hand.visible&&s->inventory.offHand.count>0;pane->tick();Screen::tick();}
 void render(int x,int y,float a)override{
  GLboolean oldDepthMask=GL_TRUE;glGetBooleanv(GL_DEPTH_WRITEMASK,&oldDepthMask);glDepthMask(GL_TRUE);glClear(GL_DEPTH_BUFFER_BIT);glDepthMask(oldDepthMask);glDisable2(GL_DEPTH_TEST);glEnable2(GL_BLEND);fill(0,0,width,height,0xff333333);Screen::render(x,y,a);pane->render(x,y,a);if(result.visible){result.render(minecraft,x,y);glEnable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);drawCenteredString(minecraft->font,">",result.x-12,result.y+(result.height-8)/2,0xffdddddd);}
  minecraft->textures->loadAndBindTexture("gui/itemframe.png");glEnable2(GL_BLEND);glColor4f2(1,1,1,1);glBlendFunc2(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);blit(0,pane->rect.y-6,0,0,width,pane->rect.h+12,215,256);
  const auto& carried=minecraft->javaSession->inventory.carried;if(carried.count>0)drawItem(carried,std::min(width-18,std::max(0,x)),std::min(height-18,std::max(24,y)));glEnable2(GL_DEPTH_TEST);
 }
 void buttonClicked(Button* b)override{if(!minecraft->javaSession||minecraft->javaSession->inventory.windowId!=windowId||minecraft->javaSession->inventory.menuGeneration!=boundGeneration)return;if(b==&done){minecraft->javaSession->closeInventory();minecraft->setScreen(nullptr);}else if(b==&result){const int slot=resultSlot();if(slot>=0)minecraft->javaSession->clickInventorySlot(slot,half,quick);}else if(b==&craft){minecraft->setScreen(new JavaCraftingScreen(windowId));}else if(b==&halfButton){if(quick){quick=false;half=false;}else if(half){half=false;quick=true;}else half=true;halfButton.msg=quick?"Move":half?"Half":"Whole";}else if(b==&hand&&hand.active){minecraft->javaSession->useOffHand=!minecraft->javaSession->useOffHand;hand.msg=minecraft->javaSession->useOffHand?"Off hand":"Main hand";}else if(b==&craft||b==&armor||b==&bag||b==&hotbar){section=b==&craft?1:b==&armor?2:b==&hotbar?3:0;setupPositions();}}
 void keyPressed(int key)override{if(key==Keyboard::KEY_ESCAPE||key==ControllerKeys::MenuCancel||(key&&key==minecraft->options.keyMenuCancel.key)){buttonClicked(&done);return;}Screen::keyPressed(key);}
 bool handleBackEvent(bool down)override{if(down)buttonClicked(&done);return true;}
 bool isPauseScreen()override{return false;}bool isInGameScreen()override{return true;}bool closeOnPlayerHurt()override{return false;}
};
#endif
