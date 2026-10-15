#include "Font.h"

//#include "SharedConstants.h"
#include "../Options.h"
#include "../renderer/Textures.h"
#include "../renderer/Tesselator.h"
#include "../../util/Mth.h"
#include <cstring>
#include "../../network/java/JavaRichText.h"
#include <algorithm>
#include <cmath>
#include <chrono>

Font::Font( Options* options, const std::string& name, Textures* textures )
:	options(options),
	fontTexture(0),
	fontName(name),
	index(0),
	count(0),
	_textures(textures),
	_x(0), _y(0),
	_cols(16), _rows(16),
	_charOffset(0),
	lineHeight(DefaultLineHeight)
{
	init(options);
}


//Font::Font( Options* options, const std::string& name, Textures* textures, int imgW, int imgH, int x, int y, int cols, int rows, unsigned char charOffset )
//:	options(options),
//	fontTexture(0),
//	fontName(name),
//	index(0),
//	count(0),
//	_textures(textures),
//	_x(x), _y(y),
//	_cols(cols), _rows(rows),
//	_charOffset(charOffset)
//{
//	init(options);
//}

void Font::onGraphicsReset()
{
	init(options);
}

void Font::init( Options* options )
{
	TextureId fontTexture = _textures->loadTexture(fontName);
	const TextureData* tex = _textures->getTemporaryTextureData(fontTexture);

	if (!tex)
		return;

	unsigned char* rawPixels = tex->data;

	const int numChars = _rows * _cols;
	for (int i = 0; i < numChars; i++) {
		int xt = i % _cols;
		int yt = i / _cols;

		int x = 7;
		for (; x >= 0; x--) {
			int xPixel = _x + xt * 8 + x;
			bool emptyColumn = true;
			for (int y = 0; y < 8 && emptyColumn; y++) {
				int yPixel = _y + (yt * 8 + y) * tex->w;
				unsigned char pixelalpha = rawPixels[(xPixel + yPixel) << 2];
				if (pixelalpha > 0) emptyColumn = false;
			}
			if (!emptyColumn) {
				break;
			}
		}

		if (i == ' ') x = 4 - 2;
		charWidths[i] = x + 2;
		fcharWidths[i] = (float) charWidths[i];
	}

#ifdef USE_VBO
	return; // this <1
#endif

#ifndef USE_VBO
	listPos = glGenLists(256 + 32);

	Tesselator& t = Tesselator::instance;
	for (int i = 0; i < 256; i++) {
		glNewList(listPos + i, GL_COMPILE);
		// @attn @huge @note: This is some dangerous code right here / Aron, added ^1
		t.begin();
		buildChar(i);
		t.end(false, -1);

		glTranslatef2((GLfloat)charWidths[i], 0.0f, 0.0f);
		glEndList();
	}

	for (int i = 0; i < 32; i++) {
		int br = ((i >> 3) & 1) * 0x55;
		int r = ((i >> 2) & 1) * 0xaa + br;
		int g = ((i >> 1) & 1) * 0xaa + br;
		int b = ((i >> 0) & 1) * 0xaa + br;
		if (i == 6) {
			r += 0x55;
		}
		bool darken = i >= 16;

		if (options->anaglyph3d) {
			int cr = (r * 30 + g * 59 + b * 11) / 100;
			int cg = (r * 30 + g * 70) / (100);
			int cb = (r * 30 + b * 70) / (100);

			r = cr;
			g = cg;
			b = cb;
		}

		// color = r << 16 | g << 8 | b;
		if (darken) {
			r /= 4;
			g /= 4;
			b /= 4;
		}

		glNewList(listPos + 256 + i, GL_COMPILE);
		glColor3f(r / 255.0f, g / 255.0f, b / 255.0f);
		glEndList();
	}
#endif
}

void Font::drawShadow( const std::string& str, float x, float y, int color )
{
	draw(str, x + 1, y + 1, color, true);
	draw(str, x, y, color);
}
void Font::drawShadow( const char* str, float x, float y, int color )
{
	draw(str, x + 1, y + 1, color, true);
	draw(str, x, y, color);
}

void Font::draw( const std::string& str, float x, float y, int color )
{
	draw(str, x, y, color, false);
}

void Font::draw( const char* str, float x, float y, int color )
{
	draw(str, x, y, color, false);
}

