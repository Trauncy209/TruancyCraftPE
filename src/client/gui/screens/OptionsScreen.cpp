#include "../../../world/level/Level.h"
#include "OptionsScreen.h"

#include "StartMenuScreen.h"
#include "UsernameScreen.h"
#include "DialogDefinitions.h"
#include "../../Minecraft.h"
#include "../../../AppPlatform.h"
#include "../../../platform/input/Keyboard.h"
#include "../../../platform/input/ControllerActionKeys.h"
#include "CreditsScreen.h"
#include "JavaAccountScreen.h"
#include "../../../locale/I18n.h"

#include "../components/OptionsPane.h"
#include "../components/ImageButton.h"
#include "../components/OptionsGroup.h"

namespace {
class KeyBindingButton : public Touch::TButton {
public:
    bool conflicted = false;
    KeyBindingButton(int id, const std::string& label) : Touch::TButton(id, label) {}
protected:
    void renderFace(Minecraft* mc, int xm, int ym) override {
        if (!conflicted || !active) { Button::renderFace(mc, xm, ym); return; }
        drawCenteredString(mc->font, msg, x + width / 2, y + (height - 8) / 2, 0xffff7070);
    }
};
bool hasBindingConflicts(const Options& options) {
    for (int i = 0; i < Options::KeyMappingCount; ++i)
        if (options.keyConflict(i, options.keyMappings[i]->key) >= 0) return true;
    return false;
}
}

OptionsScreen::OptionsScreen(bool returnToPause)
	: btnClose(NULL),
		bHeader(NULL),
		btnChangeUsername(NULL),
		btnCredits(NULL),
        btnAccount(NULL),
		currentOptionPane(NULL),
		selectedCategory(0),
		returnToPause(returnToPause),
		showProfileButtons(!returnToPause), bindPrevious(NULL), bindNext(NULL), bindReset(NULL),
        bindPage(0), bindRows(4), captureBinding(-1) {
}

