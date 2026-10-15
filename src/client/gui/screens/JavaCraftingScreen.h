#ifndef TCPE_JAVA_CRAFTING_SCREEN_H
#define TCPE_JAVA_CRAFTING_SCREEN_H
#include "crafting/PaneCraftingScreen.h"
#include "../components/NinePatch.h"
#include "../components/TextBox.h"
#include "../../../platform/input/Keyboard.h"
#include "../../../platform/input/ControllerActionKeys.h"
Screen* makeNativeJavaPlayerInventory(bool crafting=false,int menuId=0);
class JavaCraftingScreen:public Screen,public IItemPaneCallback {
 class NativeCategoryButton:public ImageButton {
  NinePatchLayer *normal,*pressed;
 public:explicit NativeCategoryButton(int id,Textures* textures):ImageButton(id,""){NinePatchFactory f(textures,"gui/spritesheet.png");normal=f.createSymmetrical(IntRectangle(8,32,8,8),2,2);pressed=f.createSymmetrical(IntRectangle(0,32,8,8),2,2);}
  ~NativeCategoryButton(){delete normal;delete pressed;}
  void renderBg(Minecraft*,int,int)override{normal->setSize(width,height);pressed->setSize(width,height);(selected?pressed:normal)->draw(Tesselator::instance,x,y);}
  bool isSecondImage(bool)override{return false;}
 };

