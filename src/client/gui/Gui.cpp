#include "../../platform/input/Keyboard.h"
#include "network/java/JavaItemIcons.h"
#include "network/java/JavaPackItemModels.h"
#include "network/java/JavaRichText.h"
#include "Gui.h"
#include <algorithm>
#include "Font.h"
#include "client/Options.h"
#include "screens/IngameBlockSelectionScreen.h"
#include "screens/JavaInventoryScreen.h"
#include "screens/ConsoleScreen.h"
#include "screens/ChatScreen.h"
#include "../Minecraft.h"
#include "../player/LocalPlayer.h"
#include "../renderer/Tesselator.h"
#include "../renderer/TileRenderer.h"
#include "../renderer/LevelRenderer.h"
#include "../renderer/GameRenderer.h"
#include "../renderer/entity/ItemRenderer.h"
#include "../player/input/IInputHolder.h"
#include "../gamemode/GameMode.h"
#include "../gamemode/CreativeMode.h"
#include "../renderer/Textures.h"
#include "../../AppConstants.h"
#include "../../world/entity/player/Inventory.h"
#include "../../world/level/material/Material.h"
#include "../../world/item/Item.h"
#include "../../world/item/ItemInstance.h"
#include "../../platform/input/Mouse.h"
#include "../../world/level/Level.h"
#include "../../world/PosTranslator.h"
#include "../../platform/time.h"
#include <cmath>

namespace {
struct JavaHotbarLayout { int left, pitch, iconSize, inventoryX; bool touch; };
JavaHotbarLayout javaHotbarLayout(Minecraft* game) {
 const int width=int(game->width*Gui::InvGuiScale);
 const bool touch=game->useTouchscreen();
 const int pitch=std::min(20,std::max(1,(width-(touch?28:8))/9));
 const int left=(width-(9*pitch+(touch?22:0)))/2;
 return {left,pitch,std::min(16,std::max(1,pitch-4)),left+9*pitch+2,touch};
}
}
float Gui::InvGuiScale = 1.0f / 3.0f;
float Gui::GuiScale = 1.0f / Gui::InvGuiScale;
const float Gui::DropTicks = 40.0f;

//#include <android/log.h>

Gui::Gui(Minecraft* minecraft)
:	minecraft(minecraft),
	tickCount(0),
	progress(0),
	overlayMessageTime(0),
	animateOverlayMessageColor(false),
	tbr(1),
	_inventoryNeedsUpdate(true),
	_flashSlotId(-1),
	_flashSlotStartTime(-1),
	_slotFont(NULL),
	_numSlots(4),
	_currentDropTicks(-1),
	_currentDropSlot(-1),
	MAX_MESSAGE_WIDTH(240),
	itemNameOverlayTime(2)
{
	glGenBuffers2(1, &_inventoryRc.vboId);
	glGenBuffers2(1, &rcFeedbackInner.vboId);
	glGenBuffers2(1, &rcFeedbackOuter.vboId);
	//Gui::InvGuiScale = 1.0f / (int) (3 * Minecraft::width / 854);
}

Gui::~Gui()
{
	if (_slotFont)
		delete _slotFont;

	glDeleteBuffers(1, &_inventoryRc.vboId);
}

void Gui::render(float a, bool mouseFree, int xMouse, int yMouse) {

	if (!minecraft->level || !minecraft->player)
		return;

	//minecraft->gameRenderer->setupGuiScreen();
	Font* font = minecraft->font;

#ifdef PLATFORM_DESKTOP
	const bool isTouchInterface = false;
#else
	const bool isTouchInterface = minecraft->useTouchscreen();
#endif
	const int screenWidth = (int)(minecraft->width * InvGuiScale);
	const int screenHeight = (int)(minecraft->height * InvGuiScale);
	blitOffset = -90;
	renderProgressIndicator(isTouchInterface, screenWidth, screenHeight, a);

	glColor4f2(1, 1, 1, 1);

	// H: 4
    // T: 7
    // L: 6
    // F: 3
	int ySlot = screenHeight - 16 - 3;

	if (minecraft->gameMode->canHurtPlayer()) {
  if(minecraft->javaSession)renderJavaVitals(screenWidth,screenHeight);else {
		minecraft->textures->loadAndBindTexture("gui/icons.png");
		Tesselator& t = Tesselator::instance;
		t.beginOverride();
		t.colorABGR(0xffffffff);
		renderHearts();
		renderBubbles();
		t.endOverrideAndDraw();
  }
	}

	if(minecraft->player->getSleepTimer() > 0) {
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_ALPHA_TEST);

		renderSleepAnimation(screenWidth, screenHeight);

		glEnable(GL_ALPHA_TEST);
		glEnable(GL_DEPTH_TEST);
	}

	renderToolBar(a, ySlot, screenWidth);
 if(minecraft->javaSession){renderJavaExperience(screenWidth,ySlot);renderJavaActionBar(screenWidth,ySlot,a);renderJavaTitles(screenWidth,screenHeight,a);renderJavaBossBars(screenWidth,screenHeight);renderJavaScoreboard(screenWidth,screenHeight);}

	glEnable(GL_BLEND);
	unsigned int max = minecraft->useTouchscreen() ? 16 : 10;
    bool isChatting = false;
	renderChatMessages(screenHeight, max, isChatting, font);
#if !defined(RPI)
	renderOnSelectItemNameText(screenWidth, font, ySlot);
#endif
#if defined(RPI)
	renderDebugInfo();
#else
	if (minecraft->options.renderDebug)
		renderDebugInfo();
#endif

    glDisable(GL_BLEND);
	glEnable2(GL_ALPHA_TEST);
}

int Gui::getSlotIdAt(int x, int y) {
	int screenWidth = (int)(minecraft->width * InvGuiScale);
	int screenHeight = (int)(minecraft->height * InvGuiScale);
	x = (int)(x * InvGuiScale);
	y = (int)(y * InvGuiScale);

	if (y < (screenHeight - 16 - 3) || y > screenHeight)
		return -1;

	if (minecraft->javaSession) {
  const auto bar=javaHotbarLayout(minecraft);
  if(bar.touch && x>=bar.inventoryX && x<bar.inventoryX+20) return 9;
  if(x<bar.left || x>=bar.left+9*bar.pitch) return -1;
  return (x-bar.left)/bar.pitch;
 }
 int xBase = 2 + screenWidth / 2 - getNumSlots() * 10;
	int xRel  = (x - xBase);
	if (xRel < 0)
		return -1;

	int slot = xRel / 20;
	return (slot >= 0 && slot < getNumSlots())? slot : -1;
}

bool Gui::isInside(int x, int y) {
	return getSlotIdAt(x, y) != -1;
}

int Gui::getNumSlots() {
	return minecraft->javaSession ? 9 : _numSlots;
}

int Gui::getItemBarSelectionSize() const {
 if(minecraft->javaSession) return 9;
#ifndef PLATFORM_DESKTOP
	return _numSlots > 0 ? _numSlots - 1 : 0;
#else
	return _numSlots;
#endif
}

void Gui::flashSlot(int slotId) {
	_flashSlotId = slotId;
	_flashSlotStartTime = getTimeS();
}

void Gui::getSlotPos(int slot, int& posX, int& posY) {
	int screenWidth = (int)(minecraft->width * InvGuiScale);
	int screenHeight = (int)(minecraft->height * InvGuiScale);
	if(minecraft->javaSession){const auto bar=javaHotbarLayout(minecraft);posX=slot==9?bar.inventoryX:bar.left+slot*bar.pitch;posY=screenHeight-22;return;}
 posX = screenWidth / 2 - getNumSlots() * 10 + slot * 20,
	posY = screenHeight - 22;
}

