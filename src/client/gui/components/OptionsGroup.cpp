#include "OptionsGroup.h"
#include "../../Minecraft.h"
#include "ImageButton.h"
#include "OptionsItem.h"
#include "Slider.h"
#include "../../../locale/I18n.h"
OptionsGroup::OptionsGroup( std::string labelID )  {
	label = I18n::get(labelID);
}

void OptionsGroup::setupPositions() {
	// First we write the header and then we add the items
	int curY = y + 18;
	for(std::vector<GuiElement*>::iterator it = children.begin(); it != children.end(); ++it) {
		(*it)->width = width - 5;
		
		(*it)->y = curY;
		(*it)->x = x + 10;
		(*it)->setupPositions();
		curY += (*it)->height + 3;
	}
	height = curY;
}

void OptionsGroup::render( Minecraft* minecraft, int xm, int ym ) {
	float padX = 10.0f;
	float padY = 5.0f;
	minecraft->font->draw(label, (float)x + padX, (float)y + padY, 0xffffffff, false);
	super::render(minecraft, xm, ym);
}

OptionsGroup& OptionsGroup::addOptionItem( const Options::Option* option, Minecraft* minecraft ) {
	if(option->isBoolean())
		createToggle(option, minecraft);
	else if(option->isProgress())
		createProgressSlider(option, minecraft);
	else if(option->isInt())
		createStepSlider(option, minecraft);
	return *this;
}

OptionsGroup& OptionsGroup::addGameModeToggle(Minecraft* minecraft) {
	createGameModeToggle(minecraft);
	return *this;
}

OptionsGroup& OptionsGroup::addKeepInventoryToggle(Minecraft* minecraft) {
	createKeepInventoryToggle(minecraft);
	return *this;
}

void OptionsGroup::createToggle( const Options::Option* option, Minecraft* minecraft ) {
	ImageDef def;
	def.setSrc(IntRectangle(160, 206, 39, 20));
	def.name = "gui/touchgui.png";
	def.width = 39 * 0.7f;
	def.height = 20 * 0.7f;
	OptionButton* element = new OptionButton(option);
	element->setImageDef(def, true);
	element->updateImage(&minecraft->options);
	std::string itemLabel = Options::getCaption(option);
	OptionsItem* item = new OptionsItem(itemLabel, element, option);
	addChild(item);
	setupPositions();
}

void OptionsGroup::createGameModeToggle( Minecraft* minecraft ) {
	ImageDef def;
	def.setSrc(IntRectangle(160, 206, 39, 20));
	def.name = "gui/touchgui.png";
	def.width = 39 * 0.7f;
	def.height = 20 * 0.7f;
	GameModeOptionButton* element = new GameModeOptionButton();
	element->setImageDef(def, true);
	element->updateImage(&minecraft->options);
	OptionsItem* item = new OptionsItem("Game Mode", element, NULL);
	addChild(item);
	setupPositions();
}

void OptionsGroup::createKeepInventoryToggle( Minecraft* minecraft ) {
	ImageDef def;
	def.setSrc(IntRectangle(160, 206, 39, 20));
	def.name = "gui/touchgui.png";
	def.width = 39 * 0.7f;
	def.height = 20 * 0.7f;
	KeepInventoryOptionButton* element = new KeepInventoryOptionButton();
	element->setImageDef(def, true);
	element->updateImage(&minecraft->options);
	OptionsItem* item = new OptionsItem("Keep Inventory", element, NULL);
	addChild(item);
	setupPositions();
}

void OptionsGroup::createProgressSlider( const Options::Option* option, Minecraft* minecraft ) {
	Slider* element = new Slider(minecraft,
									option,
									minecraft->options.getProgrssMin(option),
									minecraft->options.getProgrssMax(option));
	element->width = 100;
	element->height = 20;
	std::string itemLabel = Options::getCaption(option);
	OptionsItem* item = new OptionsItem(itemLabel, element, option);
	addChild(item);
	setupPositions();
}

void OptionsGroup::createStepSlider( const Options::Option* option, Minecraft* minecraft ) {
	// integer-valued option; use step slider
	std::vector<int> steps;
	if(option == &Options::Option::RENDER_DISTANCE) {
		// Internal values are reversed: 0=Far, 1=Normal, 2=Short, 3=Tiny.
		// Present them left-to-right as Tiny -> Short -> Normal -> Far so
		// sliding right increases render distance like the rest of the UI.
		steps.push_back(3);
		steps.push_back(2);
		steps.push_back(1);
		steps.push_back(0);
	} else if(option == &Options::Option::DIFFICULTY) {
		steps.push_back(0);
		steps.push_back(1);
		steps.push_back(2);
		steps.push_back(3);
	} else if(option == &Options::Option::GUI_SCALE) {
		// slider order: small,normal,large,larger,auto
		steps.push_back(1);
		steps.push_back(2);
		steps.push_back(3);
		steps.push_back(4);
		steps.push_back(0);
	} else {
		// fallback: use single value; duplicate so numSteps>1 and avoid divide-by-zero
		steps.push_back(0);
		steps.push_back(0);
	}
	Slider* element = new Slider(minecraft, option, steps);
	element->width = 100;
	element->height = 20;
	std::string itemLabel = Options::getCaption(option);
	OptionsItem* item = new OptionsItem(itemLabel, element, option);
	addChild(item);
	setupPositions();
}