 class RecipePane:public ItemPane {
  JavaCraftingScreen* owner;
 public:RecipePane(JavaCraftingScreen* o,const IntRectangle& rect,int count):ItemPane(o,o->minecraft->textures,rect,count,o->height,o->minecraft->height),owner(o){f=o->minecraft->font;}
  void renderBatch(std::vector<GridItem>& items,float alpha)override{
   ItemPane::renderBatch(items,alpha);
   glEnable2(GL_SCISSOR_TEST);glScissor(int(screenScale*bbox.x),physicalScreenHeight-int(screenScale*(bbox.y+bbox.h)),int(screenScale*bbox.w),int(screenScale*bbox.h));
   for(const auto& cell:items)if(cell.id>=0&&size_t(cell.id)<owner->recipeIndices.size()){const auto& r=owner->minecraft->javaSession->recipes[owner->recipeIndices[cell.id]];owner->drawItem(r.result,int(cell.xf)+itemBbox.w-16,int(cell.yf)+2);}
   glDisable2(GL_SCISSOR_TEST);
  }
 };
 std::array<NativeCategoryButton*,4> categories{{nullptr,nullptr,nullptr,nullptr}};int category=-1;IntRectangle paneRect;
 RecipePane* pane=nullptr;NinePatchLayer *background=nullptr,*frame=nullptr;CraftButton craft;ImageButton close;Touch::TButton bag,grid,all;std::vector<CItem*> items;std::vector<size_t> recipeIndices;int selected=-1;uint32_t selectedRecipeId=0;bool hasSelectedRecipe=false;int windowId;uint64_t boundGeneration=0;unsigned revision=0;bool craftAll=false;bool awaitingPlacement=false;unsigned placementRevision=0;int awaitingTicks=0;bool awaitingTake=false;unsigned takeRevision=0;std::string takenResource;int takeTicks=0;
 void clearItems(){delete pane;pane=nullptr;for(auto* item:items)delete item;items.clear();recipeIndices.clear();}
 int categoryBits(const JavaGameSession::Recipe& recipe)const{const int id=recipe.result.originalId;if(id>0&&id<Item::MAX_ITEMS&&Item::items[id]&&Item::items[id]->category>0){int bits=Item::items[id]->category;return (bits&16)?bits|8:bits;}if(recipe.category=="CraftingBuildingBlocks")return 1;if(recipe.category=="CraftingEquipment")return 2;return 8;}
 void rebuild(){
  clearItems();selected=-1;auto* s=minecraft->javaSession;const unsigned grid=windowId==0?2:3;
  for(size_t i=0;i<s->recipes.size();++i){const auto& recipe=s->recipes[i];if(recipe.width>grid||recipe.height>grid||(category>=0&&!(categoryBits(recipe)&(1<<category))))continue;if(items.size()>=4096)break;recipeIndices.push_back(i);auto* ci=new CItem(ItemInstance(),nullptr,JavaInventoryScreen::itemLabel(recipe.result));ci->setCanCraft(recipe.placeable);items.push_back(ci);if(hasSelectedRecipe&&recipe.id==selectedRecipeId)selected=int(items.size()-1);}
  if(hasSelectedRecipe&&std::none_of(s->recipes.begin(),s->recipes.end(),[&](const JavaGameSession::Recipe& r){return r.id==selectedRecipeId;})){hasSelectedRecipe=false;awaitingPlacement=false;awaitingTicks=0;}
  const int right=std::min(100,std::max(80,width/3));const int categorySize=std::max(16,(height-16)/4);const IntRectangle rect(categorySize+12,8,std::max(24,width-right-categorySize-20),std::max(22,height-38));paneRect=rect;for(auto* item:items)item->text=minecraft->font->fitJava(item->text,std::max(1,rect.w-22));pane=new RecipePane(this,rect,items.size());if(selected>=0)pane->setSelected(selected,true);frame->setSize(rect.w+2,rect.h+4);revision=s->recipesRevision;
 }
 void drawItem(const JavaGameSession::Item& item,int x,int y){if(item.count<=0)return;const auto model=item.itemModel.empty()?item.resourceId:item.itemModel;if(!JavaPackItemModels::render(minecraft->textures,model,x,y,16,&item)&&!JavaItemIcons::render(minecraft->textures,item.resourceId,x,y,16,&item)){auto native=JavaInventoryScreen::mapItem(item);if(!native.isNull())ItemRenderer::renderGuiItem(minecraft->font,minecraft->textures,&native,x,y,true);else drawString(minecraft->font,"?",x+4,y+2,0xffffcc55);}}
 static std::vector<std::string> wrapPreviewLabel(Font& font,const std::string& text,int pixels){
  std::vector<std::string> lines;std::string line,word;
  auto flushWord=[&](){if(word.empty())return;const std::string combined=line.empty()?word:line+" "+word;if(font.widthJava(combined)<=pixels)line=combined;else{if(!line.empty()){lines.push_back(line);line.clear();}auto parts=font.wrapJava(word,pixels);for(size_t i=0;i+1<parts.size();++i)lines.push_back(parts[i]);if(!parts.empty())line=parts.back();}word.clear();};
  for(const auto& glyph:JavaRichText::parse(text)){if(glyph.codepoint==' '||glyph.codepoint=='\n'){flushWord();if(glyph.codepoint=='\n'){lines.push_back(line);line.clear();}}else word+=JavaRichText::encode(glyph);}
  flushWord();if(!line.empty()||lines.empty())lines.push_back(line);return lines;
 }
 bool valid()const{return minecraft->javaSession&&minecraft->javaSession->inventory.windowId==windowId&&minecraft->javaSession->inventory.menuGeneration==boundGeneration;}
 const JavaGameSession::Recipe* current()const{return selected>=0&&size_t(selected)<recipeIndices.size()&&recipeIndices[selected]<minecraft->javaSession->recipes.size()?&minecraft->javaSession->recipes[recipeIndices[selected]]:nullptr;}
 bool outputReady()const{const auto& inv=minecraft->javaSession->inventory;return current()&&!inv.slots.empty()&&inv.slots[0].count>0&&inv.slots[0].resourceId==current()->result.resourceId;}
 bool ghostMatches()const{const auto* recipe=current();const auto& ghost=minecraft->javaSession->recipeGhost;if(!recipe||!ghost.present||ghost.windowId!=windowId)return false;const auto& preview=ghost.preview;if(recipe->width!=preview.width||recipe->height!=preview.height||recipe->kind!=preview.kind||recipe->result.resourceId!=preview.result.resourceId||recipe->result.count!=preview.result.count||recipe->ingredients.size()!=preview.ingredients.size())return false;for(size_t i=0;i<recipe->ingredients.size();++i)if(recipe->ingredients[i].resourceId!=preview.ingredients[i].resourceId||recipe->ingredients[i].count!=preview.ingredients[i].count)return false;return true;}
 bool prepareCurrent(){auto* s=minecraft->javaSession;const auto* recipe=current();if(!recipe||!recipe->placeable||awaitingPlacement||s->inventory.carried.count>0)return false;placementRevision=s->inventoryRevision;awaitingTicks=0;awaitingPlacement=s->placeRecipe(recipe->id,craftAll);return awaitingPlacement;}
 // A native recipe-button craft must finish in the bag, rather than leave
 // the result attached to an invisible mouse cursor on touch devices.
 bool pickupSlot(size_t slot){
  const auto& inv=minecraft->javaSession->inventory;
  if(slot>=inv.slots.size())return false;
  return minecraft->javaSession->connection.send("{\"type\":\"click_slot\",\"window_id\":"+std::to_string(inv.windowId)+",\"menu_generation\":"+std::to_string(inv.menuGeneration)+",\"slot\":"+std::to_string(slot)+",\"right\":false}");
 }
 void finishTake(){
  auto* s=minecraft->javaSession;
  if(!awaitingTake)return;
  if(++takeTicks>40){awaitingTake=false;return;}
  if(s->inventoryRevision==takeRevision)return;
  const auto& inv=s->inventory;
  if(inv.carried.count<=0)return; // An older inventory snapshot may precede the pickup response.
  if(inv.carried.resourceId!=takenResource){awaitingTake=false;return;}
  // Never swap an occupied slot or guess component/stack compatibility.
  for(const char* role:{"hotbar","player_inventory"}){
   const auto range=inv.slotRanges.find(role);if(range==inv.slotRanges.end())continue;
   for(int slot=range->second.start;slot<range->second.end;++slot)
    if(slot>=0&&size_t(slot)<inv.slots.size()&&inv.slots[slot].count<=0){
     pickupSlot(size_t(slot));awaitingTake=false;return;
    }
  }
  // Full inventories keep the authoritative cursor accessible in Bag/Grid.
  awaitingTake=false;
 }
public:
 explicit JavaCraftingScreen(int id):craft(1),close(2,""),bag(3,"Inventory"),grid(5,"Grid"),all(4,"One") ,windowId(id){}
 ~JavaCraftingScreen(){for(auto* button:categories)delete button;clearItems();delete background;delete frame;}
 int window()const{return windowId;}uint64_t generation()const{return boundGeneration;}
 void init()override{boundGeneration=minecraft->javaSession->inventory.menuGeneration;craft.init(minecraft->textures);ImageDef d;d.name="gui/spritesheet.png";d.x=0;d.y=1;d.width=d.height=18;d.setSrc(IntRectangle(60,0,18,18));close.setImageDef(d,true);close.scaleWhenPressed=false;buttons={&craft,&close,&bag,&grid,&all};for(int i=0;i<4;++i){categories[i]=new NativeCategoryButton(100+i,minecraft->textures);buttons.push_back(categories[i]);}tabButtons={&craft,&bag,&grid,&all,categories[0],categories[1],categories[2],categories[3],&close};NinePatchFactory f(minecraft->textures,"gui/spritesheet.png");background=f.createSymmetrical(IntRectangle(0,0,16,16),4,4);frame=f.createSymmetrical(IntRectangle(0,20,8,8),1,2)->setExcluded(1<<4);setupPositions();}
 void setupPositions()override{const int categorySize=std::max(16,(height-16)/4);for(int i=0;i<4;++i){auto* button=categories[i];button->x=6;button->y=6+i*(categorySize+1);button->width=button->height=categorySize;button->selected=category==i;ImageDef icon;icon.name="gui/spritesheet.png";icon.x=0;icon.y=1;icon.width=icon.height=categorySize;icon.setSrc(IntRectangle(32*(i/2),64+(i&1)*32,32,32));button->setImageDef(icon,false);}const int right=std::min(100,std::max(80,width/3));craft.x=width-right+6;craft.y=24;craft.setSize(right-12,62);close.x=width-19;close.y=0;close.width=close.height=19;bag.x=categorySize+12;bag.y=height-25;bag.width=std::max(30,(width-right-categorySize-24)/2);bag.height=20;bag.visible=windowId==0;bag.active=bag.visible;bag.msg=bag.width<60?"Bag":"Inventory";grid.x=bag.x+bag.width+4;grid.y=bag.y;grid.width=bag.width;grid.height=20;grid.visible=true;all.x=width-right+6;all.y=height-25;all.width=right-12;all.height=20;all.visible=height>=148;all.active=all.visible;background->setSize(width,height);rebuild();}
 void tick()override{if(!valid()){minecraft->setScreen(nullptr);return;}auto* s=minecraft->javaSession;if(s->recipesRevision!=revision)rebuild();finishTake();if(awaitingPlacement&&(s->inventoryRevision!=placementRevision||outputReady())){awaitingPlacement=false;awaitingTicks=0;}if(awaitingPlacement&&++awaitingTicks>40){awaitingPlacement=false;awaitingTicks=0;}if(ghostMatches()){awaitingPlacement=false;awaitingTicks=0;}pane->tick();all.active=all.visible&&!awaitingPlacement&&!awaitingTake&&s->inventory.carried.count<=0;craft.active=!ghostMatches()&&s->inventory.carried.count<=0&&!awaitingPlacement&&!awaitingTake&&(outputReady()||(current()&&current()->placeable));}
 void render(int x,int y,float a)override{if(!valid())return;GLboolean oldDepthMask=GL_TRUE;glGetBooleanv(GL_DEPTH_WRITEMASK,&oldDepthMask);glDepthMask(GL_TRUE);glClear(GL_DEPTH_BUFFER_BIT);glDepthMask(oldDepthMask);glDisable2(GL_DEPTH_TEST);glEnable2(GL_BLEND);background->draw(Tesselator::instance,0,0);Screen::render(x,y,a);frame->draw(Tesselator::instance,paneRect.x-1,paneRect.y-2);pane->render(x,y,a);glEnable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);const auto* recipe=current();if(recipe){const int columns=std::max(1,int(recipe->width));for(size_t i=0;i<recipe->ingredients.size();++i){const int ix=craft.x+4+int(i%columns)*20,iy=craft.y+3+int(i/columns)*18;if(recipe->ingredients[i].count>0)drawItem(recipe->ingredients[i],ix,iy);else if(!recipe->previewComplete){glEnable2(GL_TEXTURE_2D);minecraft->font->drawShadow("?",ix+4,iy+4,0xffcccccc);}}glEnable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);auto label=wrapPreviewLabel(*minecraft->font,JavaInventoryScreen::itemLabel(recipe->result),craft.width);const int previewBottom=all.visible?all.y-2:height-4;const std::string status=minecraft->javaSession->inventory.carried.count>0?(windowId==0?"Place in Bag":"Place in Grid"):ghostMatches()?"Need ingredients":awaitingPlacement?"Waiting...":outputReady()?"Tap to craft":"Tap to prepare";const auto hints=wrapPreviewLabel(*minecraft->font,status,craft.width);int yy=craft.y+craft.height+5;const int hintRows=std::min(int(hints.size()),std::max(0,(previewBottom-yy-2)/10));const int titleBottom=previewBottom-hintRows*10-2;for(const auto& line:label){if(yy+9>titleBottom)break;minecraft->font->drawShadow(line,craft.x,yy,0xfff0f0f0);yy+=10;}int statusY=yy+2;for(int i=0;i<hintRows;++i){if(statusY+9>previewBottom)break;minecraft->font->drawShadow(hints[i],craft.x,statusY,0xffc0c0c0);statusY+=10;}}else {
  // The empty selection belongs to the ingredient preview, never the recipe list.
  auto hint=wrapPreviewLabel(*minecraft->font,items.empty()?"No server recipes":"Choose a recipe",std::max(1,craft.width-8));
  const int bottom=std::min(craft.y+craft.height-4,height-29);
  const int rows=std::max(0,(bottom-craft.y-4)/10);
  const int shown=std::min(rows,int(hint.size()));
  int yy=craft.y+4+std::max(0,(bottom-craft.y-4-shown*10)/2);
  for(int i=0;i<shown;++i){drawCenteredString(minecraft->font,hint[i],craft.x+craft.width/2,yy,0xfff0f0f0);yy+=10;}
 }const auto& carried=minecraft->javaSession->inventory.carried;if(carried.count>0)drawItem(carried,std::min(width-18,std::max(0,x)),std::min(height-18,std::max(24,y)));glEnable2(GL_DEPTH_TEST);}
 void onItemSelected(const ItemPane*,int index)override{if(!valid()||awaitingPlacement||awaitingTake||index<0||size_t(index)>=recipeIndices.size())return;selected=index;selectedRecipeId=minecraft->javaSession->recipes[recipeIndices[index]].id;hasSelectedRecipe=true;awaitingPlacement=false;awaitingTicks=0;prepareCurrent();}
 const std::vector<CItem*>& getItems(const ItemPane*)override{return items;}
 void buttonClicked(Button* b)override{if(!valid())return;auto* s=minecraft->javaSession;if(b->id>=100&&b->id<104){category=category==b->id-100?-1:b->id-100;awaitingPlacement=false;setupPositions();}else if(b==&close){s->closeInventory();minecraft->setScreen(nullptr);}else if(b==&bag&&windowId==0)minecraft->setScreen(makeNativeJavaPlayerInventory());else if(b==&grid)minecraft->setScreen(makeNativeJavaPlayerInventory(true,windowId));else if(b==&all&&all.visible&&!awaitingPlacement&&!awaitingTake&&s->inventory.carried.count<=0){craftAll=!craftAll;all.msg=craftAll?"All":"One";prepareCurrent();}else if(b==&craft&&!ghostMatches()&&!awaitingPlacement&&!awaitingTake&&s->inventory.carried.count<=0){if(outputReady()){takeRevision=s->inventoryRevision;takenResource=s->inventory.slots[0].resourceId;takeTicks=0;awaitingTake=!craftAll&&pickupSlot(0);if(craftAll)s->clickInventorySlot(0,false,true);}else prepareCurrent();}}
 void keyPressed(int key)override{
  bool editing=false;for(auto* box:textBoxes)if(box->focused)editing=true;
  if(key==Keyboard::KEY_ESCAPE||key==ControllerKeys::MenuCancel||(!editing&&key&&key==minecraft->options.keyMenuCancel.key)){buttonClicked(&close);return;}
  Screen::keyPressed(key);
 }
 bool handleBackEvent(bool down)override{if(down){if(windowId==0)buttonClicked(&bag);else buttonClicked(&close);}return true;}
 bool isPauseScreen()override{return false;}bool isInGameScreen()override{return true;}bool closeOnPlayerHurt()override{return false;}
};
#endif