void Font::draw( const char* str, float x, float y, int color, bool darken )
{
 if(javaTextEnabled){drawJava(str?str:"",x,y,color,darken);return;}
#ifdef USE_VBO
	drawSlow(str, x, y, color, darken);
#endif
}

void Font::draw( const std::string& str, float x, float y, int color, bool darken )
{
 if(javaTextEnabled){drawJava(str,x,y,color,darken);return;}
#ifdef USE_VBO
	drawSlow(str, x, y, color, darken);
	return;
#endif

	if (str.empty()) return;

	if (darken) {
		int oldAlpha = color & 0xff000000;
		color = (color & 0xfcfcfc) >> 2;
		color += oldAlpha;
	}

	_textures->loadAndBindTexture(fontName);
	float r = ((color >> 16) & 0xff) / 255.0f;
	float g = ((color >> 8) & 0xff) / 255.0f;
	float b = ((color) & 0xff) / 255.0f;
	float a = ((color >> 24) & 0xff) / 255.0f;
	if (a == 0) a = 1;
	glColor4f2(r, g, b, a);

	static const std::string hex("0123456789abcdef");

	index = 0;
	glPushMatrix2();
	glTranslatef2((GLfloat)x, (GLfloat)y, 0.0f);
	for (unsigned int i = 0; i < str.length(); i++) {
		while (str.length() > i + 1 && str[i] == 'ง') {
			int cc = hex.find((char)tolower(str[i + 1]));
			if (cc < 0 || cc > 15) cc = 15;
			lists[index++] = listPos + 256 + cc + (darken ? 16 : 0);

			if (index == 1024) {
				count = index;
				index = 0;
#ifndef USE_VBO
				glCallLists(count, GL_UNSIGNED_INT, lists);
#endif
				count = 1024;
			}

			i += 2;
		}

		if (i < str.length()) {
			//int ch = SharedConstants.acceptableLetters.indexOf(str.charAt(i));
			char ch = str[i];
			if (ch >= 0) {
				//ib.put(listPos + ch + 32);
				lists[index++] = listPos + ch;
			}
		}

		if (index == 1024) {
			count = index;
			index = 0;
#ifndef USE_VBO
			glCallLists(count, GL_UNSIGNED_INT, lists);
#endif
			count = 1024;
		}
	}
	count = index;
	index = 0;
#ifndef USE_VBO
	glCallLists(count, GL_UNSIGNED_INT, lists);
#endif
	glPopMatrix2();
}

int Font::width( const std::string& str )
{
 if(javaTextEnabled)return widthJava(str);
	int maxLen = 0;
	int len = 0;

	for (unsigned int i = 0; i < str.length(); i++) {
		if (str[i] == 'ง') {
			i++;
		} else {
			//int ch = SharedConstants.acceptableLetters.indexOf(str.charAt(i));
			//if (ch >= 0) {
			//    len += charWidths[ch + 32];
			//}
			if (str[i] == '\n') {
				if (len > maxLen) maxLen = len;
				len = 0;
			}
			else {
				int charWidth = charWidths[ (unsigned char) str[i] ];
				len += charWidth;
			}
		}
	}
	return maxLen>len? maxLen : len;
}

int Font::height( const std::string& str ) {
	int h = 0;
	bool hasLine = false;
	for (unsigned int i = 0; i < str.length(); ++i) {
		if (str[i] == '\n') hasLine = true;
		else {
			if (hasLine) h += lineHeight;
			hasLine = false;
		}
	}
	return h;
}

std::string Font::sanitize( const std::string& str )
{
	std::string sanitized(str.length() + 1, 0);
	int j = 0;

	for (unsigned int i = 0; i < str.length(); i++) {
		if (str[i] == 'ง') {
			i++;
			//} else if (SharedConstants.acceptableLetters.indexOf(str.charAt(i)) >= 0) {
		} else {
			sanitized[j++] = str[i];
		}
	}
	return sanitized.erase(j);
}

void Font::drawWordWrap( const std::string& str, float x, float y, float w, int col )
{
	char* cstr = new char[str.length() + 1];
	strncpy(cstr, str.c_str(), str.length());
	cstr[str.length()] = 0;

	const char* lims = " \n\t\r";
	char* ptok = strtok(cstr, lims);

	std::vector<std::string> words;
	while (ptok != NULL) {
		words.push_back( ptok );
		ptok = strtok(NULL, lims);
	}

	delete[] cstr;

	int pos = 0;
	while (pos < (int)words.size()) {
		std::string line = words[pos++] + " ";
		while (pos < (int)words.size() && width(line + words[pos]) < w) {
			line += words[pos++] + " ";
		}
		drawShadow(line, x, y, col);
		y += lineHeight;
	}
}

