#include "../NativeServerListing.h"
#include "../DisconnectionScreen.h"
#include "../JavaAccountScreen.h"
#include "../JavaServerRow.h"
#include "TouchJoinGameScreen.h"
#include "../StartMenuScreen.h"
#include "../ProgressScreen.h"
#include "../JavaConnectScreen.h"
#include "../../Font.h"
#include "../../../Minecraft.h"
#include "../../../SavedServerList.h"
#include "../../../renderer/Textures.h"
#include "../../../../network/ClientSideNetworkHandler.h"


namespace Touch {

void AvailableGamesList::selectStart(int item) {
	startSelected = item;
}

void AvailableGamesList::selectCancel() {
	startSelected = -1;
}

void AvailableGamesList::selectItem(int item, bool doubleClick) {
	selectedItem = item;
}

void AvailableGamesList::renderItem(int i, int x, int y, int h, Tesselator& t)
{
	const JoinGameListEntry& s = copiedServerList[i];
    const int left = (int)x0 + 6;
    const int right = (int)x1 - 6;
    const int bottom = y + h;
    const int borderColor = isSelectedItem(i) ? 0xffd8e6ff : 0xff202020;
    const int panelColor = isSelectedItem(i) ? 0xff181818 : 0xff080808;
    unsigned int titleColor = s.isSpecial ? 0x6090a0 : 0xffffffff;
    unsigned int subtitleColor = s.isSaved ? 0xffd0d0d0 : 0xffa8ff90;

	if (startSelected == i && Multitouch::getFirstActivePointerIdEx() >= 0) {
		fill(left, y, right, bottom, 0x809E684F);
	}

    fill(left, y, right, bottom, borderColor);
    fill(left + 1, y + 1, right - 1, bottom - 1, panelColor);

    JavaServerRow::render(minecraft,s.statusHost.empty()?s.savedHost:s.statusHost,s.savedPort,s.title,s.isSpecial,left+4,right-4,y+1,s.isNative,s.isLan);
}

JoinGameScreen::JoinGameScreen()
:	bJoin(2, "Join"),
    bDelete(5, "Delete"),
    bAddServer(4, "Add"),
	bBack(3, "Back"),
	bHeader(0, "Servers"),
	gamesList(NULL)
{
	bJoin.active = false;
    bDelete.active = false;
}

JoinGameScreen::~JoinGameScreen()
{
	delete gamesList;
}

bool JoinGameScreen::canDeleteSelection() const
{
    return gamesList && gamesList->selectedItem >= 0 && gamesList->selectedItem < (int)gamesList->copiedServerList.size()
        && gamesList->copiedServerList[gamesList->selectedItem].isSaved;
}

void JoinGameScreen::init()
{
    minecraft->locateMultiplayer();
	buttons.push_back(&bHeader);
	buttons.push_back(&bBack);
    buttons.push_back(&bAddServer);
    buttons.push_back(&bJoin);
    buttons.push_back(&bDelete);

	gamesList = new AvailableGamesList(minecraft, width, height);

#ifdef ANDROID
	tabButtons.push_back(&bBack);
	tabButtons.push_back(&bAddServer);
    tabButtons.push_back(&bJoin);
    tabButtons.push_back(&bDelete);
#endif
}

void JoinGameScreen::setupPositions() {
    const int topPadding = 4;
    const int bottomY = height - 30;
    const int bottomButtonWidth = 90;
    const int gap = 8;
    const int leftX = width / 2 - bottomButtonWidth - gap / 2;
    const int rightX = width / 2 + gap / 2;

	bBack.y = topPadding;
    bAddServer.y = topPadding;
	bHeader.y = topPadding;

	bBack.x = 0;
    bAddServer.x = width - bAddServer.width;
	bHeader.x = bBack.width;
	bHeader.width = width - (bBack.width + bAddServer.width);
    bHeader.xText = width / 2;

    bDelete.width = bottomButtonWidth;
    bJoin.width = bottomButtonWidth;
    bDelete.y = bottomY;
    bJoin.y = bottomY;
    bDelete.x = leftX;
    bJoin.x = rightX;
}

void JoinGameScreen::buttonClicked(Button* button)
{
	if (button->id == bJoin.id)
	{
		if (isIndexValid(gamesList->selectedItem))
		{
			JoinGameListEntry selectedServer = gamesList->copiedServerList[gamesList->selectedItem];
            if(selectedServer.isNative){
                if(selectedServer.statusHost.empty())return;
                minecraft->stopJavaServer();minecraft->raknetInstance->setClientProtocol(CLIENT_PROTOCOL_CLASSIC);
                PingedCompatibleServer native;native.address=RakNet::SystemAddress(selectedServer.statusHost.c_str(),selectedServer.savedPort);
                if(minecraft->joinMultiplayer(native))minecraft->setScreen(new ProgressScreen());else minecraft->setScreen(new DisconnectionScreen("Could not connect to TCPE server"));
                return;
            }
            minecraft->cancelLocateMultiplayer();
            const bool online=JavaAccountSelection::selected();
            const std::string identity=online?JavaAccountSelection::username():minecraft->options.username;
            if(minecraft->joinJavaServer(selectedServer.javaAddress,identity,online))minecraft->setScreen(new JavaConnectScreen());
            else minecraft->setScreen(new JavaConnectScreen(JavaConnectScreen::connectionFailure(identity,online)));
		}
	}
    if (button->id == bDelete.id)
    {
        if (canDeleteSelection()) {
            const JoinGameListEntry& selectedServer = gamesList->copiedServerList[gamesList->selectedItem];
            if (SavedServerList::remove(selectedServer.savedHost,selectedServer.savedPort,selectedServer.isNative)) {
                gamesList->selectItem(-1, false);
                bJoin.active = false;
                bDelete.active = false;
            }
        }
    }
	if (button->id == bBack.id)
	{
		minecraft->cancelLocateMultiplayer();
		minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
	}
    if (button->id == bAddServer.id)
    {
        minecraft->screenChooser.setScreen(SCREEN_JOINBYIP);
    }
}

bool JoinGameScreen::handleBackEvent(bool isDown)
{
	if (!isDown)
	{
		minecraft->cancelLocateMultiplayer();
		minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
	}
	return true;
}

bool JoinGameScreen::isIndexValid(int index)
{
	return gamesList && index >= 0 && index < gamesList->getNumberOfItems();
}

void JoinGameScreen::tick()
{
    std::vector<SavedServerEntry> savedServers = SavedServerList::load();
    refreshNativeServerDiscovery(minecraft,savedServers);
    std::vector<JoinGameListEntry> serverList = visibleServerEntries<Touch::JoinGameListEntry>(savedServers,minecraft->raknetInstance->getServerList());
    std::vector<JavaServerRow::Endpoint> endpoints;for(const auto& server:serverList)if(!server.statusHost.empty())endpoints.push_back({server.statusHost,server.savedPort});JavaServerRow::refresh(minecraft,endpoints);

    std::string selectedAddress;
    bool hasSelection = false;
    if (isIndexValid(gamesList->selectedItem)) {
        selectedAddress = gamesList->copiedServerList[gamesList->selectedItem].javaAddress;
        hasSelection = true;
    }

	gamesList->copiedServerList = serverList;
	gamesList->selectItem(-1, false);

	if (hasSelection)
	{
		for (unsigned int i = 0; i < gamesList->copiedServerList.size(); ++i)
		{
			if (gamesList->copiedServerList[i].javaAddress == selectedAddress)
			{
				gamesList->selectItem(i, false);
				break;
			}
		}
	}

	bJoin.active = isIndexValid(gamesList->selectedItem)&&(!gamesList->copiedServerList[gamesList->selectedItem].isNative||!gamesList->copiedServerList[gamesList->selectedItem].statusHost.empty());
    bDelete.active = canDeleteSelection();
}

void JoinGameScreen::render(int xm, int ym, float a)
{
	renderBackground();
 gamesList->render(xm,ym,a);Screen::render(xm,ym,a);
 if(gamesList->copiedServerList.empty())drawCenteredString(minecraft->font,"Add a server to join",width/2,height/2-8,0xffcccccc);
}

bool JoinGameScreen::isInGameScreen() { return false; }

};
