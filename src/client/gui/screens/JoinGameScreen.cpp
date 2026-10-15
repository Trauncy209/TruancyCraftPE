#include "./JavaAccountScreen.h"
#include "JavaServerRow.h"
#include "JoinGameScreen.h"
#include "StartMenuScreen.h"
#include "ProgressScreen.h"
#include "JavaConnectScreen.h"
#include "../Font.h"
#include "../../SavedServerList.h"
#include "../../../network/ClientSideNetworkHandler.h"
#include "../../../network/RakNetInstance.h"

namespace {
std::vector<JoinGameListEntry> buildVisibleServerList(const std::vector<SavedServerEntry>& savedServers) {
    std::vector<JoinGameListEntry> serverList;

    JoinGameListEntry featured;featured.title="Zeus";featured.subtitle="Featured server";featured.savedHost="zeuspvp.net";featured.savedPort=25565;featured.javaAddress="zeuspvp.net:25565";featured.isSpecial=true;featured.isSaved=false;serverList.push_back(featured);
    for (size_t i = 0; i < savedServers.size(); ++i) {
        std::string normalized=savedServers[i].address;std::transform(normalized.begin(),normalized.end(),normalized.begin(),[](unsigned char c){return std::tolower(c);});if(normalized=="zeuspvp.net"&&savedServers[i].port==25565)continue;
        JoinGameListEntry entry;
        entry.savedHost=savedServers[i].address;entry.savedPort=savedServers[i].port;
        entry.javaAddress = (savedServers[i].address.find(':')!=std::string::npos&&savedServers[i].address.front()!='['?"["+savedServers[i].address+"]":savedServers[i].address) + ":" + std::to_string(savedServers[i].port);
        entry.title = savedServers[i].name.empty() ? entry.javaAddress : savedServers[i].name;
        entry.subtitle = entry.javaAddress;
        entry.isSpecial = false;
        entry.isSaved = true;
        serverList.push_back(entry);
    }



    return serverList;
}


}

void AvailableGamesList::renderItem(int i, int x, int y, int h, Tesselator& t)
{
    const JoinGameListEntry& s = copiedServerList[i];
    const int left = rowLeft();
    const int right = rowRight();
    const int bottom = y + h - 2;
    const int panelColor = isSelectedItem(i) ? 0xff181818 : 0xff080808;
    const int borderColor = isSelectedItem(i) ? 0xffd8e6ff : 0xff202020;
    unsigned int titleColor = s.isSpecial ? 0xff00b0 : 0xffffffff;
    unsigned int subtitleColor = s.isSaved ? 0xffd0d0d0 : 0xffa8ff90;

    fill(left, y - 1, right, bottom, borderColor);
    fill(left + 1, y, right - 1, bottom - 1, panelColor);

    JavaServerRow::render(minecraft,s.savedHost,s.savedPort,s.title,s.isSpecial,left+3,right-3,y);
}

JoinGameScreen::JoinGameScreen()
:	bJoin(2, "Join Game"),
    bDelete(5, "Delete"),
	bBack(3, "Back"),
    bAddServer(4, "Add Server"),
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

void JoinGameScreen::buttonClicked(Button* button)
{
	if (button->id == bJoin.id)
	{
		if (isIndexValid(gamesList->selectedItem))
		{
			JoinGameListEntry selectedServer = gamesList->copiedServerList[gamesList->selectedItem];
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
            if (SavedServerList::remove(selectedServer.savedHost,selectedServer.savedPort)) {
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
    std::vector<JoinGameListEntry> serverList = buildVisibleServerList(savedServers);
    std::vector<JavaServerRow::Endpoint> endpoints;for(const auto& server:serverList)endpoints.push_back({server.savedHost,server.savedPort});JavaServerRow::refresh(minecraft,endpoints);

    std::string selectedAddress;
    bool hasSelection = false;
    if (isIndexValid(gamesList->selectedItem)) {
        selectedAddress = gamesList->copiedServerList[gamesList->selectedItem].javaAddress;
        hasSelection = true;
    }

    gamesList->copiedServerList = serverList;
    gamesList->selectItem(-1, false);

    if (hasSelection) {
        for (unsigned int i = 0; i < gamesList->copiedServerList.size(); ++i)
        {
            if (gamesList->copiedServerList[i].javaAddress == selectedAddress)
            {
                gamesList->selectItem(i, false);
                break;
            }
        }
    }

	bJoin.active = isIndexValid(gamesList->selectedItem);
    bDelete.active = canDeleteSelection();
}

void JoinGameScreen::init()
{
	buttons.push_back(&bJoin);
    buttons.push_back(&bDelete);
	buttons.push_back(&bBack);
    buttons.push_back(&bAddServer);

	gamesList = new AvailableGamesList(minecraft, width, height);

#ifdef ANDROID
	tabButtons.push_back(&bJoin);
    tabButtons.push_back(&bDelete);
	tabButtons.push_back(&bBack);
    tabButtons.push_back(&bAddServer);
#endif
}

void JoinGameScreen::setupPositions() {
	int yBase = height - 26;
    const int buttonWidth = 100;
    const int gap = 6;
    const int startX = (width - buttonWidth * 3 - gap * 2) / 2;

	bJoin.y = yBase;
    bDelete.y = yBase;
	bBack.y = yBase;
	bJoin.width = buttonWidth;
    bDelete.width = buttonWidth;
	bBack.width = buttonWidth;

	bJoin.x = startX;
    bDelete.x = startX + buttonWidth + gap;
	bBack.x = startX + (buttonWidth + gap) * 2;

    bAddServer.width = 120;
    bAddServer.y = 4;
    bAddServer.x = width - bAddServer.width - 8;
}

void JoinGameScreen::render(int xm, int ym, float a)
{
	renderBackground();
 gamesList->render(xm,ym,a);Screen::render(xm,ym,a);
 drawCenteredString(minecraft->font,"Servers",width/2,8,0xffffffff);
 if(gamesList->copiedServerList.empty())drawCenteredString(minecraft->font,"Add a server to join",width/2,height/2-8,0xffcccccc);
}

bool JoinGameScreen::isInGameScreen() { return false; }
