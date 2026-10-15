#include "TextBox.h"
#include "../../../platform/input/Utf8KeyText.h"
#include "../Gui.h"
#include "../../Minecraft.h"
#include "../../../AppPlatform.h"
#include "../../../platform/input/Mouse.h"

// delegate constructors
TextBox::TextBox(int id, const std::string& msg)
 : TextBox(id, 0, 0, msg)
{
}

TextBox::TextBox(int id, int x, int y, const std::string& msg)
 : TextBox(id, x, y, 24, Font::DefaultLineHeight + 4, msg)
{
}

TextBox::TextBox(int id, int x, int y, int w, int h, const std::string& msg)
 : GuiElement(true, true, x, y, w, h),
   id(id), hint(msg), focused(false), blink(false), blinkTicks(0)
{
}

void TextBox::setFocus(Minecraft* minecraft) {
    if (!focused) {
        minecraft->platform()->showKeyboard();
        focused = true;
        blinkTicks = 0;
        blink = false;
    }
}

bool TextBox::loseFocus(Minecraft* minecraft) {
    if (focused) {
        minecraft->platform()->hideKeyboard();
        focused = false;
        return true;
    }
    return false;
}

void TextBox::mouseClicked(Minecraft* minecraft, int x, int y, int buttonNum) {
    if (buttonNum == MouseAction::ACTION_LEFT) {
        if (pointInside(x, y)) {
            setFocus(minecraft);
        } else {
            loseFocus(minecraft);
        }
    }
}

void TextBox::handleChar(char c) {
    if (!focused) {
        return;
    }

    if (c == '\b' || c == 127) {
        if (!text.empty()) {
            Utf8KeyText::eraseLast(text);
        }
        return;
    }

    Utf8KeyText::appendByte(text,c,256);
}

void TextBox::handleKey(int key) {
    if (focused && key == Keyboard::KEY_BACKSPACE && !text.empty()) {
        Utf8KeyText::eraseLast(text);
    }
}

void TextBox::tick(Minecraft* minecraft) {
    blinkTicks++;
    if (blinkTicks >= 5) {
        blink = !blink;
        blinkTicks = 0;
    }
}

void TextBox::render(Minecraft* minecraft, int xm, int ym) {
    // textbox like in beta 1.7.3
    // keep the focused state subtle so Android fields do not turn solid yellow.
    uint32_t bgColor = focused ? 0xffc0c0c0 : 0xffa0a0a0;
    uint32_t borderColor = 0xff000000;
    fill(x, y, x + width, y + height, borderColor);
    fill(x + 1, y + 1, x + width - 1, y + height - 1, bgColor);

    glEnable2(GL_SCISSOR_TEST);
    glScissor(
        Gui::GuiScale * (x + 2),
        minecraft->height - Gui::GuiScale * (y + height - 2),
        Gui::GuiScale * (width - 2),
        Gui::GuiScale * (height - 2)
    );

    int textY = y + (height - Font::DefaultLineHeight) / 2;
    int textX = x + 3;

    if (text.empty() && !focused) {
        minecraft->font->draw(hint, (float)textX, (float)textY, 0xff5e5e5e);
    }

    if (focused && blink) text.push_back('_');

    minecraft->font->draw(text, (float)textX, (float)textY, 0xffffffff);

    if (focused && blink) text.pop_back();

    glDisable2(GL_SCISSOR_TEST);
}
