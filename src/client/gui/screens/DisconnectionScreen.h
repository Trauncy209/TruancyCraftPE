#ifndef NET_MINECRAFT_CLIENT_GUI_SCREENS__DisconnectionScreen_H__
#define NET_MINECRAFT_CLIENT_GUI_SCREENS__DisconnectionScreen_H__

#include "../Screen.h"
#include "../Font.h"
#include "../components/Button.h"
#include "../../Minecraft.h"
#include <string>
#include <algorithm>
#include "../../../platform/input/Keyboard.h"

class DisconnectionScreen: public Screen
{
	typedef Screen super;
public:
	DisconnectionScreen(const std::string& msg)
	:	_msg(msg),
		_back(NULL),_previous(NULL),_next(NULL),_page(0),_layoutWidth(-1),_layoutHeight(-1)
	{}

	~DisconnectionScreen() {
		delete _back;
        delete _previous;delete _next;
	}

	void init() {
		if (minecraft->useTouchscreen())
			_back = new Touch::TButton(1, "Ok");
		else
			_back = new Button(1, "Ok");

		buttons.push_back(_back);
		tabButtons.push_back(_back);

		_back->width = 128;
		_back->x = (width - _back->width) / 2;
        _previous=minecraft->useTouchscreen()?static_cast<Button*>(new Touch::TButton(2,"Previous")):new Button(2,"Previous");
        _next=minecraft->useTouchscreen()?static_cast<Button*>(new Touch::TButton(3,"Next")):new Button(3,"Next");
        buttons.push_back(_previous);buttons.push_back(_next);
        tabButtons.push_back(_previous);tabButtons.push_back(_next);
        _previous->width=_next->width=96;
        updateLayout();
	}

	void render( int xm, int ym, float a ) {
        updateLayout();
        renderBackground();
        super::render(xm,ym,a);
        const size_t begin=_page*_pageLines,end=std::min(_lines.size(),begin+_pageLines);
        const int blockHeight=static_cast<int>(end-begin)*12;
        int y=std::max(12,(_back->y-12-blockHeight)/2);
        for(size_t i=begin;i<end;++i){
            if(minecraft->javaSession){const int w=minecraft->font->widthJava(_lines[i]);minecraft->font->drawJava(_lines[i],(width-w)/2+1,y+1,0xffffffff,true);minecraft->font->drawJava(_lines[i],(width-w)/2,y,0xffffffff);}
            else minecraft->font->drawShadow(_lines[i],(width-minecraft->font->width(_lines[i]))/2,y,0xffffffff);
            y+=12;
        }
    }

	void keyPressed(int key) override {
        if (minecraft->javaSession && key == Keyboard::KEY_ESCAPE) { minecraft->leaveGame(); return; }
        if(key==33||key==38){changePage(-1);return;}
        if(key==34||key==40){changePage(1);return;}
        super::keyPressed(key);
    }
	void buttonClicked(Button* button) {
		if(button==_previous){changePage(-1);return;}
        if(button==_next){changePage(1);return;}
        if (button->id == _back->id) {
			minecraft->leaveGame();
		}
	}
	bool isInGameScreen() { return false; }

private:
	std::string _msg;
    Button* _back;Button* _previous;Button* _next;
    size_t _page,_pageLines=1;int _layoutWidth,_layoutHeight;
    std::vector<std::string> _lines;
    void changePage(int delta){if(delta<0&&_page>0)--_page;else if(delta>0&&(_page+1)*_pageLines<_lines.size())++_page;updatePageButtons();}
    void updatePageButtons(){const bool pages=_lines.size()>_pageLines;_previous->visible=_next->visible=pages;_previous->active=_page>0;_next->active=(_page+1)*_pageLines<_lines.size();}
    void updateLayout(){
        if(_layoutWidth==width&&_layoutHeight==height)return;
        _layoutWidth=width;_layoutHeight=height;_page=0;_lines.clear();
        const int available=std::max(1,width-24);
        if(minecraft->javaSession)_lines=minecraft->font->wrapJava(_msg,available);
        else {
            std::string line;
            for(char c:_msg){if(c=='\n'){_lines.push_back(line);line.clear();continue;}const std::string candidate=line+c;if(!line.empty()&&minecraft->font->width(candidate)>available){_lines.push_back(line);line.clear();}line+=c;}
            if(!line.empty()||_lines.empty())_lines.push_back(line);
        }
        _pageLines=std::max(1,(height-100)/12);
        const bool pages=_lines.size()>_pageLines;
        _back->x=(width-_back->width)/2;_back->y=pages?height-44:std::max(height/2,std::min(height-44,height/2+static_cast<int>(_lines.size())*6));
        if(pages){const int column=std::max(1,(width-32)/3);_previous->width=_back->width=_next->width=column;_previous->x=12;_back->x=16+column;_next->x=20+2*column;_previous->msg=width<340?"Prev":"Previous";}else {_back->width=std::min(128,std::max(1,width-24));_back->x=(width-_back->width)/2;}
        _previous->y=_next->y=_back->y;
        updatePageButtons();
    }
};

#endif /*NET_MINECRAFT_CLIENT_GUI_SCREENS__DisconnectionScreen_H__*/
