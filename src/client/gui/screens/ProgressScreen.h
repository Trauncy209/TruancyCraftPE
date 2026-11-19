#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__ProgressScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__ProgressScreen_H__

#include "../Screen.h"

class ProgressScreen: public Screen
{
public:
	ProgressScreen();

	void render(int xm, int ym, float a);
	bool isInGameScreen();

	void tick();
    void keyPressed(int eventKey) override;
    bool handleBackEvent(bool isDown) override;
private:
    void cancelNativeJoin();

};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__ProgressScreen_H__*/
