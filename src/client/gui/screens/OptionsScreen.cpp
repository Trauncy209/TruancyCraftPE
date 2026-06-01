#include "OptionsScreen.h"

#include "StartMenuScreen.h"
#include "UsernameScreen.h"
#include "DialogDefinitions.h"
#include "../../Minecraft.h"
#include "../../../AppPlatform.h"
#include "../../../platform/input/Keyboard.h"
#include "CreditsScreen.h"
#include "../../../locale/I18n.h"

#include "../components/OptionsPane.h"
#include "../components/ImageButton.h"
#include "../components/OptionsGroup.h"

OptionsScreen::OptionsScreen(bool returnToPause)
	: btnClose(NULL),
		bHeader(NULL),
		btnChangeUsername(NULL),
		btnCredits(NULL),
		currentOptionPane(NULL),
		selectedCategory(0),
		returnToPause(returnToPause),
		showProfileButtons(!returnToPause) {
}

OptionsScreen::~OptionsScreen() {
	if (btnClose != NULL) delete btnClose;
	if (bHeader != NULL) delete bHeader;
	if (btnChangeUsername != NULL) delete btnChangeUsername;
	if (btnCredits != NULL) delete btnCredits;
	for (std::vector<Touch::TButton*>::iterator it = categoryButtons.begin(); it != categoryButtons.end(); ++it)
		if (*it != NULL) delete *it;
	for (std::vector<OptionsPane*>::iterator it = optionPanes.begin(); it != optionPanes.end(); ++it)
		if (*it != NULL) delete *it;
}

void OptionsScreen::init() {
	bHeader = new Touch::THeader(0, I18n::get("options.title"));
	btnClose = new ImageButton(1, "");

	ImageDef def;
	def.name = "gui/touchgui.png";
	def.width = 34;
	def.height = 26;
	def.setSrc(IntRectangle(150, 0, (int)def.width, (int)def.height));
	btnClose->setImageDef(def, true);

	categoryButtons.push_back(new Touch::TButton(2, I18n::get("options.group.game")));
	categoryButtons.push_back(new Touch::TButton(3, I18n::get("options.group.controls")));
	categoryButtons.push_back(new Touch::TButton(4, I18n::get("options.group.graphics")));
	categoryButtons.push_back(new Touch::TButton(5, I18n::get("options.group.audio")));

	buttons.push_back(bHeader);
	buttons.push_back(btnClose);


	if (showProfileButtons) {
		btnChangeUsername = new Button(10, "Username");
		btnCredits = new Button(11, "Credits");
		buttons.push_back(btnChangeUsername);
		buttons.push_back(btnCredits);
		tabButtons.push_back(btnChangeUsername);
		tabButtons.push_back(btnCredits);
	}

	for (std::vector<Touch::TButton*>::iterator it = categoryButtons.begin(); it != categoryButtons.end(); ++it) {
		buttons.push_back(*it);
		tabButtons.push_back(*it);
	}

	generateOptionScreens();
	selectCategory(selectedCategory);
}

void OptionsScreen::setupPositions() {
	int buttonHeight = btnClose->height;
	btnClose->x = width - btnClose->width;
	btnClose->y = 0;

	int offsetNum = 1;
	for (std::vector<Touch::TButton*>::iterator it = categoryButtons.begin(); it != categoryButtons.end(); ++it) {
		(*it)->x = 0;
		(*it)->y = offsetNum * buttonHeight;
		(*it)->selected = false;
		offsetNum++;
	}

	bHeader->x = 0;
	bHeader->y = 0;
	bHeader->width = width - btnClose->width;
	bHeader->height = btnClose->height;

	int paneBottom = height;
	if (showProfileButtons && btnChangeUsername != NULL && btnCredits != NULL) {
		btnChangeUsername->width = categoryButtons[0]->width;
		btnChangeUsername->height = btnClose->height;
		btnChangeUsername->x = 0;
		btnChangeUsername->y = height - btnChangeUsername->height;

		btnCredits->width = btnChangeUsername->width;
		btnCredits->height = btnChangeUsername->height;
		btnCredits->x = width - btnCredits->width;
		btnCredits->y = height - btnCredits->height;
		paneBottom = btnChangeUsername->y - 2;
	}

	for (std::vector<OptionsPane*>::iterator it = optionPanes.begin(); it != optionPanes.end(); ++it) {
		(*it)->x = categoryButtons[0]->width;
		(*it)->y = bHeader->height;
		(*it)->width = width - categoryButtons[0]->width;
		(*it)->height = paneBottom - (*it)->y;
		(*it)->setupPositions();
	}

	selectCategory(selectedCategory);
}

void OptionsScreen::render(int xm, int ym, float a) {
	renderBackground();
	super::render(xm, ym, a);
	int xmm = xm * width / minecraft->width;
	int ymm = ym * height / minecraft->height - 1;
	if (currentOptionPane != NULL) currentOptionPane->render(minecraft, xmm, ymm);
}

