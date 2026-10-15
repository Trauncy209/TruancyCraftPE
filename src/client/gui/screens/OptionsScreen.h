#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__OptionsScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__OptionsScreen_H__

#include "../Screen.h"
#include "../components/Button.h"

class ImageButton;
class OptionsPane;

class OptionsScreen: public Screen
{
	typedef Screen super;

	void init();
	void generateOptionScreens();
	void closeScreen();

public:
	explicit OptionsScreen(bool returnToPause = false);
	~OptionsScreen();

	void setupPositions();
	void buttonClicked(Button* button);
	void render(int xm, int ym, float a);
	void removed();
	void selectCategory(int index);
	virtual void keyPressed(int eventKey);
	virtual bool handleBackEvent(bool isDown);

	virtual void mouseClicked(int x, int y, int buttonNum);
	virtual void mouseReleased(int x, int y, int buttonNum);
	virtual void tick();

private:
	Touch::THeader* bHeader;
	ImageButton* btnClose;

	Button* btnChangeUsername;
	Button* btnCredits;
    Button* btnAccount;

	std::vector<Touch::TButton*> categoryButtons;
	std::vector<OptionsPane*> optionPanes;

	OptionsPane* currentOptionPane;

	int selectedCategory;
	bool returnToPause;
	bool showProfileButtons;
    std::vector<Touch::TButton*> bindButtons;
    Touch::TButton* bindPrevious;
    Touch::TButton* bindNext;
    Touch::TButton* bindReset;
    int bindPage, bindRows, captureBinding;
    std::string bindFeedback;
    void updateBindButtons();
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__OptionsScreen_H__*/
