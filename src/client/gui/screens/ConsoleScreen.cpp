#include "../../../network/java/JavaGameSession.h"
#include "ConsoleScreen.h"
#include "../../NativeWeatherCommand.h"
#include "JavaChatActionsScreen.h"
#include "../Gui.h"
#include "../components/Button.h"
#include "../Font.h"
#include "../../../network/java/JavaRichText.h"
#include "../../Minecraft.h"
#include "../../player/LocalPlayer.h"
#include "../../../platform/input/Keyboard.h"
#include "../../../world/level/Level.h"
#include "../../../network/RakNetInstance.h"
#include "../../../network/ServerSideNetworkHandler.h"
#include "../../../network/packet/ChatPacket.h"

#include <sstream>
#include <cstdlib>
#include <cctype>
#include <algorithm>

namespace {
static void eraseLastUtf8(std::string& value){if(value.empty())return;size_t start=value.size()-1;while(start>0&&(static_cast<unsigned char>(value[start])&0xc0)==0x80)--start;value.erase(start);}
static size_t pendingChatUtf8(const std::string& value){
    if(value.empty())return 0;size_t start=value.size()-1;
    while(start>0&&(static_cast<unsigned char>(value[start])&0xc0)==0x80)--start;
    const auto lead=static_cast<unsigned char>(value[start]);
    const size_t expected=lead>=0xc2&&lead<=0xdf?2:lead>=0xe0&&lead<=0xef?3:lead>=0xf0&&lead<=0xf4?4:0;
    return expected&&value.size()-start<expected?value.size()-start:0;
}
static void discardIncompleteChatUtf8(std::string& value){const size_t pending=pendingChatUtf8(value);if(pending)value.erase(value.size()-pending);}
static void appendChatByte(std::string& value,char input){
    const auto byte=static_cast<unsigned char>(input);const size_t pending=pendingChatUtf8(value);
    if(pending){
        const auto lead=static_cast<unsigned char>(value[value.size()-pending]);
        const bool continuation=(byte&0xc0)==0x80;
        const bool scalar=!(pending==1&&((lead==0xe0&&byte<0xa0)||(lead==0xed&&byte>=0xa0)||(lead==0xf0&&byte<0x90)||(lead==0xf4&&byte>=0x90)));
        if(continuation&&scalar&&value.size()<1027){value+=input;return;}
        discardIncompleteChatUtf8(value);if(continuation)return;
    }
    if(value.size()>=1024)return;
    if((byte>=32&&byte<127)||(byte>=0xc2&&byte<=0xf4))value+=input;
}
static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t");
    return s.substr(a, b - a + 1);
}

// Keep TCPE's local color tags while using the same styled wrapping as Java chat.
static std::string nativeChatStyle(const std::string& text){
    std::string out;size_t pos=0;uint32_t color=0xffffff;
    auto marker=[&](){const char* hex="0123456789abcdef";out+=u8"§x";for(int shift=20;shift>=0;shift-=4){out+=u8"§";out+=hex[(color>>shift)&15];}};
    while(pos<text.size()){
        const size_t open=text.find('[',pos);if(open==std::string::npos){out+=text.substr(pos);break;}
        out+=text.substr(pos,open-pos);const size_t close=text.find(']',open);
        if(close==std::string::npos){out+=text.substr(open);break;}
        std::string tag=text.substr(open+1,close-open-1);
        if(!tag.empty()&&tag[0]=='/')color=0xffffff;
        else{for(char& c:tag)c=char(std::tolower(static_cast<unsigned char>(c)));
            if(tag.find("gold")!=std::string::npos)color=0xffd700;
            else if(tag.find("green")!=std::string::npos)color=0x00ff00;
            else if(tag.find("yellow")!=std::string::npos)color=0xffff00;
            else if(tag.find("red")!=std::string::npos)color=0xff0000;
            else if(tag.find("blue")!=std::string::npos)color=0x0000ff;}
        marker();pos=close+1;
    }
    return out;
}
static std::vector<std::string> wrapChatHistory(Font* font,const std::string& text,int maxWidth){
    std::vector<std::string> lines;std::string line;float used=0;std::vector<JavaRichText::Glyph> word;
    auto finish=[&](){lines.push_back(line);line.clear();used=0;};
    auto flushWord=[&](){float wordWidth=0;for(const auto& glyph:word)wordWidth+=font->javaGlyphAdvance(glyph.codepoint,glyph.bold,glyph.font);
        if(!line.empty()&&used+wordWidth>maxWidth)finish();
        for(const auto& glyph:word){const float advance=font->javaGlyphAdvance(glyph.codepoint,glyph.bold,glyph.font);if(!line.empty()&&used+advance>maxWidth)finish();line+=JavaRichText::encode(glyph);used+=advance;}word.clear();};
    for(const auto& glyph:JavaRichText::parse(text)){
        if(glyph.codepoint=='\n'){flushWord();finish();}
        else if(glyph.codepoint==' '||glyph.codepoint=='\t'){flushWord();const float advance=font->javaGlyphAdvance(' ',glyph.bold,glyph.font);if(!line.empty()&&used+advance>maxWidth)finish();else{auto space=glyph;space.codepoint=' ';line+=JavaRichText::encode(space);used+=advance;}}
        else word.push_back(glyph);
    }
    flushWord();if(!line.empty()||lines.empty())lines.push_back(line);return lines;
}
struct ChatLayout {
    int historyLeft,historyRight,historyTop,historyBottom;
    int inputLeft,inputRight,inputTop,inputBottom;
    int titleY,lineHeight;
};
static ChatLayout buildLayout(int width,int height,bool) {
    ChatLayout l;
    l.historyLeft=10;l.historyRight=width-10;l.historyTop=36;
    // A fullscreen IME can leave only a short landscape surface. Keep
    // history between the header and input instead of forcing a row into
    // the navigation buttons when there is no room for both.
    l.historyBottom=std::max(l.historyTop,height-(height>=116?70:34));
    l.inputLeft=6;l.inputRight=width-70;l.inputTop=height-30;l.inputBottom=height-6;
    l.titleY=14;l.lineHeight=10;
    return l;
}

}

