#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__JoinGameScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__JoinGameScreen_H__

#include <string>
#include <vector>

#include "../Screen.h"
#include "../components/Button.h"
#include "../components/SmallButton.h"
#include "../components/ScrolledSelectionList.h"
#include "../../Minecraft.h"
#include "../../../network/RakNetInstance.h"

struct JoinGameListEntry {
    std::string title;
    std::string subtitle;
    std::string javaAddress;
    std::string statusHost;
    std::string savedHost;unsigned short savedPort=25565;
    RakNet::SystemAddress address;
    bool isSpecial;
    bool isSaved;
    bool isNative=false,isLan=false;
};

class JoinGameScreen;

class AvailableGamesList : public ScrolledSelectionList
{
	int selectedItem;
	std::vector<JoinGameListEntry> copiedServerList;

	friend class JoinGameScreen;

public:
	AvailableGamesList(Minecraft* _minecraft, int _width, int _height)
	:	ScrolledSelectionList(_minecraft, _width, _height, 24, _height - 30, 44),
        selectedItem(-1)
	{ setWideRows(true);
	}

protected:
	virtual int getNumberOfItems() { return (int)copiedServerList.size(); }
	virtual void selectItem(int item, bool doubleClick) { selectedItem = item; }
	virtual bool isSelectedItem(int item) { return item == selectedItem; }
	virtual void renderBackground() {}
	virtual void renderItem(int i, int x, int y, int h, Tesselator& t);
};

class JoinGameScreen: public Screen
{
public:
	JoinGameScreen();
	virtual ~JoinGameScreen();

	void init();
	void setupPositions();

	virtual bool handleBackEvent(bool isDown);
	virtual bool isIndexValid(int index);
	virtual void tick();

	void render(int xm, int ym, float a);
	void buttonClicked(Button* button);
	bool isInGameScreen();
private:
    bool canDeleteSelection() const;

	Button bJoin;
    Button bDelete;
	Button bBack;
    Button bAddServer;
	AvailableGamesList* gamesList;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__JoinGameScreen_H__*/
