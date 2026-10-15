#include "OptionsPane.h"
#include "OptionsGroup.h"
#include "OptionsItem.h"
#include "ImageButton.h"
#include "Slider.h"
#include "../../Minecraft.h"
#include "../Gui.h"
#include <algorithm>

OptionsPane::OptionsPane() : scrollOffset(0), contentHeight(0) {

}

void OptionsPane::setupPositions() {
	int currentHeight = y + 1 - scrollOffset;
	for(std::vector<GuiElement*>::iterator it = children.begin(); it != children.end(); ++it ) {
		(*it)->width = width;
		(*it)->y = currentHeight;
		(*it)->x = x;
		(*it)->setupPositions();
		currentHeight += (*it)->height + 1;
	}
    contentHeight = currentHeight - y + scrollOffset;
    int bounded = std::max(0,std::min(scrollOffset,std::max(0,contentHeight-height)));
    if(bounded!=scrollOffset){scrollOffset=bounded;setupPositions();}
}

bool OptionsPane::canScroll(int direction) const {
    return direction<0?scrollOffset>0:scrollOffset<std::max(0,contentHeight-height);
}
void OptionsPane::scrollPage(int direction){
    scrollOffset=std::max(0,std::min(scrollOffset+direction*std::max(20,height-20),std::max(0,contentHeight-height)));
    setupPositions();
}
void OptionsPane::render(Minecraft* minecraft,int xm,int ym){
    GLint previous[4];glGetIntegerv(GL_SCISSOR_BOX,previous);
    const GLboolean enabled=glIsEnabled(GL_SCISSOR_TEST);
    glEnable2(GL_SCISSOR_TEST);
    glScissor(int(Gui::GuiScale*x),minecraft->height-int(Gui::GuiScale*(y+height)),int(Gui::GuiScale*width),int(Gui::GuiScale*height));
    super::render(minecraft,xm,ym);
    glScissor(previous[0],previous[1],previous[2],previous[3]);
    if(!enabled)glDisable2(GL_SCISSOR_TEST);
}
void OptionsPane::mouseClicked(Minecraft* minecraft,int px,int py,int button){
    if(px>=x&&px<x+width&&py>=y&&py<y+height)super::mouseClicked(minecraft,px,py,button);
}
void OptionsPane::mouseReleased(Minecraft* minecraft,int px,int py,int button){
    super::mouseReleased(minecraft,px,py,button);
}

OptionsGroup& OptionsPane::createOptionsGroup( std::string label ) {
	OptionsGroup* newGroup = new OptionsGroup(label);
	children.push_back(newGroup);
	// create and return a new group index
	return *newGroup;
}

void OptionsPane::createToggle( unsigned int group, std::string label, const Options::Option* option ) {
// 	if(group > children.size()) return;
// 	ImageDef def;
// 	def.setSrc(IntRectangle(160, 206, 39, 20));
// 	def.name = "gui/touchgui.png";
// 	def.width = 39 * 0.7f;
// 	def.height = 20 * 0.7f;
// 	OptionButton* element = new OptionButton(option);
// 	element->setImageDef(def, true);
// 	OptionsItem* item = new OptionsItem(label, element);
// 	((OptionsGroup*)children[group])->addChild(item);
// 	setupPositions();
}

void OptionsPane::createProgressSlider( Minecraft* minecraft, unsigned int group, std::string label, const Options::Option* option, float progressMin/*=1.0f*/, float progressMax/*=1.0f */ ) {
// 	if(group > children.size()) return;
// 	Slider* element = new Slider(minecraft, option, progressMin, progressMax);
// 	element->width = 100;
// 	element->height = 20;
// 	OptionsItem* item = new OptionsItem(label, element);
// 	((OptionsGroup*)children[group])->addChild(item);
// 	setupPositions();
}

void OptionsPane::createStepSlider( Minecraft* minecraft, unsigned int group, std::string label, const Options::Option* option, const std::vector<int>& stepVec ) {
// 	if(group > children.size()) return;
// 	Slider* element = new Slider(minecraft, option, stepVec);
// 	element->width = 100;
// 	element->height = 20;
// 	sliders.push_back(element);
// 	OptionsItem* item = new OptionsItem(label, element);
// 	((OptionsGroup*)children[group])->addChild(item);
// 	setupPositions();
}
