#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__DemoChooseLevelScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__DemoChooseLevelScreen_H__

#include "ChooseLevelScreen.h"
#include "../components/TextBox.h"
#include "../components/Button.h"    // for Touch::THeader
class Button;
class ImageButton;

class SimpleChooseLevelScreen: public ChooseLevelScreen
{
public:
	SimpleChooseLevelScreen(const std::string& levelName);
	virtual ~SimpleChooseLevelScreen();
	void init();
	void setupPositions();
	void tick();
	void render(int xm, int ym, float a);
	void buttonClicked(Button* button);
	bool handleBackEvent(bool isDown);
	virtual void keyPressed(int eventKey);
	virtual void keyboardNewChar(char inputChar);
	virtual void mouseClicked(int x, int y, int buttonNum);

private:
	Touch::THeader* bHeader;
	Button* bGamemode;
	Button* bWorldType;
	Button* bWorldOptions;
	Button* bOptionsBack;
	Button* bOptionCaves;
	Button* bOptionRavines;
	Button* bOptionWaterLakes;
	Button* bOptionLavaLakes;
	Button* bOptionWaterSprings;
	Button* bOptionLavaSprings;
	Button* bOptionBiomeGrassTint;
	Button* bOptionTallGrass;
	Button* bOptionBetaWorldGeneration;
	Button* bOptionModernTerrainGeneration;
	Button* bOptionExperimentalFeatures;
	ImageButton* bBack;
	Button* bCreate;
	bool hasChosen;
	bool inWorldOptions;
	std::string levelName;
	int gamemode;
	int generatorVersion;
	bool optCaves;
	bool optRavines;
	bool optWaterLakes;
	bool optLavaLakes;
	bool optWaterSprings;
	bool optLavaSprings;
	bool optBiomeGrassTint;
	bool optTallGrass;
	bool optBetaWorldGeneration;
	bool optModernTerrainGeneration;
	bool optExperimentalFeatures;
	int worldOptionsScroll;
	void applyGeneratorDefaults();
	void refreshWorldOptionLabels();
	void setWorldOptionsVisible(bool visible);
	TextBox tLevelName;
	TextBox tSeed;
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__DemoChooseLevelScreen_H__*/
