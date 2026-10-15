#include "platform/input/Keyboard.h"
#include <cassert>
#include <iostream>
static std::string queued(){std::string s;while(Keyboard::nextTextChar())s+=Keyboard::getChar();return s;}
int main(){
 Keyboard::reset();Keyboard::feedText('A');assert(Keyboard::feedCodepoint(0xe9));assert(Keyboard::feedCodepoint(0x4e2d));assert(Keyboard::feedCodepoint(0x1f600));
 assert(queued()==std::string("A\xc3\xa9\xe4\xb8\xad\xf0\x9f\x98\x80"));
 for(uint32_t bad:{0xd800u,0xdfffu,0x110000u,0xffffffffu}){Keyboard::reset();assert(!Keyboard::feedCodepoint(bad));assert(queued().empty());}
 for(auto pair:{std::pair<uint32_t,unsigned>{0,1},{0x7f,1},{0x80,2},{0x7ff,2},{0x800,3},{0xffff,3},{0x10000,4},{0x10ffff,4}}){char b[4];assert(Utf8KeyText::encode(pair.first,b)==pair.second);}
 std::string field;for(char c:std::string("A\xc3\xa9\xe4\xb8\xad\xf0\x9f\x98\x80"))assert(Utf8KeyText::appendByte(field,c,256));Utf8KeyText::eraseLast(field);assert(field==std::string("A\xc3\xa9\xe4\xb8\xad"));Utf8KeyText::eraseLast(field);assert(field==std::string("A\xc3\xa9"));Utf8KeyText::eraseLast(field);assert(field=="A");
 field="1234";for(char c:std::string("\xe4\xb8\xad"))Utf8KeyText::appendByte(field,c,6);assert(field=="1234");assert(Utf8KeyText::appendByte(field,'A',6));assert(Utf8KeyText::appendByte(field,'B',6));assert(!Utf8KeyText::appendByte(field,'C',6));assert(field=="1234AB");
 field.clear();assert(Utf8KeyText::appendByte(field,(char)0xed,256));assert(!Utf8KeyText::appendByte(field,(char)0xa0,256));assert(field.empty());assert(!Utf8KeyText::appendByte(field,(char)0x80,256));assert(!Utf8KeyText::appendByte(field,'\n',256));
 std::cout<<"Unicode keyboard PASS: actual Keyboard text queue keeps ASCII/accent/CJK/emoji UTF-8; invalid scalars rejected; native field byte limits and whole-scalar backspace verified."<<std::endl;
}