RectangleArea Gui::getRectangleArea(int extendSide) {
	if(minecraft->javaSession){const auto bar=javaHotbarLayout(minecraft);const float left=(bar.left-3)*GuiScale,right=(bar.left+9*bar.pitch+(bar.touch?22:0)+3)*GuiScale;return RectangleArea(extendSide<0?0:left,minecraft->height-25*GuiScale,extendSide>0?minecraft->width:right,minecraft->height);}
 const int Spacing = 3;
	const float pCenterX   = 2.0f + (float)(minecraft->width / 2);
	const float pHalfWidth = (1.0f + (getNumSlots() * 10 + Spacing)) * Gui::GuiScale;
	const float pHeight    = (22 + Spacing) * Gui::GuiScale;

	if (extendSide < 0)
		return RectangleArea(0, (float)minecraft->height-pHeight, pCenterX+pHalfWidth+2, (float)minecraft->height);
	if (extendSide > 0)
		return RectangleArea(pCenterX-pHalfWidth, (float)minecraft->height-pHeight, (float)minecraft->width, (float)minecraft->height);
	
	return RectangleArea(pCenterX-pHalfWidth, (float)minecraft->height-pHeight, pCenterX+pHalfWidth+2, (float)minecraft->height);
}

void Gui::handleClick(int button, int x, int y) {
	if (button != MouseAction::ACTION_LEFT)	return;

	int slot = getSlotIdAt(x, y);
	if (slot != -1)
	{
  if(minecraft->javaSession && slot==9){showJavaInventory(minecraft,true);return;}
#ifndef PLATFORM_DESKTOP
		if (!minecraft->javaSession && slot == (getNumSlots()-1))
		{
			if (!showJavaInventory(minecraft, true)) minecraft->screenChooser.setScreen(SCREEN_BLOCKSELECTION);
		}
		else
#endif
		{
			minecraft->player->inventory->selectSlot(slot);
            if (minecraft->javaSession) minecraft->javaSession->connection.selectHotbar(slot);
			itemNameOverlayTime = 0;
		}
	}
}

void Gui::handleKeyPressed(int key)
{
	if (key == 99)
	{
		if (minecraft->player->inventory->selected > 0)
		{
			minecraft->player->inventory->selected--;
            if (minecraft->javaSession) minecraft->javaSession->connection.selectHotbar(minecraft->player->inventory->selected);
		}
	}
	else if (key == 4)
	{
		if (minecraft->player->inventory->selected < (getItemBarSelectionSize() - 1))
		{
			minecraft->player->inventory->selected++;
            if (minecraft->javaSession) minecraft->javaSession->connection.selectHotbar(minecraft->player->inventory->selected);
		}
	}
	else if (key == 100)
	{
		if (!showJavaInventory(minecraft, true)) minecraft->screenChooser.setScreen(SCREEN_BLOCKSELECTION);
	}
	else if (key == minecraft->options.keyDrop.key) 
	{
		if (minecraft->javaSession) minecraft->javaSession->connection.send(Keyboard::isKeyDown(17) ? "{\"type\":\"drop_selected\",\"all\":true}" : "{\"type\":\"drop_selected\",\"all\":false}");
        else minecraft->player->inventory->dropSlot(minecraft->player->inventory->selected, false);
	}
}

void Gui::tick() {
 if(!minecraft->javaSession||!minecraft->javaSession->spawned)javaHearts.reset();
	if (overlayMessageTime > 0) overlayMessageTime--;
	tickCount++;
	if(itemNameOverlayTime < 2)
		itemNameOverlayTime += 1.0f / SharedConstants::TicksPerSecond;
	for (unsigned int i = 0; i < guiMessages.size(); i++) {
	    guiMessages.at(i).ticks++;
	}

    for(auto& message:rawGuiMessages)++message.ticks;
    if (!minecraft->isCreativeMode())
        tickItemDrop();
}

void Gui::addMessage(const std::string& text,const std::string& signatureId) {
 if(!signatureId.empty()&&std::any_of(rawGuiMessages.begin(),rawGuiMessages.end(),[&](const GuiMessage& m){return m.signatureId==signatureId;}))return;
 if(!minecraft->font)return;
 size_t rawEnd=std::min(size_t(32767),text.size());while(rawEnd<text.size()&&rawEnd>0&&(static_cast<unsigned char>(text[rawEnd])&0xc0)==0x80)--rawEnd;
 GuiMessage raw;raw.signatureId=signatureId;raw.message=text.substr(0,rawEnd);raw.ticks=0;rawGuiMessages.insert(rawGuiMessages.begin(),raw);while(rawGuiMessages.size()>100)rawGuiMessages.pop_back();
 auto addLine=[&](const std::string& line){GuiMessage message;message.signatureId=signatureId;message.message=line;message.ticks=0;guiMessages.insert(guiMessages.begin(),message);};
 if(minecraft->javaSession){for(const auto& line:minecraft->font->wrapJava(raw.message,MAX_MESSAGE_WIDTH))addLine(line);}
 else {std::string remaining=raw.message;while(minecraft->font->width(remaining)>MAX_MESSAGE_WIDTH){size_t i=1;while(i<remaining.size()&&minecraft->font->width(remaining.substr(0,i+1))<=MAX_MESSAGE_WIDTH)++i;while(i<remaining.size()&&i>0&&(static_cast<unsigned char>(remaining[i])&0xc0)==0x80)--i;if(i==0){i=1;while(i<remaining.size()&&(static_cast<unsigned char>(remaining[i])&0xc0)==0x80)++i;}addLine(remaining.substr(0,i));remaining=remaining.substr(i);}addLine(remaining);}
 while(guiMessages.size()>100)guiMessages.pop_back();
}

void Gui::deleteMessageBySignature(const std::string& signatureId){
 if(signatureId.empty())return;
 auto matches=[&](const GuiMessage& message){return message.signatureId==signatureId;};
 guiMessages.erase(std::remove_if(guiMessages.begin(),guiMessages.end(),matches),guiMessages.end());
 rawGuiMessages.erase(std::remove_if(rawGuiMessages.begin(),rawGuiMessages.end(),matches),rawGuiMessages.end());
}

void Gui::setNowPlaying(const std::string& string) {
	overlayMessageString = "Now playing: " + string;
	overlayMessageTime = 20 * 3;
	animateOverlayMessageColor = true;
}

void Gui::displayClientMessage(const std::string& messageId) {
	//Language language = Language.getInstance();
	//std::string languageString = language.getElement(messageId);
	addMessage(std::string("Client message: ") + messageId);
}

