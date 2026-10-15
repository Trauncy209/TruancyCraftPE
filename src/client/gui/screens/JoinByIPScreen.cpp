#include "JoinByIPScreen.h"

#include <cstdlib>

#include "../../SavedServerList.h"
#include "client/gui/Screen.h"
#include "client/gui/components/TextBox.h"

JoinByIPScreen::JoinByIPScreen() :
    bProtocol(6,"Type: Java"),
    tName(0, "Server Name"),
    tIP(1, "Address or IP"),
    tPort(2, "Port"),
    bHeader(3, "Server Details"),
	bJoin(4, "Save Server"),
	bBack(5, "")
{
	bJoin.active = false;
}

JoinByIPScreen::~JoinByIPScreen()
{
}

void JoinByIPScreen::buttonClicked(Button* button)
{
    if(button==&bProtocol){native=!native;bProtocol.msg=native?"Type: TCPE":"Type: Java";return;}
	if (button->id == bJoin.id)
	{
        SavedServerEntry entry;
        entry.native=native;
        entry.name = tName.text;
        entry.address = tIP.text;
        entry.port = std::atoi(tPort.text.c_str());
        if (SavedServerList::addOrUpdate(entry)) {
            minecraft->screenChooser.setScreen(SCREEN_JOINGAME);
        }
	}
	if (button->id == bBack.id)
	{
		minecraft->screenChooser.setScreen(SCREEN_JOINGAME);
	}
}

bool JoinByIPScreen::handleBackEvent(bool isDown)
{
	if (!isDown)
	{
		minecraft->screenChooser.setScreen(SCREEN_JOINGAME);
	}
	return true;
}

void JoinByIPScreen::tick()
{
	Screen::tick();
	bJoin.active = !tIP.text.empty() && !tPort.text.empty();
}

void JoinByIPScreen::init()
{
    ImageDef def;
	def.name = "gui/touchgui.png";
	def.width = 34;
	def.height = 26;
	def.setSrc(IntRectangle(150, 0, (int)def.width, (int)def.height));
	bBack.setImageDef(def, true);

	buttons.push_back(&bProtocol);
    buttons.push_back(&bJoin);
	buttons.push_back(&bBack);
	buttons.push_back(&bHeader);
    textBoxes.push_back(&tName);
    textBoxes.push_back(&tIP);
    textBoxes.push_back(&tPort);
#ifdef ANDROID
	tabButtons.push_back(&bProtocol);
    tabButtons.push_back(&bJoin);
	tabButtons.push_back(&bBack);
    tabButtons.push_back(&bHeader);
#endif
    tPort.text = "25565";
}

void JoinByIPScreen::setupPositions() {
    int boxWidth = width - 48;
    if (boxWidth > 260) boxWidth = 260;
    const int portWidth = 82;
    const int boxHeight = 20;
    const int labelGap = 9;
    const int centerX = (width - boxWidth) / 2;
    const int baseY = height / 2 - 50;

	bBack.x = width - bBack.width;
	bBack.y = 0;
	bHeader.x = 0;
	bHeader.y = 0;
	bHeader.width = width - bBack.width;
    bJoin.width = 110;

    tName.x = centerX;
    tName.y = baseY;
    tName.width = boxWidth-96;
    bProtocol.x=tName.x+tName.width+6;bProtocol.y=tName.y;bProtocol.width=90;bProtocol.height=boxHeight;
    tName.height = boxHeight;

    tIP.x = centerX;
    tIP.y = baseY + boxHeight + 14 + labelGap;
    tIP.width = boxWidth - portWidth - 6;
    tIP.height = boxHeight;

    tPort.x = tIP.x + tIP.width + 6;
    tPort.y = tIP.y;
    tPort.width = portWidth;
    tPort.height = boxHeight;

    bJoin.x = (width - bJoin.width) / 2;
	bJoin.y = tIP.y + boxHeight + 18 + labelGap;
}

void JoinByIPScreen::render( int xm, int ym, float a )
{
	renderBackground();

    const int cardLeft = tName.x - 12;
    const int cardTop = tName.y - 18;
    const int cardRight = tPort.x + tPort.width + 12;
    const int cardBottom = bJoin.y + bJoin.height + 12;
    fill(cardLeft, cardTop, cardRight, cardBottom, 0xff202020);
    fill(cardLeft + 1, cardTop + 1, cardRight - 1, cardBottom - 1, 0x70404040);

    minecraft->font->draw("Name", (float)tName.x, (float)(tName.y - 10), 0xffffffff);
    minecraft->font->draw("Address", (float)tIP.x, (float)(tIP.y - 10), 0xffffffff);
    minecraft->font->draw("Port", (float)tPort.x, (float)(tPort.y - 10), 0xffffffff);

	Screen::render(xm, ym, a);
}

void JoinByIPScreen::keyPressed(int eventKey)
{
    if (eventKey == Keyboard::KEY_ESCAPE) {
        minecraft->screenChooser.setScreen(SCREEN_JOINGAME);
        return;
    }
    Screen::keyPressed(eventKey);
}