OptionsScreen::~OptionsScreen() {
    for (auto* button : bindButtons) delete button;
    delete bindPrevious; delete bindNext; delete bindReset;
	if (btnClose != NULL) delete btnClose;
	if (bHeader != NULL) delete bHeader;
	if (btnChangeUsername != NULL) delete btnChangeUsername;
	if (btnCredits != NULL) delete btnCredits;
    delete btnAccount;
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
    categoryButtons.push_back(new Touch::TButton(6, "Keyboard"));
    for (int i = 0; i < 8; ++i) {
        auto* button = new KeyBindingButton(100 + i, "");
        bindButtons.push_back(button); buttons.push_back(button); tabButtons.push_back(button);
    }
    bindPrevious = new Touch::TButton(120, "<");
    bindNext = new Touch::TButton(121, ">");
    bindReset = new Touch::TButton(122, "Defaults");
    buttons.push_back(bindPrevious); buttons.push_back(bindNext); buttons.push_back(bindReset);
    tabButtons.push_back(bindPrevious); tabButtons.push_back(bindNext); tabButtons.push_back(bindReset);

	buttons.push_back(bHeader);
	buttons.push_back(btnClose);
    btnAccount=new Button(12,"Account");buttons.push_back(btnAccount);tabButtons.push_back(btnAccount);


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

	int offsetNum = 0;
    bool profileVisible = showProfileButtons && height >= 140;
    bool accountVisible = height >= 140;
    int categoryHeight = std::min(buttonHeight, std::max(10, (height - buttonHeight - (accountVisible ? buttonHeight : 0)) / 5));
	for (std::vector<Touch::TButton*>::iterator it = categoryButtons.begin(); it != categoryButtons.end(); ++it) {
		(*it)->x = 0;
		(*it)->y = buttonHeight + offsetNum * categoryHeight;
        (*it)->height = categoryHeight;
		(*it)->selected = false;
		offsetNum++;
	}

	bHeader->x = 0;
	bHeader->y = 0;
	bHeader->width = width - btnClose->width;
	bHeader->height = btnClose->height;

	int paneBottom = height;
	if (btnChangeUsername) btnChangeUsername->visible = btnChangeUsername->active = profileVisible;
    if (btnCredits) btnCredits->visible = btnCredits->active = profileVisible;
    if (profileVisible && btnChangeUsername != NULL && btnCredits != NULL) {
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

    btnAccount->visible=btnAccount->active=accountVisible;
    btnAccount->width=std::min(128,std::max(48,width-(profileVisible?2*categoryButtons[0]->width:0)-8));
    btnAccount->height=buttonHeight;btnAccount->x=(width-btnAccount->width)/2;btnAccount->y=height-buttonHeight;
    if(accountVisible)paneBottom=std::min(paneBottom,btnAccount->y-2);

	for (std::vector<OptionsPane*>::iterator it = optionPanes.begin(); it != optionPanes.end(); ++it) {
		(*it)->x = categoryButtons[0]->width;
		(*it)->y = bHeader->height;
		(*it)->width = width - categoryButtons[0]->width;
		(*it)->height = std::max(1,paneBottom - (*it)->y - 24);
		(*it)->setupPositions();
	}

    int left = categoryButtons[0]->width + 4;
    int usableWidth = std::max(40, width - left - 4);
    int headingSpace = height < 140 ? 14 : 20;
    bindRows = std::max(1, std::min(8, (paneBottom - buttonHeight - headingSpace - 34) / 22));
    for (int i = 0; i < 8; ++i) {
        bindButtons[i]->x = left; bindButtons[i]->y = buttonHeight + headingSpace + i * 22;
        bindButtons[i]->width = usableWidth; bindButtons[i]->height = 20;
    }
    int arrowWidth = usableWidth < 110 ? 20 : 28;
    bindPrevious->x = left; bindNext->x = left + arrowWidth + 2;
    bindPrevious->width = bindNext->width = arrowWidth;
    bindReset->msg = usableWidth < 110 ? "Reset" : "Defaults";
    bindReset->x = left + (arrowWidth + 2) * 2; bindReset->width = std::max(1, usableWidth - (arrowWidth + 2) * 2);
    bindPrevious->height = bindNext->height = bindReset->height = 20;
    bindPrevious->y = bindNext->y = bindReset->y = paneBottom - 22;
    selectCategory(selectedCategory);
}

void OptionsScreen::render(int xm, int ym, float a) {
	renderBackground();
	super::render(xm, ym, a);
	int xmm = xm * width / minecraft->width;
	int ymm = ym * height / minecraft->height - 1;
	if (currentOptionPane != NULL) currentOptionPane->render(minecraft, xmm, ymm);
    if (selectedCategory == 4) {
        int x = categoryButtons[0]->width + 4;
        std::string hint = height < 140 ? (captureBinding >= 0 ? "Press key" : "Tap action") : (captureBinding >= 0 ? "Press key; Esc cancels" : "Tap an action to change");
        while (!hint.empty() && minecraft->font->width(hint) > width - x - 4) hint.pop_back();
        drawString(minecraft->font, hint, x, bHeader->height + 4, 0xffffffff);
        bool conflicts = hasBindingConflicts(minecraft->options);
        std::string status = conflicts && captureBinding < 0 ? "Warning: shared keys" : bindFeedback.empty() ? std::to_string(bindPage + 1) + " / " + std::to_string((Options::KeyMappingCount + bindRows - 1) / bindRows) : bindFeedback;
        while (!status.empty() && minecraft->font->width(status) > width - x - 4) status.pop_back();
        drawString(minecraft->font, status, x, bindReset->y - 11, conflicts ? 0xffffa060 : 0xffeeeeaa);
    }
}

void OptionsScreen::removed() {}

void OptionsScreen::closeScreen() {
	minecraft->options.save();
	if (returnToPause) minecraft->screenChooser.setScreen(SCREEN_PAUSE);
	else minecraft->screenChooser.setScreen(SCREEN_STARTMENU);
}

void OptionsScreen::buttonClicked(Button* button) {
	if (selectedCategory == 4 && button->id >= 100 && button->id < 108) {
        captureBinding = bindPage * bindRows + button->id - 100;
        bindFeedback = "Backspace clears binding"; updateBindButtons(); return;
    }
    if (button == bindPrevious || button == bindNext) {
        if(selectedCategory!=4&&currentOptionPane){currentOptionPane->scrollPage(button==bindNext?1:-1);updateBindButtons();return;}
        captureBinding = -1; bindFeedback.clear();
        bindPage += button == bindNext ? 1 : -1; updateBindButtons(); return;
    }
    if (button == bindReset) {
        minecraft->options.resetKeys(); captureBinding = -1;
        bindFeedback = "Default keys restored"; updateBindButtons(); return;
    }
    if(button==btnAccount){minecraft->options.save();minecraft->setScreen(new JavaAccountScreen(returnToPause));}
    else if (button == btnClose) closeScreen();
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
	if (index < 0 || index >= (int)categoryButtons.size()) return;
	selectedCategory = index;
	for (size_t i = 0; i < categoryButtons.size(); ++i)
		categoryButtons[i]->selected = ((int)i == index);
	currentOptionPane = index < (int)optionPanes.size() ? optionPanes[index] : NULL;
    captureBinding = -1; bindFeedback.clear(); updateBindButtons();
}

void OptionsScreen::generateOptionScreens() {
	optionPanes.push_back(new OptionsPane());
	optionPanes.push_back(new OptionsPane());
	optionPanes.push_back(new OptionsPane());
	optionPanes.push_back(new OptionsPane());

	OptionsGroup& gameOptions = optionPanes[0]->createOptionsGroup("options.group.game");
	if (!minecraft->level || !minecraft->level->isClientSide)
		gameOptions.addOptionItem(&Options::Option::DIFFICULTY, minecraft);
	if (returnToPause && minecraft->level && !minecraft->level->isClientSide)
		gameOptions.addGameModeToggle(minecraft);
	if (returnToPause && minecraft->level && !minecraft->level->isClientSide)
		gameOptions.addKeepInventoryToggle(minecraft);
	gameOptions.addOptionItem(&Options::Option::DEBUG_MENU, minecraft);
	if (returnToPause && minecraft->level && !minecraft->level->isClientSide)
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
    if (captureBinding >= 0) {
        if (eventKey == Keyboard::KEY_ESCAPE || eventKey == ControllerKeys::MenuCancel) { captureBinding = -1; bindFeedback = "Cancelled"; }
        else if((eventKey>=ControllerKeys::Forward&&eventKey<=ControllerKeys::Pause)||eventKey==99||eventKey==100){bindFeedback="Use keyboard to bind";}
        else {
            int key = eventKey == Keyboard::KEY_BACKSPACE ? 0 : eventKey;
            int conflict = minecraft->options.keyConflict(captureBinding, key);
            minecraft->options.setKey(captureBinding, key);
            captureBinding = -1;
            bindFeedback = conflict >= 0 ? "Shared key saved" : "Binding saved";
        }
        updateBindButtons(); return;
    }
	if (eventKey == Keyboard::KEY_ESCAPE || eventKey == ControllerKeys::MenuCancel || (eventKey && eventKey == minecraft->options.keyMenuCancel.key)) {
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

void OptionsScreen::updateBindButtons() {
    int pages = (Options::KeyMappingCount + bindRows - 1) / bindRows;
    bindPage = std::max(0, std::min(bindPage, pages - 1));
    bool visible = selectedCategory == 4;
    for (int row = 0; row < (int)bindButtons.size(); ++row) {
        int index = bindPage * bindRows + row;
        auto* button = bindButtons[row];
        button->visible = button->active = visible && row < bindRows && index < Options::KeyMappingCount;
        if (!button->visible) continue;
        static_cast<KeyBindingButton*>(button)->conflicted = minecraft->options.keyConflict(index, minecraft->options.keyMappings[index]->key) >= 0;
        std::string action = minecraft->options.getKeyDescription(index);
        std::string suffix = ": " + (captureBinding == index ? "..." : minecraft->options.getKeyMessage(index));
        while (action.size() > 1 && minecraft->font->width(action + suffix) > button->width - 8) action.pop_back();
        button->msg = action + suffix;
        button->selected = captureBinding == index;
    }
    const bool panePaging=!visible&&currentOptionPane&&(currentOptionPane->canScroll(-1)||currentOptionPane->canScroll(1));
    bindPrevious->visible = bindNext->visible = visible||panePaging;
    bindReset->visible = visible;
    bindPrevious->active = visible ? bindPage > 0 : panePaging&&currentOptionPane->canScroll(-1);
    bindNext->active = visible ? bindPage + 1 < pages : panePaging&&currentOptionPane->canScroll(1);
    bindReset->active = visible;
}