void Font::drawSlow( const std::string& str, float x, float y, int color, bool darken /*= false*/ ) {
	drawSlow(str.c_str(), x, y, color, darken);
}
void Font::drawSlow( const char* str, float x, float y, int color, bool darken /*= false*/ )
{
	if (!str) return;

	if (darken) {
		int oldAlpha = color & 0xff000000;
		color = (color & 0xfcfcfc) >> 2;
		color += oldAlpha;
	}

	_textures->loadAndBindTexture(fontName);

	Tesselator& t = Tesselator::instance;
	t.begin();
	int alpha = (0xff000000 & color) >> 24;
	if (!alpha) alpha = 0xff;
	t.color((color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff, alpha);
	
	t.addOffset((float)x, (float)y, 0);
	float xOffset = 0;
	float yOffset = 0;

	while (unsigned char ch = *(str++)) {
		if (ch == '\n') {
			xOffset = 0;
			yOffset += lineHeight;
		} else {
			buildChar(ch, xOffset, yOffset);
			xOffset += fcharWidths[ch];
		}
	}
	t.draw();
	t.addOffset(-(float)x, -(float)y, 0);
}

void Font::buildChar( unsigned char i, float x /*= 0*/, float y /*=0*/ )
{
	Tesselator& t = Tesselator::instance;

	//i -= _charOffset;
	//int ix = (i % _cols) * 8 + _x;
	//int iy = (i / _cols) * 8 + _y;
	float ix = (float)((i & 15) * 8);
	float iy = (float)((i >> 4) * 8);
	float s = 7.99f;

	float uo = (0.0f) / 128.0f;
	float vo = (0.0f) / 128.0f;

	t.vertexUV(x, y + s, 0, ix / 128.0f + uo, (iy + s) / 128.0f + vo);
	t.vertexUV(x + s, y + s, 0, (ix + s) / 128.0f + uo, (iy + s) / 128.0f + vo);
	t.vertexUV(x + s, y, 0, (ix + s) / 128.0f + uo, iy / 128.0f + vo);
	t.vertexUV(x, y, 0, ix / 128.0f + uo, iy / 128.0f + vo);
}


namespace {
struct JavaBaseFontGlyph {uint32_t cp;int atlas,x,y,width,height,imageWidth,imageHeight;float scale,ascent,advance;};
const JavaBaseFontGlyph javaBaseFontGlyphs[]={
#include "JavaBaseFontGlyphs.inc"
};
const JavaBaseFontGlyph* baseJavaGlyph(uint32_t cp){const auto* begin=javaBaseFontGlyphs;const auto* end=begin+sizeof(javaBaseFontGlyphs)/sizeof(*begin);const auto* result=std::lower_bound(begin,end,cp,[](const JavaBaseFontGlyph& glyph,uint32_t code){return glyph.cp<code;});return result!=end&&result->cp==cp?result:NULL;}
struct JavaUnicodeGlyph {uint32_t cp,index;uint8_t width,advance;};
const JavaUnicodeGlyph javaUnicodeGlyphs[]={
#include "JavaUnicodeGlyphs.inc"
};
const JavaUnicodeGlyph* unicodeGlyph(uint32_t cp){const auto* begin=javaUnicodeGlyphs;const auto* end=begin+sizeof(javaUnicodeGlyphs)/sizeof(*begin);const auto* result=std::lower_bound(begin,end,cp,[](const JavaUnicodeGlyph& glyph,uint32_t code){return glyph.cp<code;});if(result!=end&&result->cp==cp)return result;if(cp!=0xfffd)return unicodeGlyph(0xfffd);return NULL;}
}
const Font::PackGlyph* Font::packGlyph(uint32_t cp,const std::string& font)const{auto table=packGlyphs.find(font.empty()?"minecraft:default":font);if(table==packGlyphs.end())return NULL;auto glyph=table->second.find(cp);return glyph==table->second.end()?NULL:&glyph->second;}
float Font::javaGlyphAdvance(uint32_t cp,bool bold,const std::string& font){auto packed=packGlyph(cp,font);float advance=0;if(packed)advance=packed->advance;else if(const auto* base=baseJavaGlyph(cp))advance=base->advance;else if(cp<128)advance=fcharWidths[cp];else{const auto* entry=unicodeGlyph(cp);advance=entry?entry->advance*0.5f+1:8;}return advance+(bold?1.f:0.f);}
int Font::widthJava(const std::string& text){float width=0,maxWidth=0;for(const auto& glyph:JavaRichText::parse(text)){if(glyph.codepoint=='\n'){maxWidth=std::max(maxWidth,width);width=0;continue;}width+=javaGlyphAdvance(glyph.codepoint,glyph.bold,glyph.font);}return static_cast<int>(std::ceil(std::max(width,maxWidth)));}
void Font::drawJava(const std::string& text,float x,float y,int baseColor,bool darken){
 Tesselator& t=Tesselator::instance;float ox=x,oy=y;bool drawing=false;TextureId activeTexture=Textures::InvalidId;
 std::map<int,TextureId> pageTextures,baseTextures;
 auto flush=[&](){if(drawing){t.draw();drawing=false;}};
 auto textureFor=[&](const std::string& resource){
  // Loading a missing texture binds it. Flush before that side effect, so the
  // queued glyphs keep their original atlas even on the first frame.
  if(!_textures->hasTexture(resource)){flush();activeTexture=Textures::InvalidId;}
  return _textures->loadTexture(resource);
 };
 auto bindTexture=[&](TextureId next){if(drawing&&next==activeTexture)return;flush();_textures->bind(next);activeTexture=next;t.begin();drawing=true;};
 auto bindPage=[&](int next){auto found=pageTextures.find(next);TextureId id;if(found==pageTextures.end()){id=textureFor(next<0?fontName:"java-font/page-"+std::to_string(next)+".png");pageTextures[next]=id;}else id=found->second;bindTexture(id);};
 auto quad=[&](float px,float py,float w,float h,float u,float v,float uw,float vh,bool italic){const float skew=italic?1.0f:0.0f;t.vertexUV(px-skew,py+h,0,u,v+vh);t.vertexUV(px+w-skew,py+h,0,u+uw,v+vh);t.vertexUV(px+w+skew,py,0,u+uw,v);t.vertexUV(px+skew,py,0,u,v);};
 for(auto glyph:JavaRichText::parse(text,static_cast<uint32_t>(baseColor))){
  if(glyph.codepoint=='\n'){ox=x;oy+=lineHeight;continue;}
  if(glyph.obfuscated&&glyph.codepoint!=' '){const uint64_t frame=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()/50;uint32_t seed=static_cast<uint32_t>(frame*1664525u+glyph.codepoint*1013904223u+static_cast<unsigned>(ox*2));
   if(glyph.codepoint<128){const float target=fcharWidths[glyph.codepoint];for(unsigned k=0;k<95;k++){const uint32_t candidate=33+(seed+k)%94;if(fcharWidths[candidate]==target){glyph.codepoint=candidate;break;}}}
   else {const JavaUnicodeGlyph* original=unicodeGlyph(glyph.codepoint);const size_t count=sizeof(javaUnicodeGlyphs)/sizeof(*javaUnicodeGlyphs);if(original)for(unsigned k=0;k<128;k++){const auto& candidate=javaUnicodeGlyphs[(seed+k*37u)%count];if(candidate.advance==original->advance&&candidate.width==original->width){glyph.codepoint=candidate.cp;break;}}}
  }
  uint32_t color=glyph.color;if(darken)color=(color&0xff000000)|((color&0xfcfcfc)>>2);
  auto colorize=[&](){t.color(int((color>>16)&255),int((color>>8)&255),int(color&255),int((color>>24)&255));};
  auto packed=packGlyph(glyph.codepoint,glyph.font);
  Font::PackGlyph baseGlyph;
  if(!packed){if(const auto* base=baseJavaGlyph(glyph.codepoint)){
   baseGlyph.advance=base->advance;baseGlyph.ascent=base->ascent;
   if(base->atlas>=0){auto found=baseTextures.find(base->atlas);TextureId id;if(found==baseTextures.end()){id=textureFor("java-font/base-"+std::to_string(base->atlas)+".png");baseTextures[base->atlas]=id;}else id=found->second;baseGlyph.texture=id;baseGlyph.u=float(base->x)/base->imageWidth;baseGlyph.v=float(base->y)/base->imageHeight;baseGlyph.uw=float(base->width)/base->imageWidth;baseGlyph.vh=float(base->height)/base->imageHeight;baseGlyph.width=base->width*base->scale;baseGlyph.height=base->height*base->scale;}
   packed=&baseGlyph;
  }}
  if(packed){
   const auto& g=*packed;
   if(g.texture&&g.width>0&&g.height>0){bindTexture(g.texture);colorize();quad(ox,oy+8-g.ascent,g.width,g.height,g.u,g.v,g.uw,g.vh,glyph.italic);if(glyph.bold)quad(ox+1.f,oy+8-g.ascent,g.width,g.height,g.u,g.v,g.uw,g.vh,glyph.italic);}
   const float advance=javaGlyphAdvance(glyph.codepoint,glyph.bold,glyph.font);
   // Bitmap and spacing providers retain the same text decorations as base glyphs.
   // Rebind through Textures so the following glyph cannot reuse a stale atlas.
   if(glyph.underlined||glyph.struck){bindPage(0);colorize();if(glyph.underlined)quad(ox,oy+8,advance,0.5f,511.5f/512,511.5f/512,0,0,false);if(glyph.struck)quad(ox,oy+4,advance,0.5f,511.5f/512,511.5f/512,0,0,false);}
   ox+=advance;continue;
  }
  float advance=0,w=8,u=0,v=0,uw=0,vh=0;int glyphPage=-1;
  if(glyph.codepoint<128){advance=fcharWidths[glyph.codepoint];u=(glyph.codepoint%16)*8.0f/128;v=(glyph.codepoint/16)*8.0f/128;uw=7.99f/128;vh=7.99f/128;}
  else {const JavaUnicodeGlyph* entry=unicodeGlyph(glyph.codepoint);if(!entry)continue;glyphPage=entry->index/961;const unsigned cell=entry->index%961;u=(cell%31)*16.0f/512;v=(cell/31)*16.0f/512;w=entry->width*0.5f;uw=(entry->width-0.01f)/512;vh=15.99f/512;advance=entry->advance*0.5f+1;}
  bindPage(glyphPage);colorize();quad(ox,oy,w,7.99f,u,v,uw,vh,glyph.italic);if(glyph.bold){quad(ox+1.f,oy,w,7.99f,u,v,uw,vh,glyph.italic);advance+=1.f;}
  if(glyph.underlined||glyph.struck){bindPage(0);colorize();if(glyph.underlined)quad(ox,oy+8,advance,0.5f,511.5f/512,511.5f/512,0,0,false);if(glyph.struck)quad(ox,oy+4,advance,0.5f,511.5f/512,511.5f/512,0,0,false);}
  ox+=advance;
 }
 if(drawing)t.draw();
}

std::vector<std::string> Font::wrapJava(const std::string& text,int maxWidth){std::vector<std::string> lines;std::string line;int width=0;for(const auto& glyph:JavaRichText::parse(text)){if(glyph.codepoint=='\n'){lines.push_back(line);line.clear();width=0;continue;}const std::string encoded=JavaRichText::encode(glyph);const int advance=static_cast<int>(std::ceil(javaGlyphAdvance(glyph.codepoint,glyph.bold,glyph.font)));if(!line.empty()&&width+advance>std::max(1,maxWidth)){lines.push_back(line);line.clear();width=0;}line+=encoded;width+=advance;}if(!line.empty()||lines.empty())lines.push_back(line);return lines;}

std::string Font::fitJava(const std::string& text,int maxWidth){
 if(maxWidth<=0)return "";if(widthJava(text)<=maxWidth)return text;
 const std::string ellipsis="ยงr...";const int reserve=widthJava(ellipsis);if(reserve>maxWidth)return "";
 std::string result;int used=0;for(const auto& glyph:JavaRichText::parse(text)){if(glyph.codepoint=='\n')break;const auto encoded=JavaRichText::encode(glyph);const int advance=static_cast<int>(std::ceil(javaGlyphAdvance(glyph.codepoint,glyph.bold,glyph.font)));if(used+advance+reserve>maxWidth)break;result+=encoded;used+=advance;}return result+ellipsis;
}