ConsoleScreen::ConsoleScreen()
:   _historyDragging(false),
    _dragStartY(0),
    _dragStartOffset(0),
    _input(""),
    _cursorBlink(0),
    _historyOffset(0)
{
}

ConsoleScreen::~ConsoleScreen(){for(auto* button:_ownedButtons)delete button;}
void ConsoleScreen::init()
{
    _historyOffset=0;
    for(auto* button:_ownedButtons)delete button;
    _ownedButtons.clear();buttons.clear();tabButtons.clear();
    const char* labels[]={"Close","Links","Older","Latest","Send",""};
    for(int i=0;i<6;++i){Button* b=minecraft->useTouchscreen()?static_cast<Button*>(new Touch::TButton(i,labels[i])):new Button(i,labels[i]);_ownedButtons.push_back(b);buttons.push_back(b);tabButtons.push_back(b);}
    setupPositions();
    if(minecraft->useTouchscreen())minecraft->platform()->showKeyboard();
}
void ConsoleScreen::setupPositions(){
    if(_ownedButtons.size()!=6)return;
    auto set=[&](int i,int x,int y,int w,int h){auto* b=_ownedButtons[i];b->x=x;b->y=y;b->width=std::max(1,w);b->height=h;};
    set(0,6,6,52,24);set(1,width-58,6,52,24);
    set(2,6,height-62,58,24);set(3,width-64,height-62,58,24);
    set(4,width-64,height-30,58,24);set(5,6,height-30,width-76,24);
    _ownedButtons[1]->visible=minecraft->javaSession&&minecraft->javaSession->chatActionsPending;
    _ownedButtons[2]->visible=_ownedButtons[3]->visible=height>=116;
}
void ConsoleScreen::buttonClicked(Button* button){
    switch(button->id){case 0:minecraft->setScreen(NULL);break;
    case 1:if(minecraft->javaSession&&minecraft->javaSession->chatActionsPending)minecraft->setScreen(new JavaChatActionsScreen(*minecraft->javaSession));break;
    case 2:scrollHistory(std::max(1,getVisibleHistoryLines()-1));break;
    case 3:_historyOffset=0;break;
    case 4:execute(false);break;
    case 5:if(minecraft->useTouchscreen())minecraft->platform()->showKeyboard();break;}
}

void ConsoleScreen::removed()
{
    if (minecraft && minecraft->useTouchscreen())
        minecraft->platform()->hideKeyboard();
}

void ConsoleScreen::tick()
{
    _cursorBlink++;
}

bool ConsoleScreen::handleBackEvent(bool /*isDown*/)
{
    minecraft->setScreen(NULL);
    return true;
}

