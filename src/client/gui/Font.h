#ifndef NET_MINECRAFT_CLIENT_GUI__Font_H__
#define NET_MINECRAFT_CLIENT_GUI__Font_H__

//package net.minecraft.client.gui;

#include <string>
#include <cctype>
#include <vector>
#include <map>

#include "../renderer/gles.h"

class Textures;
class Options;

class Font
{
public:
 struct PackGlyph {unsigned texture=0;float u=0,v=0,uw=0,vh=0,width=0,height=0,ascent=0,advance=0;};
 void setPackGlyph(uint32_t codepoint,const PackGlyph& glyph,const std::string& font="minecraft:default"){packGlyphs[font][codepoint]=glyph;}
 void clearPackGlyphs(){packGlyphs.clear();}
 float javaGlyphAdvance(uint32_t codepoint,bool bold=false,const std::string& font="");
 void setJavaTextEnabled(bool enabled){javaTextEnabled=enabled;}
 void drawJava(const std::string& text,float x,float y,int color,bool darken=false);
 int widthJava(const std::string& text);
 std::string fitJava(const std::string& text,int maxWidth);
 std::vector<std::string> wrapJava(const std::string& text,int maxWidth);
    Font(Options* options, const std::string& name, Textures* textures);
	//Font(Options* options, const std::string& name, Textures* textures, int imgW, int imgH, int x, int y, int cols, int rows, unsigned char charOffset);
	
	void init(Options* options);
	void onGraphicsReset();

	void draw(const char* str, float x, float y, int color);
    void draw(const std::string& str, float x, float y, int color);
	void draw(const char* str, float x, float y, int color, bool darken);
    void draw(const std::string& str, float x, float y, int color, bool darken);
	void drawShadow(const std::string& str, float x, float y, int color);
	void drawShadow(const char* str, float x, float y, int color);
	void drawWordWrap(const std::string& str, float x, float y, float w, int col);

	int width(const std::string& str);
	int height(const std::string& str);

	static std::string sanitize(const std::string& str);
private:
 std::map<std::string,std::map<uint32_t,PackGlyph>> packGlyphs;
 const PackGlyph* packGlyph(uint32_t codepoint,const std::string& font)const;
	void buildChar(unsigned char i, float x = 0, float y = 0);
	void drawSlow(const std::string& str, float x, float y, int color, bool darken = false);
	void drawSlow(const char* str, float x, float y, int color, bool darken = false);
public:
	int fontTexture;
	int lineHeight;
	static const int DefaultLineHeight = 10;
private:
	int charWidths[256];
	float fcharWidths[256];
	int listPos;

	int index;
	int count;
	GLuint lists[1024];

	bool javaTextEnabled=false;
	std::string fontName;
	Textures* _textures;

	Options* options;

	int _x, _y;
	int _cols;
	int _rows;
	unsigned char _charOffset;
};

#endif /*NET_MINECRAFT_CLIENT_GUI__Font_H__*/
