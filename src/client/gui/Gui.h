#ifndef NET_MINECRAFT_CLIENT_GUI__Gui_H__
#define NET_MINECRAFT_CLIENT_GUI__Gui_H__

//package net.minecraft.client.gui;

#include "GuiComponent.h"
#include "Font.h"
#include "../player/input/touchscreen/TouchAreaModel.h"
#include "../renderer/RenderChunk.h"
#include "../../util/Random.h"
#include "../IConfigListener.h"
#include "../../network/java/JavaHeartState.h"

class Minecraft;
class ItemInstance;
class Textures;
class Tesselator;
struct IntRectangle;

struct GuiMessage
{
	std::string message;
 std::string signatureId;
	int ticks;
};

typedef std::vector<GuiMessage> GuiMessageList;

class Gui: public GuiComponent, IConfigListener
{
public:
    Gui(Minecraft* minecraft);
	~Gui();

	int getSlotIdAt(int x, int y);
	int getItemBarSelectionSize() const;
	void flashSlot(int slotId);
	bool isInside(int x, int y);
	RectangleArea getRectangleArea(int extendSide);
	void getSlotPos(int slot, int& posX, int& posY);
	int getNumSlots();

	void handleClick(int button, int x, int y);
	void handleKeyPressed( int key );

	void tick();
	void render(float a, bool mouseFree, int xMouse, int yMouse);

	void renderToolBar( float a, int ySlot, const int screenWidth );

	void renderChatMessages( const int screenHeight, unsigned int max, bool isChatting, Font* font );

	// draw a string containing simple [color]...[/color] tags; color names are matched
	// case-insensitively and default to white. alpha is applied to each segment.
	// draw tagged string (ignores simple [color]…[/color] tags)
	static void drawColoredString(Font* font, const std::string& text, float x, float y, int alpha);
	static float getColoredWidth(Font* font, const std::string& text);

	void renderOnSelectItemNameText( const int screenWidth, Font* font, int ySlot );

	void renderSleepAnimation( const int screenWidth, const int screenHeight );

	void renderBubbles();
	void renderHearts();
 void renderJavaVitals(int screenWidth,int screenHeight);
 void renderJavaExperience(int screenWidth,int ySlot);
 void renderJavaActionBar(int screenWidth,int ySlot,float partial);
 void renderJavaScoreboard(int screenWidth,int screenHeight);
 void renderJavaBossBars(int screenWidth,int screenHeight);
 void renderJavaTitles(int screenWidth,int screenHeight,float partial);
	void renderDebugInfo();

	void renderProgressIndicator( const bool isTouchInterface, const int screenWidth, const int screenHeight, float a );

    void addMessage(const std::string& string,const std::string& signatureId="");
 void deleteMessageBySignature(const std::string& signatureId);
	const GuiMessageList& getMessages() const { return guiMessages; }
 const GuiMessageList& getRawMessages() const { return rawGuiMessages; }
 struct ChatPanelBounds {int x=0,y=0,width=0,height=0,lineHeight=10;};
 ChatPanelBounds incomingChatBounds(int width,int height) const;
 std::vector<std::string> wrapIncomingChat(Font* font,const std::string& text,int width) const;
	void postError(int errCode);

    void onGraphicsReset();
	void inventoryUpdated();

	void setNowPlaying(const std::string& string);
	void displayClientMessage(const std::string& messageId);
	void renderSlotText(const ItemInstance* item, float x, float y, bool hasFinite, bool shadow);
	void texturesLoaded( Textures* textures );

	void onConfigChanged(const Config& config);
	void onLevelGenerated();

	void setScissorRect(const IntRectangle& rect);

	static float floorAlignToScreenPixel(float);
	static int itemCountItoa(char* buf, int count);
private:
	void renderVignette(float br, int w, int h);
	void renderSlot(int slot, int x, int y, float a);
	void tickItemDrop();
	float cubeSmoothStep(float percentage, float min, float max);
public:
	float progress;
	std::string selectedName;
	static float InvGuiScale;
	static float GuiScale;

private:
	int MAX_MESSAGE_WIDTH;
	//ItemRenderer itemRenderer;
	GuiMessageList guiMessages;
 GuiMessageList rawGuiMessages;
	Random random;

	Minecraft* minecraft;
	int tickCount;
 JavaHeartState javaHearts;
	float itemNameOverlayTime;
	std::string overlayMessageString;
	int overlayMessageTime;
	bool animateOverlayMessageColor;

	float tbr;

	RenderChunk _inventoryRc;
	bool _inventoryNeedsUpdate;
	
	int _flashSlotId;
	float _flashSlotStartTime;

	Font* _slotFont;
	int _numSlots;

	RenderChunk rcFeedbackOuter;
	RenderChunk rcFeedbackInner;

	// For dropping
	static const float DropTicks;
	float  _currentDropTicks;
	int    _currentDropSlot;
};

#endif /*NET_MINECRAFT_CLIENT_GUI__Gui_H__*/
