#ifndef TCPE_JAVA_RICH_TEXT_H
#define TCPE_JAVA_RICH_TEXT_H
#include <cstdint>
#include <string>
#include <vector>
namespace JavaRichText {
struct Glyph {uint32_t codepoint,color;std::string font;bool bold=false,italic=false,underlined=false,struck=false,obfuscated=false;};
std::string encode(const Glyph& glyph);
std::vector<Glyph> parse(const std::string& text,uint32_t baseColor=0xffffffff);
}
#endif