void Gui::renderVignette(float br, int w, int h) {
	br = 1 - br;
	if (br < 0) br = 0;
	if (br > 1) br = 1;
	tbr += (br - tbr) * 0.01f;

	glDisable(GL_DEPTH_TEST);
	glDepthMask(false);
	glBlendFunc2(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
	glColor4f2(tbr, tbr, tbr, 1);
	minecraft->textures->loadAndBindTexture("misc/vignette.png");

	Tesselator& t = Tesselator::instance;
	t.begin();
	t.vertexUV(0, (float)h, -90, 0, 1);
	t.vertexUV((float)w, (float)h, -90, 1, 1);
	t.vertexUV((float)w, 0, -90, 1, 0);
	t.vertexUV(0, 0, -90, 0, 0);
	t.draw();
	glDepthMask(true);
	glEnable(GL_DEPTH_TEST);
	glColor4f2(1, 1, 1, 1);
	glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Gui::renderSlot(int slot, int x, int y, float a) {
 if(minecraft->javaSession&&slot>=0&&slot<9){const auto& item=minecraft->javaSession->inventory.hotbar[slot];
  const std::string model=item.itemModel.empty()?item.resourceId:item.itemModel;
  if(item.count>0&&(JavaPackItemModels::has(model)||JavaItemIcons::has(item.resourceId))){auto& t=Tesselator::instance;bool batch=t.isOverridden();if(batch)t.endOverrideAndDraw();float size=float(javaHotbarLayout(minecraft).iconSize);bool rendered=JavaPackItemModels::render(minecraft->textures,model,float(x),float(y),size,&item)||JavaItemIcons::render(minecraft->textures,JavaItemIcons::animatedResource(*minecraft->javaSession,item,minecraft->player->x,minecraft->player->z,minecraft->player->yRot,minecraft->getSimulationTick(),minecraft->player->entityId),float(x),float(y),size,&item);if(batch)t.beginOverride();if(rendered||(item.hasProfile&&item.resourceId=="minecraft:player_head"))return;}
 }

	ItemInstance* item = minecraft->player->inventory->getItem(slot);
	if (!item) {
		//LOGW("Warning: item @ Gui::renderSlot is NULL\n");
		return;
	}

	const bool fancy = true;
	ItemRenderer::renderGuiItem(minecraft->font, minecraft->textures, item, (float)x, (float)y, fancy);
}

void Gui::renderSlotText( const ItemInstance* item, float x, float y, bool hasFinite, bool shadow )
{
	//if (!item || item->getItem()->getMaxStackSize() <= 1) {
	if (item->count <= 1) {
		return;
	}

	int c = item->count;

	char buffer[4] = {0,0,0,0};
	if (hasFinite)
		itemCountItoa(buffer, c);
	else
		buffer[0] = (char)157;

	//LOGI("slot: %d - %s\n", slot, buffer);
	if (shadow)
		minecraft->font->drawShadow(buffer, x, y, item->count>0?0xffcccccc:0x60cccccc);
	else
		minecraft->font->draw(buffer, x, y, item->count>0?0xffcccccc:0x60cccccc);
}

void Gui::inventoryUpdated() {
	_inventoryNeedsUpdate = true;
}

void Gui::onGraphicsReset() {
    inventoryUpdated();
}

void Gui::texturesLoaded( Textures* textures ) {
	//_slotFont = new Font(&minecraft->options, "gui/gui_blocks.png", textures, 0, 504, 10, 1, '0');
}

void Gui::onConfigChanged( const Config& c ) {
	Tesselator& t = Tesselator::instance;
	t.begin();

	//
	// Create outer feedback circle
	//
#ifdef ANDROID
	const float mm = 12;
#else
	const float mm = 12;
#endif
	const float maxRadius = minecraft->pixelCalcUi.millimetersToPixels(mm);
	const float radius = Mth::Min(80.0f/2, maxRadius);
	//LOGI("radius, maxradius: %f, %f\n", radius, maxRadius);
	const float radiusInner = radius * 0.95f;

	const int steps = 24;
	const float fstep = Mth::TWO_PI / steps;
	for (int i = 0; i < steps; ++i) {
		float a = i * fstep;;
		float b = a + fstep;

		float aCos = Mth::cos(a);
		float bCos = Mth::cos(b);
		float aSin = Mth::sin(a);
		float bSin = Mth::sin(b);
		float x00 = radius * aCos;
		float x01 = radiusInner * aCos;
		float x10 = radius * bCos;
		float x11 = radiusInner * bCos;
		float y00 = radius * aSin;
		float y01 = radiusInner * aSin;
		float y10 = radius * bSin;
		float y11 = radiusInner * bSin;

		t.vertexUV(x01, y01, 0, 0, 1);
		t.vertexUV(x11, y11, 0, 1, 1);
		t.vertexUV(x10, y10, 0, 1, 0);
		t.vertexUV(x00, y00, 0, 0, 0);
	}
	rcFeedbackOuter = t.end(true, rcFeedbackOuter.vboId);

	//
	// Create the inner feedback ring
	//
	t.begin(GL_TRIANGLE_FAN);
	t.vertex(0, 0, 0);
	for (int i = 0; i < steps + 1; ++i) {
		float a = -i * fstep;
		float xx = radiusInner * Mth::cos(a);
		float yy = radiusInner * Mth::sin(a);
		t.vertex(xx, yy, 0);
		//LOGI("x,y: %f, %f\n", xx, yy);
	}
	rcFeedbackInner = t.end(true, rcFeedbackInner.vboId);

	if (c.minecraft->useTouchscreen()) {
		// I'll bump this up to 6.
		int num = 6; // without "..." dots
		if (!c.minecraft->options.isJoyTouchArea && c.width > 480) {
			while (num < Inventory::MAX_SELECTION_SIZE - 1) {
				int x0, x1, y;
				getSlotPos(0, x0, y);
				getSlotPos(num, x1, y);
				int width = x1 - x0;
				float leftoverPixels = c.width - c.guiScale*width;
				if (c.pixelCalc.pixelsToMillimeters(leftoverPixels) < 80)
					break;
				num++;
			}
		}
		_numSlots = num;
#if defined(__APPLE__)
		_numSlots = Mth::Min(7, _numSlots);
#endif
	} else {
		_numSlots = Inventory::MAX_SELECTION_SIZE; // Xperia Play
	}
	MAX_MESSAGE_WIDTH = c.guiWidth;
}

float Gui::floorAlignToScreenPixel(float v) {
	return (int)(v * Gui::GuiScale) * Gui::InvGuiScale;
}

int Gui::itemCountItoa( char* buffer, int count )
{
	if (count < 0)
		return 0;

	if (count < 10) { // 1 digit
		buffer[0] = '0' + count;
		buffer[1] = 0;
		return 1;
	} else if (count < 100) { // 2 digits
		int digit = count/10;
		buffer[0] = '0' + digit;
		buffer[1] = '0' + count - digit*10;
		buffer[2] = 0;
	} else { // 3 digits -> "99+"
		buffer[0] = buffer[1] = '9';
		buffer[2] = '+';
		buffer[3] = 0;
		return 3;
	}
	return 2;
}

void Gui::tickItemDrop()
{
	// Handle item drop
	static bool isCurrentlyActive = false;
	isCurrentlyActive = false;

	int slots = getNumSlots();

#ifndef PLATFORM_DESKTOP
	if(!minecraft->javaSession) slots--;
#endif

	if (Mouse::isButtonDown(MouseAction::ACTION_LEFT)) {
		int slot = getSlotIdAt(Mouse::getX(), Mouse::getY());
		if (slot >= 0 && slot < slots) {
			if (slot != _currentDropSlot) {
				_currentDropTicks = 0;
				_currentDropSlot = slot;
			}
			isCurrentlyActive = true;
			if ((_currentDropTicks += 1.0f) >= DropTicks) {
				if (!minecraft->javaSession) minecraft->player->inventory->dropSlot(slot, false);
				minecraft->level->playSound(minecraft->player, "random.pop", 0.3f, 1);
				isCurrentlyActive = false;
			}
		}
	}
	if (!isCurrentlyActive) {
		_currentDropSlot = -1;
		_currentDropTicks = -1;
	}
}

void Gui::postError( int errCode )
{
	static std::set<int> posted;
	if (posted.find(errCode) != posted.end())
		return;

	posted.insert(errCode);

	std::stringstream s;
	s << "Something went wrong! (errcode " << errCode << ")\n";
	addMessage(s.str());
}

void Gui::setScissorRect( const IntRectangle& bbox )
{
	GLuint x = (GLuint)(GuiScale * bbox.x);
	GLuint y = minecraft->height - (GLuint)(GuiScale * (bbox.y + bbox.h));
	GLuint w = (GLuint)(GuiScale * bbox.w);
	GLuint h = (GLuint)(GuiScale * bbox.h);
	glScissor(x, y, w, h);
}

float Gui::cubeSmoothStep(float percentage, float min, float max) {
	//percentage = percentage * percentage;
	//return (min * percentage) + (max * (1 - percentage));
	return (percentage) * (percentage) * (3 - 2 * (percentage));
}

void Gui::renderProgressIndicator( const bool isTouchInterface, const int screenWidth, const int screenHeight, float a ) {
	ItemInstance* currentItem = minecraft->player->inventory->getSelected();
	bool bowEquipped = currentItem != NULL ? currentItem->getItem() == Item::bow : false;
	bool itemInUse = currentItem != NULL ? currentItem->getItem() == minecraft->player->getUseItem()->getItem() : false;
	if (!isTouchInterface || minecraft->options.isJoyTouchArea || (bowEquipped && itemInUse)) {
		minecraft->textures->loadAndBindTexture("gui/icons.png");
		glEnable(GL_BLEND);
		glBlendFunc2(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_COLOR);
		blit(screenWidth/2 - 8, screenHeight/2 - 8, 0, 0, 16, 16);
		glDisable(GL_BLEND);
	} else if(!bowEquipped) {
		const float tprogress = minecraft->gameMode->destroyProgress;
		const float alpha = Mth::clamp(minecraft->inputHolder->alpha, 0.0f, 1.0f);
		//LOGI("alpha: %f\n", alpha);

		if (tprogress <= 0 && minecraft->inputHolder->alpha >= 0) {
			glDisable2(GL_TEXTURE_2D);
			glEnable2(GL_BLEND);
			glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			if (minecraft->hitResult.isHit())
				glColor4f2(1, 1, 1, 0.8f * alpha);
			else
				glColor4f2(1, 1, 1, Mth::Min(0.4f, alpha*0.4f));

			//LOGI("alpha2: %f\n", alpha);
			const float x = InvGuiScale * minecraft->inputHolder->mousex;
			const float y = InvGuiScale * minecraft->inputHolder->mousey;
			glTranslatef2(x, y, 0);
			drawArrayVT(rcFeedbackOuter.vboId, rcFeedbackOuter.vertexCount, VertexSizeBytes);
			glTranslatef2(-x, -y, 0);

			glEnable2(GL_TEXTURE_2D);
			glDisable(GL_BLEND);
		} else if (tprogress > 0) {
			const float oProgress = minecraft->gameMode->oDestroyProgress;
			const float progress = 0.5f * (oProgress + (tprogress - oProgress) * a);

			//static Stopwatch w;
			//w.start();

			glDisable2(GL_TEXTURE_2D);
			glColor4f2(1, 1, 1, 0.8f * alpha);
			glEnable(GL_BLEND);
			glBlendFunc2(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			const float x = InvGuiScale * minecraft->inputHolder->mousex;
			const float y = InvGuiScale * minecraft->inputHolder->mousey;
			glPushMatrix2();
			glTranslatef2(x, y, 0);
			drawArrayVT(rcFeedbackOuter.vboId, rcFeedbackOuter.vertexCount, VertexSizeBytes);
			glScalef2(0.5f + progress, 0.5f + progress, 1);
			//glDisable2(GL_CULL_FACE);
			glColor4f2(1, 1, 1, 1);
			glBlendFunc2(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_COLOR);
			drawArrayVT(rcFeedbackInner.vboId, rcFeedbackInner.vertexCount, VertexSizeBytes, GL_TRIANGLE_FAN);
			glPopMatrix2();

			glDisable(GL_BLEND);
			glEnable2(GL_TEXTURE_2D);

			//w.stop();
			//w.printEvery(100, "feedback-r ");
		}
	}
}

void Gui::renderHearts() {
	bool blink = (minecraft->player->invulnerableTime / 3) % 2 == 1;
	if (minecraft->player->invulnerableTime < 10) blink = false;
	int h = minecraft->player->health;
	int oh = minecraft->player->lastHealth;
	random.setSeed(tickCount * 312871);

	int xx = 2;//screenWidth / 2 - getNumSlots() * 10;

	int armor = minecraft->player->getArmorValue();
	for (int i = 0; i < Player::MAX_HEALTH / 2; i++) {
		int yo = 2;
		int ip2 = i + i + 1;

		if (armor > 0) {
		    int xo = xx + 80 + i * 8 + 4;
		    if (ip2 < armor) blit(xo, yo, 16 + 2 * 9, 9 * 1, 9, 9);
		    else if (ip2 == armor) blit(xo, yo, 16 + 4 * 9, 9 * 1, 9, 9);
		    else if (ip2 > armor) blit(xo, yo, 16 + 0 * 9, 9 * 1, 9, 9);
		}

		int bg = 0;
		if (blink) bg = 1;
		int xo = xx + i * 8;
		if (h <= 4) {
			yo = yo + random.nextInt(2) - 1;
		}
		blit(xo, yo, 16 + bg * 9, 9 * 0, 9, 9);
		if (blink) {
			if (ip2 < oh) blit(xo, yo, 16 + 6 * 9, 9 * 0, 9, 9);
			else if (ip2 == oh) blit(xo, yo, 16 + 7 * 9, 9 * 0, 9, 9);
		}
		if (ip2 < h) blit(xo, yo, 16 + 4 * 9, 9 * 0, 9, 9);
		else if (ip2 == h) blit(xo, yo, 16 + 5 * 9, 9 * 0, 9, 9);
	}
}

void Gui::renderBubbles() {
	if (minecraft->player->isUnderLiquid(Material::water)) {
		int yo = 12;
  if(minecraft->javaSession){const auto* v=minecraft->javaSession;const auto bounded=[](double n){return std::isfinite(n)?std::max(0.,std::min(1024.,n)):0.;};const int slots=(int(std::ceil(std::max(bounded(v->maxHealth),bounded(v->health))))+1)/2+(int(std::ceil(bounded(v->absorption)))+1)/2;const int columns=std::max(1,std::min(10,(int(minecraft->width*InvGuiScale)-4)/8)),rows=(slots+columns-1)/columns;const int gap=std::max(1,std::min(10,(int(minecraft->height*InvGuiScale)/3)/std::max(1,rows)));yo=12+std::max(0,rows-1)*gap;}
		int count = (int) std::ceil((minecraft->player->airSupply - 2) * 10.0f / Player::TOTAL_AIR_SUPPLY);
		int extra = (int) std::ceil((minecraft->player->airSupply) * 10.0f / Player::TOTAL_AIR_SUPPLY) - count;
		for (int i = 0; i < count + extra; i++) {
			int xo =  i * 8 + 2;
			if (i < count) blit(xo, yo, 16, 9 * 2, 9, 9);
			else blit(xo, yo, 16 + 9, 9 * 2, 9, 9);
		}
	}
}

static OffsetPosTranslator posTranslator;
void Gui::onLevelGenerated() {
	if (Level* level = minecraft->level) {
		Pos p = level->getSharedSpawnPos();
		posTranslator = OffsetPosTranslator((float)-p.x, (float)-p.y, (float)-p.z);
	}
}

void Gui::renderDebugInfo() {
	// FPS counter (updates once per second)
	static float fps = 0.0f;
	static float fpsLastTime = 0.0f;
	static int   fpsFrames = 0;
	float now = getTimeS();
	fpsFrames++;
	if (now - fpsLastTime >= 1.0f) {
		fps = fpsFrames / (now - fpsLastTime);
		fpsFrames = 0;
		fpsLastTime = now;
	}

	LocalPlayer* p   = minecraft->player;
	Level*       lvl = minecraft->level;

	// Position
	float px = p->x, py = p->y - p->heightOffset, pz = p->z;
	posTranslator.to(px, py, pz);
	int bx = (int)floorf(px), by = (int)floorf(py), bz = (int)floorf(pz);
	int cx = bx >> 4, cz = bz >> 4;

	// Facing direction
	float yMod = fmodf(p->yRot, 360.0f);
	if (yMod < 0) yMod += 360.0f;
	const char* facing;
	const char* axis;
	if      (yMod < 45  || yMod >= 315) { facing = "South"; axis = "+Z"; }
	else if (yMod < 135)                 { facing = "West";  axis = "-X"; }
	else if (yMod < 225)                 { facing = "North"; axis = "-Z"; }
	else                                 { facing = "East";  axis = "+X"; }

	// Biome
	const char* biomeName = "unknown";
	if (lvl) {
		Biome* biome = lvl->getBiome(bx, bz);
		if (biome) biomeName = biome->name.c_str();
	}

	// Time
	long worldTime = lvl ? lvl->getTime() : 0;
	long dayTime   = worldTime % Level::TICKS_PER_DAY;
	long day       = worldTime / Level::TICKS_PER_DAY;
	long seed      = lvl ? lvl->getSeed() : 0;

	// Build lines (NULL entry = blank gap)
	static char ln[8][96];
	sprintf(ln[0], "Minecraft PE 0.6.1 alpha (TruancyCraftPE)");
	sprintf(ln[1], "%.1f fps", fps);
	ln[2][0] = '\0'; // blank separator
	sprintf(ln[3], "XYZ: %.3f / %.3f / %.3f", px, py, pz);
	sprintf(ln[4], "Block: %d %d %d   Chunk: %d %d", bx, by, bz, cx, cz);
	sprintf(ln[5], "Facing: %s (%s)  (%.1f / %.1f)", facing, axis, p->yRot, p->xRot);
	sprintf(ln[6], "Biome: %s", biomeName);
	sprintf(ln[7], "Day %ld  Time: %ld  Seed: %ld", day, dayTime, seed);

	const int N   = 8;
	const float LH  = (float)Font::DefaultLineHeight; // 10 font-pixels
	const float MGN = 2.0f;  // left/top margin in font-pixels
	const float PAD = 2.0f;  // horizontal padding for background
	Font* font = minecraft->font;

	// 1) Draw semi-transparent background boxes behind each line
	for (int i = 0; i < N; i++) {
		if (ln[i][0] == '\0') continue;
		float w  = (float)font->width(ln[i]);
		float x0 = MGN - PAD;
		float y0 = MGN + i * LH - 1.0f;
		float x1 = MGN + w + PAD;
		float y1 = MGN + (i + 1) * LH - 1.0f;
		fill(x0, y0, x1, y1, 0x90000000);
	}

	// 2) Draw text (no extra scale — font coords are in GUI units, same as fill)
	Tesselator& t = Tesselator::instance;
	t.beginOverride();
	for (int i = 0; i < N; i++) {
		if (ln[i][0] == '\0') continue;
		float y = MGN + i * LH;
		int col = (i == 0) ? 0xffFFFF55 : 0xffffffff; // title yellow, rest white
		font->draw(ln[i], MGN, y, col);
	}
	t.endOverrideAndDraw();
}

void Gui::renderSleepAnimation( const int screenWidth, const int screenHeight ) {
	int timer = minecraft->player->getSleepTimer();
	float amount = (float) timer / (float) Player::SLEEP_DURATION;
	if (amount > 1) {
		// waking up
		amount = 1.0f - ((float) (timer - Player::SLEEP_DURATION) / (float) Player::WAKE_UP_DURATION);
	}

	int color = (int) (220.0f * amount) << 24 | (0x101020);
	fill(0, 0, screenWidth, screenHeight, color);
}

void Gui::renderOnSelectItemNameText( const int screenWidth, Font* font, int ySlot ) {
	if(itemNameOverlayTime < 1.0f) {
		ItemInstance* item = minecraft->player->inventory->getSelected();
		if(item != NULL) {
			float x = float(screenWidth / 2 - font->width(item->getName()) / 2);
			float y = float(ySlot - 22);
			int alpha = 255;
			if(itemNameOverlayTime > 0.75) {
				float time = 0.25f - (itemNameOverlayTime - 0.75f);
				float percentage = cubeSmoothStep(time *  4, 0.0f, 1.0f);
				alpha = int(percentage * 255);
			}
			if(alpha != 0)
				font->drawShadow(item->getName(), x, y, 0x00ffffff + (alpha << 24));
		}
	}
}



// helper structure used by drawColoredString
struct ColorSegment {
    std::string text;
    uint32_t color;
};

// parse [tag] and [/tag] markers; tags may contain a color name (gold, green, etc.)
static void parseColorTags(const std::string& in, std::vector<ColorSegment>& out) {
    uint32_t curColor = 0xffffff;
    size_t pos = 0;
    while (pos < in.size()) {
        size_t open = in.find('[', pos);
        if (open == std::string::npos) {
            out.push_back({in.substr(pos), curColor});
            break;
        }
        if (open > pos) {
            out.push_back({in.substr(pos, open - pos), curColor});
        }
        size_t close = in.find(']', open);
        if (close == std::string::npos) {
            out.push_back({in.substr(open), curColor});
            break;
        }
        std::string tag = in.substr(open + 1, close - open - 1);
        if (!tag.empty() && tag[0] == '/') {
            curColor = 0xffffff;
        } else {
            std::string lower;
            lower.resize(tag.size());
            std::transform(tag.begin(), tag.end(), lower.begin(), ::tolower);
            if (lower.find("gold") != std::string::npos) curColor = 0xffd700;
            else if (lower.find("green") != std::string::npos) curColor = 0x00ff00;
            else if (lower.find("yellow") != std::string::npos) curColor = 0xffff00;
            else if (lower.find("red") != std::string::npos) curColor = 0xff0000;
            else if (lower.find("blue") != std::string::npos) curColor = 0x0000ff;
        }
        pos = close + 1;
    }
}

void Gui::drawColoredString(Font* font, const std::string& text, float x, float y, int alpha) {
    std::vector<ColorSegment> segs;
    parseColorTags(text, segs);
    float cx = x;
    for (auto &s : segs) {
        int color = s.color + (alpha << 24);
        font->drawShadow(s.text, cx, y, color);
        cx += font->width(s.text);
    }
}

float Gui::getColoredWidth(Font* font, const std::string& text) {
    std::vector<ColorSegment> segs;
    parseColorTags(text, segs);
    float w = 0;
    for (auto &s : segs) {
        w += font->width(s.text);
    }
    return w;
}

Gui::ChatPanelBounds Gui::incomingChatBounds(int width,int height)const{
 ChatPanelBounds box;if(width<40||height<60)return box;box.width=std::max(16,int(width*.40f));box.x=4;
 box.y=minecraft->javaSession?24:(minecraft->player&&minecraft->player->getArmorValue()>0?14:4);box.height=std::min(int(height*.20f),std::max(0,height-34-box.y));
 return box;
}

std::vector<std::string> Gui::wrapIncomingChat(Font* font,const std::string& text,int width)const{
 if(!font||width<=0)return {};
 if(minecraft->javaSession){
  std::vector<std::string> lines;std::vector<JavaRichText::Glyph> current;float used=0;
  auto emit=[&](size_t count){std::string value;for(size_t i=0;i<count;++i)value+=JavaRichText::encode(current[i]);lines.push_back(std::move(value));};
  auto measure=[&](){used=0;for(const auto& glyph:current)used+=font->javaGlyphAdvance(glyph.codepoint,glyph.bold,glyph.font);};
  for(const auto& glyph:JavaRichText::parse(text)){
   if(glyph.codepoint=='\n'){emit(current.size());current.clear();used=0;continue;}
   const float advance=font->javaGlyphAdvance(glyph.codepoint,glyph.bold,glyph.font);
   bool wrapped=false;if(!current.empty()&&used+advance>width){wrapped=true;size_t space=current.size();while(space>0&&current[space-1].codepoint!=' ')--space;
    if(space>0){emit(space-1);current.erase(current.begin(),current.begin()+space);measure();}
    else {emit(current.size());current.clear();used=0;}
    if(!current.empty()&&used+advance>width){emit(current.size());current.clear();used=0;}
   }
   if(wrapped&&current.empty()&&glyph.codepoint==' ')continue;current.push_back(glyph);used+=advance;
  }
  if(!current.empty()||lines.empty())emit(current.size());return lines;
 }

 std::vector<std::string> lines;std::string line,tag;
 for(size_t i=0;i<text.size();){size_t end=i+1;const unsigned char c=text[i];if(c=='['){const auto close=text.find(']',i);if(close!=std::string::npos){end=close+1;tag=text.substr(i,end-i);}}
 else if(c>=0xc2&&c<=0xf4){const size_t bytes=c<=0xdf?2:c<=0xef?3:4;end=std::min(text.size(),i+bytes);}
 const std::string token=text.substr(i,end-i);if(token=="\n"){lines.push_back(line);line=tag;}else{if(!line.empty()&&getColoredWidth(font,line+token)>width){lines.push_back(line);line=tag;}line+=token;}i=end;}
 if(!line.empty()||lines.empty())lines.push_back(line);return lines;
}
void Gui::renderChatMessages(const int screenHeight,unsigned int max,bool isChatting,Font* font){
 if(!font||!minecraft||isChatting||dynamic_cast<ConsoleScreen*>(minecraft->screen)||dynamic_cast<ChatScreen*>(minecraft->screen))return;const int width=int(minecraft->width*InvGuiScale);const auto box=incomingChatBounds(width,screenHeight);const int capacity=std::min(int(max),std::max(0,(box.height-2)/box.lineHeight));if(capacity<=0)return;
 struct Line{std::string text;int alpha;};std::vector<Line> lines;
 for(const auto& message:rawGuiMessages){if(message.ticks>=200&&!isChatting)continue;float fade=std::max(0.f,std::min(1.f,(1-message.ticks/200.f)*10));const int alpha=isChatting?255:int(255*fade*fade);if(alpha<=0)continue;
  for(const auto& line:wrapIncomingChat(font,message.message,box.width-6)){if(int(lines.size())>=capacity)break;lines.push_back({line,alpha});}if(int(lines.size())>=capacity)break;}
 if(lines.empty())return;int alpha=0;for(const auto& line:lines)alpha=std::max(alpha,line.alpha);
 const bool scissor=glIsEnabled(GL_SCISSOR_TEST);GLint oldScissor[4],viewport[4],blendSrc=0,blendDst=0;glGetIntegerv(GL_SCISSOR_BOX,oldScissor);glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_BLEND_SRC,&blendSrc);glGetIntegerv(GL_BLEND_DST,&blendDst);
 int left=viewport[0]+int(std::floor(float(box.x)*viewport[2]/width)),right=viewport[0]+int(std::ceil(float(box.x+box.width)*viewport[2]/width));int bottom=viewport[1]+int(std::floor(float(screenHeight-box.y-box.height)*viewport[3]/screenHeight)),top=viewport[1]+int(std::ceil(float(screenHeight-box.y)*viewport[3]/screenHeight));if(scissor){left=std::max(left,oldScissor[0]);right=std::min(right,oldScissor[0]+oldScissor[2]);bottom=std::max(bottom,oldScissor[1]);top=std::min(top,oldScissor[1]+oldScissor[3]);}glEnable2(GL_SCISSOR_TEST);glScissor(left,bottom,std::max(0,right-left),std::max(0,top-bottom));
 const bool depth=glIsEnabled(GL_DEPTH_TEST),test=glIsEnabled(GL_ALPHA_TEST),blend=glIsEnabled(GL_BLEND);glDisable2(GL_DEPTH_TEST);glDisable2(GL_ALPHA_TEST);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 fill(box.x,box.y,box.x+box.width,box.y+int(lines.size())*box.lineHeight+2,(alpha/3)<<24);
 glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 for(size_t i=0;i<lines.size();++i){const float y=box.y+1+int(i)*box.lineHeight;if(minecraft->javaSession)font->drawShadow(lines[i].text,box.x+3,y,(lines[i].alpha<<24)|0xffffff);else drawColoredString(font,lines[i].text,box.x+3,y,lines[i].alpha);}
 glScissor(oldScissor[0],oldScissor[1],oldScissor[2],oldScissor[3]);if(scissor)glEnable2(GL_SCISSOR_TEST);else glDisable2(GL_SCISSOR_TEST);glBlendFunc(blendSrc,blendDst);
 if(depth)glEnable2(GL_DEPTH_TEST);else glDisable2(GL_DEPTH_TEST);if(test)glEnable2(GL_ALPHA_TEST);else glDisable2(GL_ALPHA_TEST);if(blend)glEnable2(GL_BLEND);else glDisable2(GL_BLEND);glColor4f2(1,1,1,1);
}

void Gui::renderToolBar( float a, int ySlot, const int screenWidth ) {
	glColor4f2(1, 1, 1, .5);
	minecraft->textures->loadAndBindTexture("gui/gui.png");

	Inventory* inventory = minecraft->player->inventory;

	int xBase, yBase;
	getSlotPos(0, xBase, yBase);
	const bool java=bool(minecraft->javaSession);
 const auto bar=javaHotbarLayout(minecraft);
 const int pitch=java?bar.pitch:20;
 const float baseItemX = float(xBase)+(java?(pitch-bar.iconSize)/2:3);
	const int slotsWidth = pitch * getNumSlots();
	// Left + right side of the selection bar
	if(java && pitch<20){for(int i=0;i<9;++i)blit(xBase+i*pitch,yBase,0,0,20,22,pitch,22);}else blit(xBase, yBase, 0, 0, slotsWidth, 22);
	blit(xBase + slotsWidth, yBase, 180, 0, 2, 22);

	if (_currentDropSlot >= 0 && inventory->getItem(_currentDropSlot)) {
		int x = xBase + 3 +  _currentDropSlot * 20;
		int color = 0x8000ff00;
		int yy = (int)(17.0f * (_currentDropTicks + a) / DropTicks);

		if (_currentDropTicks >= 3) {
			glColor4f2(0, 1, 0, 0.5f);
		}
		fill(x, ySlot+16-yy, x+16, ySlot+16, color);
	}
	blit(xBase-1 + pitch*inventory->selected, yBase - 1, 0, 22, 24, 22,pitch+4,22);
	glColor4f2(1, 1, 1, 1);

	// Flash a slot background
	if (_flashSlotId >= 0) {
		const float since = getTimeS() - _flashSlotStartTime;
		if (since > 0.2f) _flashSlotId = -1;
		else {
			int x = screenWidth / 2 - getNumSlots() * 10 + _flashSlotId * 20 + 2;
			int color = 0xffffff + (((int)(/*0x80 * since +*/ 0x51 - 0x50 * Mth::cos(10 * 6.28f * since))) << 24);
			//LOGI("Color: %.8x\n", color);
			fill(x, ySlot, x+16, ySlot+16, color);
		}
	}
	glColor4f2(1, 1, 1, 1);

	//static Stopwatch w;
	//w.start();

	Tesselator& t = Tesselator::instance;
	t.beginOverride();

	float x = baseItemX;

	int slots = getNumSlots();

	// TODO: if using touchscreen

#ifndef PLATFORM_DESKTOP
	if(!minecraft->javaSession) slots--;
#endif

	for (int i = 0; i < slots; i++) {
		renderSlot(i, (int)x, ySlot, a);
		x += pitch;
	}
	_inventoryNeedsUpdate = false;

#ifndef PLATFORM_DESKTOP
 if(!java) blit(screenWidth / 2 + 10 * getNumSlots() - 20 + 4, ySlot + 6, 242, 252, 14, 4, 14, 4);
#endif

	minecraft->textures->loadAndBindTexture("gui/gui_blocks.png");
	t.endOverrideAndDraw();

 // Keep the touch frame and original three-dot glyph in their own texture draws.
 if(java && bar.touch){minecraft->textures->loadAndBindTexture("gui/gui.png");blit(bar.inventoryX,yBase,0,0,20,22);minecraft->textures->loadAndBindTexture("gui/gui_blocks.png");blit(bar.inventoryX+3,ySlot+6,242,252,14,4,14,4);}

	// Render damaged items (@todo: investigate if it's faster by drawing in same batch)
	glDisable2(GL_DEPTH_TEST);
	glDisable2(GL_TEXTURE_2D);
	t.beginOverride();
	x = baseItemX;
	for (int i = 0; i < slots; i++) {
        if(java){
          const auto& item=minecraft->javaSession->inventory.hotbar[i];
          if(item.count>0&&item.damage>0&&item.maxDamage>0){
            const float remaining=std::max(0.f,std::min(1.f,1.f-float(item.damage)/float(item.maxDamage)));
            const int green=int(std::floor(255.5f*remaining));
            const int red=255-green;
            const int width=std::max(1,bar.iconSize-3), top=ySlot+std::max(0,bar.iconSize-3);
            auto rect=[&](int width,int rgb){t.begin();t.color(rgb);t.vertex(x+2,top,0);t.vertex(x+2,top+1,0);t.vertex(x+2+width,top+1,0);t.vertex(x+2+width,top,0);t.draw();};
            rect(width,0);rect(std::max(0,width-1),((red/4)<<16)|(63<<8));rect(int(std::floor((width+.5f)*remaining)),(red<<16)|(green<<8));
          }
        }else ItemRenderer::renderGuiItemDecorations(minecraft->player->inventory->getItem(i), x, (float)ySlot);
		x += pitch;
	}
	t.endOverrideAndDraw();
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);

	//w.stop();
	//w.printEvery(100, "gui-slots");

	// Draw count
	//Tesselator& t = Tesselator::instance;
	glPushMatrix2();
	glScalef2(InvGuiScale + InvGuiScale, InvGuiScale + InvGuiScale, 1);
	const float k = 0.5f * GuiScale;

	t.beginOverride();
	if (java || minecraft->gameMode->isSurvivalType()) {
		x = baseItemX;
		for (int i = 0; i < slots; i++) {
			ItemInstance authoritative;
            if(java) authoritative.count=minecraft->javaSession->inventory.hotbar[i].count;
            ItemInstance* item = java ? &authoritative : minecraft->player->inventory->getItem(i);
			if (item && item->count >= 0)
				renderSlotText(item, k*x, k*ySlot + 1, true, true);
			x += pitch;
		}
	}
	minecraft->textures->loadAndBindTexture("font/default8.png");
	t.endOverrideAndDraw();

	glPopMatrix2();
}

void Gui::renderJavaExperience(int screenWidth,int ySlot) {
 const auto* session=minecraft->javaSession;
 if(!session||!session->spawned||!minecraft->font||screenWidth<20)return;
 const int width=std::min(182,screenWidth-16),left=(screenWidth-width)/2,top=ySlot-7;
 const double progress=std::isfinite(session->experienceProgress)?std::max(0.,std::min(1.,session->experienceProgress)):0.;
 fill(left,top,left+width,top+3,0xff18320c);
 fill(left,top,left+int(width*progress),top+3,0xff80ff20);
 if(std::isfinite(session->experienceLevel)&&session->experienceLevel>0){
  const std::string label=std::to_string(static_cast<unsigned long long>(std::min(4294967295.,session->experienceLevel)));
  minecraft->font->drawShadow(label,(screenWidth-minecraft->font->width(label))/2,top-9,0xff80ff20);
 }
}

void Gui::renderJavaVitals(int width,int height){
 const auto* v=minecraft->javaSession;if(!v||!v->spawned){javaHearts.reset();return;}
 const auto bounded=[](double n,double maximum){return std::isfinite(n)?std::max(0.,std::min(maximum,n)):0.;};
 const int hp=int(std::ceil(bounded(v->health,1024)));
 const bool blink=javaHearts.initialized&&javaHearts.world==v->worldRevision&&javaHearts.blinking(tickCount);
 javaHearts.update(v->worldRevision,hp,v->hearts.invulnerableTicks,tickCount,JavaBossState::now());
 const int maximum=int(std::ceil(std::max(bounded(v->maxHealth,1024),double(std::max(hp,javaHearts.display)))));
 const int gold=int(std::ceil(bounded(v->absorption,1024))),redSlots=(maximum+1)/2,goldSlots=(gold+1)/2;
 const int columns=std::max(1,std::min(10,(width-4)/8)),rows=(redSlots+goldSlots+columns-1)/columns;
 const int gap=std::max(1,std::min(10,(height/3)/std::max(1,rows)));
 const bool hardcore=v->hearts.hardcore;
 const auto heartKind=JavaHeartState::kind(v->hearts.poisoned,v->hearts.withered,v->hearts.frozenTicks);
 const int regeneration=v->hearts.regenerating?tickCount%(std::max(1,maximum+5)):-1;
 random.setSeed(tickCount*312871);
 Tesselator& t=Tesselator::instance;std::string heartTexture;
 auto drawHeart=[&](JavaHeartState::Kind kind,int x,int y,bool half,bool flashing){
  const bool native=!hardcore&&(kind==JavaHeartState::Normal||kind==JavaHeartState::Container);
  const std::string texture=native?"gui/icons.png":"java-hearts.png";
  if(texture!=heartTexture){if(!heartTexture.empty())t.endOverrideAndDraw();minecraft->textures->loadAndBindTexture(texture);t.beginOverride();t.colorABGR(0xffffffff);heartTexture=texture;}
  if(native)blit(x,y,kind==JavaHeartState::Container?(flashing?25:16):(flashing?(half?79:70):(half?61:52)),0,9,9);
  else blit(x,y,JavaHeartState::atlasX(hardcore,half,flashing),JavaHeartState::atlasY(kind),9,9);
 };
 for(int i=redSlots+goldSlots-1;i>=0;--i){int x=2+(i%columns)*8,y=2+(i/columns)*gap;
  if(hp+gold<=4)y+=random.nextInt(2);
  if(i<redSlots&&i==regeneration)y-=2;
  drawHeart(JavaHeartState::Container,x,y,false,blink);
  if(i>=redSlots){const int remaining=gold-(i-redSlots)*2;if(remaining>0)drawHeart(heartKind==JavaHeartState::Withered?heartKind:JavaHeartState::Absorbing,x,y,remaining==1,false);}
  if(blink&&javaHearts.display-i*2>0)drawHeart(heartKind,x,y,javaHearts.display-i*2==1,true);
  const int remaining=hp-i*2;if(remaining>0)drawHeart(heartKind,x,y,remaining==1,false);
 }
 if(!heartTexture.empty())t.endOverrideAndDraw();
 minecraft->textures->loadAndBindTexture("gui/icons.png");t.beginOverride();t.colorABGR(0xffffffff);
 // Preserve the native side-by-side row when it fits; narrow mobile GUI
 // scales place the full armor row below hearts rather than clipping icons.
 const bool narrowArmor=width<167;
 const int armorColumns=narrowArmor?columns:10;
 const int armorY=narrowArmor?2+(std::max(1,rows)-1)*gap+11:2;
 const int armor=int(std::ceil(bounded(v->armor,20)));
 for(int i=0;i<10&&armor>0;++i){const int remaining=armor-i*2;
  blit((narrowArmor?2:86)+(i%armorColumns)*8,armorY+(i/armorColumns)*10,remaining>=2?34:remaining==1?52:16,9,9,9);
 }
 t.endOverrideAndDraw();
 minecraft->textures->loadAndBindTexture("java-hud.png");t.beginOverride();t.colorABGR(0xffffffff);
 int foodY=12;
#ifndef PLATFORM_DESKTOP
 // Native touch Chat/Pause buttons occupy the top-right 22 GUI pixels.
 // Keep every hunger icon below their hit and drawing bounds.
 if(minecraft->useTouchscreen())foodY=24;
#endif
 if(narrowArmor&&armor>0)foodY=std::max(foodY,armorY+((10+armorColumns-1)/armorColumns)*10+1);
 const int food=int(bounded(v->food,20));for(int i=0;i<10;++i){const int x=width-11-(i%columns)*8,y=foodY+(i/columns)*10,remaining=food-i*2;blit(x,y,18,0,9,9);if(remaining>0)blit(x,y,remaining>=2?27:36,0,9,9);}
 t.endOverrideAndDraw();minecraft->textures->loadAndBindTexture("gui/icons.png");
 t.beginOverride();t.colorABGR(0xffffffff);renderBubbles();t.endOverrideAndDraw();
}

void Gui::renderJavaActionBar(int width,int ySlot,float partial){
 const auto* session=minecraft->javaSession;if(!session||!session->spawned||!minecraft->font||session->actionBar.empty()||width<=16)return;
 const int alpha=std::max(0,std::min(255,int((session->actionBarTicks-partial)*255.f/20.f)));if(alpha<=8)return;
 const std::string text=minecraft->font->fitJava(session->actionBar,width-16);const int textWidth=minecraft->font->widthJava(text);
 glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glPushMatrix2();glTranslatef2(width*.5f,float(ySlot-31),0);minecraft->font->drawShadow(text,-textWidth*.5f,0,(alpha<<24)|0xffffff);glPopMatrix2();
}

void Gui::renderJavaTitles(int width,int height,float partial){
 const auto* session=minecraft->javaSession;if(!session||!session->spawned||!minecraft->font||width<=16)return;const auto& title=session->titleOverlay;const int alpha=title.alpha(partial);if(alpha<=8)return;
 glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 auto line=[&](const std::string& text,float y,float size){if(text.empty())return;const int textWidth=minecraft->font->widthJava(text);const float scale=textWidth>0?std::min(size,float(width-16)/textWidth):size;glPushMatrix2();glTranslatef2(width*.5f,y,0);glScalef2(scale,scale,1);minecraft->font->drawShadow(text,-textWidth*.5f,0,(alpha<<24)|0xffffff);glPopMatrix2();};
 line(title.title,height*.4f-12,2);line(title.subtitle,height*.4f+16,1);
}

void Gui::renderJavaBossBars(int width,int height){
 const auto* session=minecraft->javaSession;if(!session||!session->spawned||!minecraft->font||width<20)return;
 const double now=JavaBossState::now();const int barWidth=std::min(182,width-16),x=(width-barWidth)/2;
 int y=32; // Below TCPE's top-edge survival HUD.
 const double maxHealth=std::isfinite(session->maxHealth)?std::max(0.,std::min(1024.,session->maxHealth)):20.;
 const double absorption=std::isfinite(session->absorption)?std::max(0.,std::min(1024.,session->absorption)):0.;
 const int slots=(int(std::ceil(maxHealth))+1)/2+(int(std::ceil(absorption))+1)/2;
 const int columns=std::max(1,std::min(10,(width-4)/8)),rows=(slots+columns-1)/columns;
 const int gap=std::max(1,std::min(10,(height/3)/std::max(1,rows)));y=std::max(y,14+std::max(0,rows-1)*gap);
 glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 for(const auto& bar:session->bossOverlay.bars){
  if(y+5>height/3)break;
  const int filled=int(182*bar.progress(now));
  minecraft->textures->loadAndBindTexture("java-bossbars.png");glPushMatrix2();glTranslatef2(float(x),float(y),0);glScalef2(float(barWidth)/182,1,1);auto& t=Tesselator::instance;t.beginOverride();t.colorABGR(0xffffffff);
  blit(0,0,0,bar.color*10,182,5);if(filled>0)blit(0,0,0,bar.color*10+5,filled,5);
  if(bar.overlay>0){const int sourceY=(6+bar.overlay)*10;blit(0,0,0,sourceY,182,5);if(filled>0)blit(0,0,0,sourceY+5,filled,5);}t.endOverrideAndDraw();glPopMatrix2();
  const std::string text=minecraft->font->fitJava(bar.text,width-16);const int textWidth=minecraft->font->widthJava(text);
  glPushMatrix2();glTranslatef2(width*.5f,float(y-10),0);minecraft->font->drawShadow(text,-textWidth*.5f,0,0xffffffff);glPopMatrix2();y+=19;
 }
}

void Gui::renderJavaScoreboard(int width,int height){
 const auto* session=minecraft->javaSession;if(!session||!session->spawned||!minecraft->font||width<=20||height<=80)return;const auto* objective=session->scoreboard.sidebar(minecraft->options.username);if(!objective)return;const auto rows=session->scoreboard.rows(*objective);if(rows.empty())return;
 Font& font=*minecraft->font;const float scale=.55f;const int availableWidth=std::max(1,int(width*.42f/scale));const size_t visibleRows=std::min(rows.size(),size_t(std::max(1,int((height-60)/(10*scale))-1)));
 const std::string title=font.fitJava(objective->text,availableWidth);int boxWidth=font.widthJava(title);std::vector<std::pair<std::string,std::string>> lines;
 for(size_t i=0;i<visibleRows;++i){const std::string number=font.fitJava(rows[i].number,availableWidth);const int numberWidth=number.empty()?0:8+font.widthJava(number);const std::string label=font.fitJava(rows[i].label,std::max(0,availableWidth-numberWidth));boxWidth=std::max(boxWidth,font.widthJava(label)+numberWidth);lines.push_back({label,number});}boxWidth=std::max(1,boxWidth);
 float top=std::max(30.f,(height-(lines.size()+1)*10*scale)*.5f);const float left=width-4-(boxWidth+4)*scale;
 glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glPushMatrix2();glTranslatef2(left,top,0);glScalef2(scale,scale,1);
 fill(-2,-1,boxWidth+2,int((lines.size()+1)*10),0x50000000);fill(-2,-1,boxWidth+2,9,0x70000000);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);font.drawShadow(title,(boxWidth-font.widthJava(title))*.5f,0,0xffffffff);
 for(size_t i=0;i<lines.size();++i){font.drawShadow(lines[i].first,0,float((i+1)*10),0xffffffff);if(!lines[i].second.empty())font.drawShadow(lines[i].second,float(boxWidth-font.widthJava(lines[i].second)),float((i+1)*10),0xffffffff);}glPopMatrix2();
}
