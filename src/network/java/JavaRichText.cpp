#include "JavaRichText.h"
namespace JavaRichText {
namespace {
int hex(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
bool marker(const std::string& s,size_t i,size_t& len){if(i+1<s.size()&&static_cast<unsigned char>(s[i])==0xc2&&static_cast<unsigned char>(s[i+1])==0xa7){len=2;return true;}return false;}
uint32_t decode(const std::string& s,size_t& i){const unsigned char c=s[i++];if(c<128)return c;unsigned n=c>=0xc2&&c<=0xdf?1:c>=0xe0&&c<=0xef?2:c>=0xf0&&c<=0xf4?3:0;if(!n)return 0xfffd;uint32_t cp=c&((1u<<(6-n))-1);if(i+n>s.size())return 0xfffd;for(unsigned k=0;k<n;k++){const unsigned char ch=s[i+k];if((ch&0xc0)!=0x80)return 0xfffd;cp=(cp<<6)|(ch&63);}i+=n;if(cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff)||(n==1&&cp<128)||(n==2&&cp<2048)||(n==3&&cp<65536))return 0xfffd;return cp;}
}
std::string encode(const Glyph& glyph){std::string text="\xc2\xa7x";const char* hex="0123456789abcdef";for(int shift=20;shift>=0;shift-=4){text+="\xc2\xa7";text+=hex[(glyph.color>>shift)&15];}text+="\xc2\xa7z"+(glyph.font.empty()?std::string("minecraft:default"):glyph.font)+";";if(glyph.bold)text+="\xc2\xa7l";if(glyph.italic)text+="\xc2\xa7o";if(glyph.underlined)text+="\xc2\xa7n";if(glyph.struck)text+="\xc2\xa7m";if(glyph.obfuscated)text+="\xc2\xa7k";const uint32_t c=glyph.codepoint;if(c<128)text+=char(c);else if(c<2048){text+=char(0xc0|(c>>6));text+=char(0x80|(c&63));}else if(c<65536){text+=char(0xe0|(c>>12));text+=char(0x80|((c>>6)&63));text+=char(0x80|(c&63));}else{text+=char(0xf0|(c>>18));text+=char(0x80|((c>>12)&63));text+=char(0x80|((c>>6)&63));text+=char(0x80|(c&63));}return text;}
std::vector<Glyph> parse(const std::string& text,uint32_t baseColor){
 std::vector<Glyph> glyphs;std::string currentFont;Glyph style{};style.color=baseColor;if(!(style.color>>24))style.color|=0xff000000;
 static const uint32_t colors[]={0x000000,0x0000aa,0x00aa00,0x00aaaa,0xaa0000,0xaa00aa,0xffaa00,0xaaaaaa,0x555555,0x5555ff,0x55ff55,0x55ffff,0xff5555,0xff55ff,0xffff55,0xffffff};
 for(size_t i=0;i<text.size();){size_t m=0;if(marker(text,i,m)&&i+m<text.size()){
  char code=text[i+m];if(code>='A'&&code<='Z')code=code-'A'+'a';uint32_t rgb=0;bool colored=false;size_t end=i+m+1;
  if(code=='z'){const size_t finish=text.find(';',end);if(finish!=std::string::npos&&finish-end<=256){const auto id=text.substr(end,finish-end);bool valid=!id.empty()&&id.find(':')!=std::string::npos;for(unsigned char c:id)if(!(c>='a'&&c<='z')&&!(c>='0'&&c<='9')&&std::string(":/._-").find(char(c))==std::string::npos)valid=false;if(valid){currentFont=id;i=finish+1;continue;}}}
  if(code=='x'){size_t p=end;colored=true;for(int k=0;k<6;k++){size_t len=0;if(!marker(text,p,len)||p+len>=text.size()||hex(text[p+len])<0){colored=false;break;}rgb=(rgb<<4)|hex(text[p+len]);p+=len+1;}if(colored)end=p;}
  else if(hex(code)>=0){rgb=colors[hex(code)];colored=true;}
  if(colored){style=Glyph{};style.color=(baseColor&0xff000000)|rgb;if(!(style.color>>24))style.color|=0xff000000;i=end;continue;}
  bool valid=true;switch(code){case 'r':style=Glyph{};style.color=baseColor;if(!(style.color>>24))style.color|=0xff000000;break;case 'l':style.bold=true;break;case 'o':style.italic=true;break;case 'n':style.underlined=true;break;case 'm':style.struck=true;break;case 'k':style.obfuscated=true;break;default:valid=false;break;}if(valid){i=end;continue;}
 }
 Glyph next=style;next.font=currentFont;next.codepoint=decode(text,i);glyphs.push_back(next);
 }
 return glyphs;
}
}