void OptionsScreen::removed() {}

void OptionsScreen::closeScreen() {
	minecraft->options.save();
	if (returnToPause) minecraft->screenChooser.setScreen(SCREEN_PAUSE);
	else minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
}

void OptionsScreen::buttonClicked(Button* button) {
	if (button == btnClose) closeScreen();
	else if (showProfileButtons && button == btnChangeUsername) {
		minecraft->options.save();
		minecraft->setScreen(new UsernameScreen());
	} else if (showProfileButtons && button == btnCredits) {
		minecraft->setScreen(new CreditsScreen());
	} else {
		for (size_t i = 0; i < categoryButtons.size(); ++i) {
			if (button == categoryButtons[i]) {
				selectCategory((int)i);
				break;
			}
		}
	}
}

void OptionsScreen::selectCategory(int index) {
	if (index < 0 || index >= (int)optionPanes.size()) return;
	selectedCategory = index;
	for (size_t i = 0; i < categoryButtons.size(); ++i)
		categoryButtons[i]->selected = ((int)i == index);
	currentOptionPane = optionPanes[index];
}

void OptionsScreen::generateOptionScreens() {
	optionPanes.push_back(new OptionsPane());
	optionPanes.push_back(new OptionsPane());
	optionPanes.push_back(new OptionsPane());
	optionPanes.push_back(new OptionsPane());

	OptionsGroup& gameOptions = optionPanes[0]->createOptionsGroup("options.group.game")
		.addOptionItem(&Options::Option::DIFFICULTY, minecraft);
	if (returnToPause)
		gameOptions.addGameModeToggle(minecraft);
	if (returnToPause)
		gameOptions.addOptionItem(&Options::Option::DEBUG_TICK_SPEED, minecraft);
	gameOptions
		.addOptionItem(&Options::Option::SERVER_VISIBLE, minecraft)
		.addOptionItem(&Options::Option::THIRD_PERSON, minecraft)
		.addOptionItem(&Options::Option::HIDE_GUI, minecraft)
		.addOptionItem(&Options::Option::GUI_SCALE, minecraft);

	optionPanes[1]->createOptionsGroup("options.group.controls")
		.addOptionItem(&Options::Option::SENSITIVITY, minecraft)
		.addOptionItem(&Options::Option::FOV, minecraft)
		.addOptionItem(&Options::Option::INVERT_MOUSE, minecraft)
		.addOptionItem(&Options::Option::PIXELS_PER_MILLIMETER, minecraft)
		.addOptionItem(&Options::Option::LEFT_HANDED, minecraft)
		.addOptionItem(&Options::Option::USE_TOUCHSCREEN, minecraft)
		.addOptionItem(&Options::Option::USE_TOUCH_JOYPAD, minecraft)
		.addOptionItem(&Options::Option::CLASSIC_CONTROLS, minecraft)
		.addOptionItem(&Options::Option::DESTROY_VIBRATION, minecraft);

	optionPanes[2]->createOptionsGroup("options.group.graphics")
		.addOptionItem(&Options::Option::RENDER_DISTANCE, minecraft)
		.addOptionItem(&Options::Option::GRAPHICS, minecraft)
		.addOptionItem(&Options::Option::VIEW_BOBBING, minecraft)
		.addOptionItem(&Options::Option::AMBIENT_OCCLUSION, minecraft)
		.addOptionItem(&Options::Option::ANAGLYPH, minecraft)
		.addOptionItem(&Options::Option::LIMIT_FRAMERATE, minecraft)
		.addOptionItem(&Options::Option::VSYNC, minecraft);

	optionPanes[3]->createOptionsGroup("options.group.audio")
		.addOptionItem(&Options::Option::MUSIC, minecraft)
		.addOptionItem(&Options::Option::SOUND, minecraft);
}

void OptionsScreen::keyPressed(int eventKey) {
	if (eventKey == Keyboard::KEY_ESCAPE) {
		closeScreen();
		return;
	}
	super::keyPressed(eventKey);
}

bool OptionsScreen::handleBackEvent(bool isDown) {
	if (!isDown) closeScreen();
	return true;
}

void OptionsScreen::mouseClicked(int x, int y, int buttonNum) {
	if (currentOptionPane != NULL) currentOptionPane->mouseClicked(minecraft, x, y, buttonNum);
	super::mouseClicked(x, y, buttonNum);
}

void OptionsScreen::mouseReleased(int x, int y, int buttonNum) {
	if (currentOptionPane != NULL) currentOptionPane->mouseReleased(minecraft, x, y, buttonNum);
	super::mouseReleased(x, y, buttonNum);
}

void OptionsScreen::tick() {
	if (currentOptionPane != NULL) currentOptionPane->tick(minecraft);
	super::tick();
}
