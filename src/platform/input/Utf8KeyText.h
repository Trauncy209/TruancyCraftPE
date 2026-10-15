#ifndef TCPE_UTF8_KEY_TEXT_H
#define TCPE_UTF8_KEY_TEXT_H
#include <cstdint>
#include <cstddef>
#include <string>
namespace Utf8KeyText {
inline unsigned encode(uint32_t c,char out[4]) {
 if(c>0x10ffff || (c>=0xd800&&c<=0xdfff))return 0;
 if(c<=0x7f){out[0]=(char)c;return 1;}
 if(c<=0x7ff){out[0]=(char)(0xc0|(c>>6));out[1]=(char)(0x80|(c&63));return 2;}
 if(c<=0xffff){out[0]=(char)(0xe0|(c>>12));out[1]=(char)(0x80|((c>>6)&63));out[2]=(char)(0x80|(c&63));return 3;}
 out[0]=(char)(0xf0|(c>>18));out[1]=(char)(0x80|((c>>12)&63));out[2]=(char)(0x80|((c>>6)&63));out[3]=(char)(0x80|(c&63));return 4;
}
inline void eraseLast(std::string& text){if(text.empty())return;size_t p=text.size()-1;while(p>0&&(static_cast<unsigned char>(text[p])&0xc0)==0x80)--p;text.erase(p);}
inline bool appendByte(std::string& text,char c,size_t limit) {
 unsigned byte=static_cast<unsigned char>(c);size_t start=text.size();unsigned lead=0,expected=0,pending=0;
 if(!text.empty()){
  start=text.size()-1;while(start>0&&(static_cast<unsigned char>(text[start])&0xc0)==0x80)--start;
  lead=static_cast<unsigned char>(text[start]);expected=lead>=0xc2&&lead<=0xdf?2:lead>=0xe0&&lead<=0xef?3:lead>=0xf0&&lead<=0xf4?4:0;
  if(expected&&text.size()-start<expected)pending=(unsigned)(text.size()-start);
 }
 if(pending){
  bool continuation=(byte&0xc0)==0x80;
  bool scalar=!(pending==1&&((lead==0xe0&&byte<0xa0)||(lead==0xed&&byte>=0xa0)||(lead==0xf0&&byte<0x90)||(lead==0xf4&&byte>=0x90)));
  if(continuation&&scalar&&text.size()<limit){text+=c;return true;}
  text.erase(start);if(continuation)return false;
 }
 unsigned length=byte>=32&&byte<127?1:byte>=0xc2&&byte<=0xdf?2:byte>=0xe0&&byte<=0xef?3:byte>=0xf0&&byte<=0xf4?4:0;
 if(!length||text.size()+length>limit)return false;text+=c;return true;
}
}
#endif