int ConsoleScreen::getVisibleHistoryLines() const {
    const auto l=buildLayout(width,height,minecraft->useTouchscreen());
    return std::max(0,(l.historyBottom-l.historyTop)/l.lineHeight);
}
std::vector<std::string> ConsoleScreen::historyLines()const {
    std::vector<std::string> lines;
    const auto l=buildLayout(width,height,minecraft->useTouchscreen());
    const auto& messages=minecraft->gui.getRawMessages();
    for(const auto& message:messages){auto wrapped=wrapChatHistory(font,minecraft->javaSession?message.message:nativeChatStyle(message.message),std::max(1,l.historyRight-l.historyLeft-2));for(auto i=wrapped.rbegin();i!=wrapped.rend();++i)lines.push_back(*i);}
    return lines;
}
int ConsoleScreen::getMaxHistoryOffset() const {
    return std::max(0,int(historyLines().size())-getVisibleHistoryLines());
}

void ConsoleScreen::scrollHistory(int delta)
{
    _historyOffset += delta;
    if (_historyOffset < 0)
        _historyOffset = 0;
    int maxOffset = getMaxHistoryOffset();
    if (_historyOffset > maxOffset)
        _historyOffset = maxOffset;
}

void ConsoleScreen::keyPressed(int eventKey)
{
    if (eventKey == Keyboard::KEY_ESCAPE) {
        minecraft->setScreen(NULL);
    } else if (eventKey == Keyboard::KEY_RETURN) {
        execute(!minecraft->useTouchscreen());
    } else if (eventKey == Keyboard::KEY_BACKSPACE) {
        eraseLastUtf8(_input);
    } else if (eventKey == 38) {
        scrollHistory(minecraft->useTouchscreen() ? 4 : 1);
    } else if (eventKey == 40) {
        scrollHistory(minecraft->useTouchscreen() ? -4 : -1);
    } else {
        super::keyPressed(eventKey);
    }
}

void ConsoleScreen::keyboardNewChar(char inputChar){appendChatByte(_input,inputChar);}

void ConsoleScreen::mouseClicked(int x,int y,int buttonNum){
    if(buttonNum!=MouseAction::ACTION_LEFT)return;
    const auto l=buildLayout(width,height,minecraft->useTouchscreen());
    if(x>=l.historyLeft&&x<l.historyRight&&y>=l.historyTop&&y<l.historyBottom){_historyDragging=true;_dragStartY=y;_dragStartOffset=_historyOffset;return;}
    super::mouseClicked(x,y,buttonNum);
}
void ConsoleScreen::mouseReleased(int x,int y,int buttonNum){
    if(_historyDragging&&buttonNum==MouseAction::ACTION_LEFT){_historyOffset=_dragStartOffset;scrollHistory((y-_dragStartY)/10);_historyDragging=false;return;}
    super::mouseReleased(x,y,buttonNum);
}

void ConsoleScreen::execute(bool closeAfter)
{
    discardIncompleteChatUtf8(_input);
    if (trim(_input).empty()) {
        if (closeAfter)
            minecraft->setScreen(NULL);
        return;
    }

    if (minecraft->javaSession) {
        minecraft->javaSession->connection.chat(_input);
    } else if (NativeWeatherCommand::dispatch(*minecraft,_input)) {
        // Local owner execution or unchanged remote-server forwarding.
    } else if (_input[0] == '/') {
        std::string result = processCommand(_input);
        if (!result.empty())
            minecraft->gui.addMessage(result);
    } else {
        std::string msg = std::string("<") + minecraft->player->name + "> " + _input;
        if (minecraft->netCallback && minecraft->raknetInstance->isServer()) {
            static_cast<ServerSideNetworkHandler*>(minecraft->netCallback)->displayGameMessage(msg);
        } else if (minecraft->netCallback) {
            ChatPacket chatPkt(msg);
            minecraft->raknetInstance->send(chatPkt);
        } else {
            minecraft->gui.addMessage(msg);
        }
    }

    _input.clear();
    _cursorBlink = 0;
    _historyOffset = 0;

    if (closeAfter)
        minecraft->setScreen(NULL);
    else if (minecraft->useTouchscreen())
        minecraft->platform()->showKeyboard();
}

