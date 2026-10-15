#include "../original-tcpe/src/network/java/JavaRichText.h"
#include <cassert>
#include <iostream>
int main(){using JavaRichText::parse;
 auto basic=parse(u8"§r§a[§lYes§r]");assert(basic.size()==5);assert(basic[0].codepoint=='['&&basic[0].color==0xff55ff55);assert(basic[1].bold);assert(!basic[4].bold&&basic[4].color==0xffffffff);
 auto hex=parse(u8"§x§1§2§A§b§0§FRGB");assert(hex.size()==3);for(const auto& glyph:hex)assert(glyph.color==0xff12ab0f);
 auto gradient=parse(u8"§x§f§f§0§0§0§0A§x§0§0§f§f§0§0B§x§0§0§0§0§f§fC");assert(gradient.size()==3);assert(gradient[0].color==0xffff0000);assert(gradient[1].color==0xff00ff00);assert(gradient[2].color==0xff0000ff);
 auto unicode=parse(u8"ᴘʀᴀᴄᴛɪᴄᴇ 中文 𝔸Ａ\n★");assert(unicode.size()==16);assert(unicode[0].codepoint==0x1d18);assert(unicode[9].codepoint==0x4e2d);assert(unicode[12].codepoint==0x1d538);assert(unicode[13].codepoint==0xff21);assert(unicode[14].codepoint=='\n');
 auto styles=parse(u8"§l§o§n§m§kA§rB",0x80334455);assert(styles.size()==2);assert(styles[0].bold&&styles[0].italic&&styles[0].underlined&&styles[0].struck&&styles[0].obfuscated);assert(styles[1].color==0x80334455&&!styles[1].bold);
 for(const auto& glyph:gradient){auto again=parse(JavaRichText::encode(glyph));assert(again.size()==1&&again[0].codepoint==glyph.codepoint&&again[0].color==glyph.color);}
 auto malformed=parse(std::string("\xf0\x28\x8c\x28",4));assert(!malformed.empty()&&malformed[0].codepoint==0xfffd);
 auto overlong=parse(std::string("\xc0\xaf",2));assert(overlong[0].codepoint==0xfffd);
 auto named=parse(u8"§zoraxen:shift;§aA§rB§zminecraft:default;C");assert(named.size()==3&&named[0].font=="oraxen:shift"&&named[1].font=="oraxen:shift"&&named[2].font=="minecraft:default");auto round=parse(JavaRichText::encode(named[0]));assert(round.size()==1&&round[0].font=="oraxen:shift"&&round[0].color==named[0].color);
 std::cout<<"Rich UTF8, native formatting, exact RGB gradients, styles and invalid UTF8 PASS\n";
}
