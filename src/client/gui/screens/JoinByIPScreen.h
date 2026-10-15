#include "../Screen.h"
#include "../components/Button.h"
#include "../../Minecraft.h"
#include "../components/ImageButton.h"
#include "../components/TextBox.h"

class JoinByIPScreen: public Screen
{
public:
	JoinByIPScreen();
	virtual ~JoinByIPScreen();

	void init();
	void setupPositions();

	virtual void tick();
    void render(int xm, int ym, float a);

	virtual void keyPressed(int eventKey);
	void buttonClicked(Button* button);
    virtual bool handleBackEvent(bool isDown);
private:
    bool native=false;
    Touch::TButton bProtocol;
    TextBox tName;
    TextBox tIP;
    TextBox tPort;
    Touch::THeader bHeader;
	Touch::TButton bJoin;
	ImageButton bBack;
};