std::string ConsoleScreen::processCommand(const std::string& raw)
{
    std::string line = raw;
    if (!line.empty() && line[0] == '/') line = line.substr(1);
    line = trim(line);

    std::vector<std::string> args;
    {
        std::istringstream ss(line);
        std::string tok;
        while (ss >> tok) args.push_back(tok);
    }

    if (args.empty()) return "";

    Level* level = minecraft->level;
    if (!level) return "No level loaded.";

    if (args[0] == "time") {
        if (args.size() < 2)
            return "Usage: /time <add|set|query> ...";

        const std::string& sub = args[1];

        if (sub == "add") {
            if (args.size() < 3) return "Usage: /time add <value>";
            long delta = std::atol(args[2].c_str());
            long newTime = level->getTime() + delta;
            level->setTime(newTime);
            std::ostringstream out;
            out << "Set the time to " << (newTime % Level::TICKS_PER_DAY);
            return out.str();
        }

        if (sub == "set") {
            if (args.size() < 3) return "Usage: /time set <value|day|night|noon|midnight>";
            const std::string& val = args[2];

            long t = -1;
            if      (val == "day")      t = 1000;
            else if (val == "noon")     t = 6000;
            else if (val == "night")    t = 13000;
            else if (val == "midnight") t = 18000;
            else {
                bool numeric = true;
                for (char c : val)
                    if (!std::isdigit((unsigned char)c)) { numeric = false; break; }
                if (!numeric) return std::string("Unknown value: ") + val;
                t = std::atol(val.c_str());
            }

            long dayCount = level->getTime() / Level::TICKS_PER_DAY;
            long newTime  = dayCount * Level::TICKS_PER_DAY + (t % Level::TICKS_PER_DAY);
            level->setTime(newTime);
            std::ostringstream out;
            out << "Set the time to " << t;
            return out.str();
        }

        if (sub == "query") {
            if (args.size() < 3) return "Usage: /time query <daytime|gametime|day>";
            const std::string& what = args[2];

            long total   = level->getTime();
            long daytime = total % Level::TICKS_PER_DAY;
            long day     = total / Level::TICKS_PER_DAY;

            std::ostringstream out;
            if      (what == "daytime")  { out << "The time of day is " << daytime; }
            else if (what == "gametime") { out << "The game time is "   << total;   }
            else if (what == "day")      { out << "The day is "         << day;     }
            else return std::string("Unknown query: ") + what;
            return out.str();
        }

        return "Unknown sub-command. Usage: /time <add|set|query> ...";
    }

    return std::string("Unknown command: /") + args[0];
}

void ConsoleScreen::render(int xm,int ym,float a)
{
    renderBackground();setupPositions();
    const auto l=buildLayout(width,height,minecraft->useTouchscreen());
    drawCenteredString(font,"Chat",width/2,l.titleY,0xffffffff);
    if(_historyDragging&&ym>=0){_historyOffset=_dragStartOffset;scrollHistory((ym-_dragStartY)/10);}
    const auto lines=historyLines();const int visible=getVisibleHistoryLines();
    if(lines.empty()&&visible>0)drawCenteredString(font,"No messages yet",width/2,(l.historyTop+l.historyBottom)/2,0xffcccccc);
    _historyOffset=std::max(0,std::min(_historyOffset,std::max(0,int(lines.size())-visible)));
    int y=l.historyBottom-10;
    for(int i=_historyOffset;i<int(lines.size())&&i<_historyOffset+visible;++i){font->drawJava(lines[i],float(l.historyLeft+1),float(y+1),0xffffffff,true);font->drawJava(lines[i],float(l.historyLeft),float(y),0xffffffff);y-=l.lineHeight;}
    _ownedButtons[2]->active=_historyOffset<std::max(0,int(lines.size())-visible);
    _ownedButtons[3]->active=_historyOffset>0;
    _ownedButtons[4]->active=!trim(_input).empty();
    if(width>=240&&height>=116)drawCenteredString(font,_historyOffset>0?"Browsing history":"Newest messages",width/2,height-54,0xffcccccc);
    super::render(xm,ym,a);
    std::string displayed=_input;
    if((_cursorBlink/10)%2==0)displayed+='_';
    const int maxWidth=std::max(1,l.inputRight-l.inputLeft-12);
    while(!displayed.empty()&&font->widthJava(displayed)>maxWidth){size_t count=1;while(count<displayed.size()&&(static_cast<unsigned char>(displayed[count])&0xc0)==0x80)++count;displayed.erase(0,count);}
    if(_input.empty())font->drawJava(font->fitJava("Message or /command",maxWidth),float(l.inputLeft+6),float(l.inputTop+8),0xffaaaaaa);
    else {font->drawJava(displayed,float(l.inputLeft+7),float(l.inputTop+9),0xffffffff,true);font->drawJava(displayed,float(l.inputLeft+6),float(l.inputTop+8),0xffffffff);}
}
