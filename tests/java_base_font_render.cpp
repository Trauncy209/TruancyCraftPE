#include "network/java/JavaPackFontRuntime.h"
#include "client/renderer/Tesselator.h"
#include "AppPlatform_glfw.h"
#include <cassert>
#include <iostream>
extern "C" TcpeJavaSession* tcpe_java_connect(const char*,size_t,const char*,size_t,bool){return nullptr;}
extern "C" void tcpe_java_close(TcpeJavaSession*){}
extern "C" bool tcpe_java_command(TcpeJavaSession*,const char*,size_t){return true;}
int main(){
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
 auto* window=glfwCreateWindow(160,96,"Packed glyph decorations",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();Tesselator::instance.init();
 AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);Font font(&options,"font/default8.png",&textures);font.setJavaTextEnabled(true);
 TextureData image;image.w=image.h=1;image.numBytes=4;image.data=new unsigned char[4]{255,255,255,255};const auto texture=textures.assignTexture("test/packed-style",image);
 Font::PackGlyph bitmap;bitmap.texture=texture;bitmap.uw=bitmap.vh=1;bitmap.width=8;bitmap.height=2;bitmap.ascent=8;bitmap.advance=8;font.setPackGlyph(0xe000,bitmap,"test:font");Font::PackGlyph space;space.advance=8;font.setPackGlyph(0xe001,space,"test:font");
 auto render=[&](const std::string& value,int color=0xffffffff,bool darken=false){glViewport(0,0,160,96);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,80,48,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable2(GL_DEPTH_TEST);glEnable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);font.drawJava(value,20,20,color,darken);glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> pixels(160*96*4);glReadPixels(0,0,160,96,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());return pixels;};
 auto red=[](const std::vector<unsigned char>& p,int x,int y){return p[((95-y)*160+x)*4];};
 assert(font.javaGlyphAdvance(0x1d00)==6&&font.javaGlyphAdvance(0x1d07)==6&&font.javaGlyphAdvance(0x1d1b)==6);assert(font.javaGlyphAdvance(0x1d00,true)==7);assert(font.widthJava(u8"ᴀᴇᴛ")==18);assert(font.widthJava(u8"§lᴀᴇᴛ")==21);assert(font.javaGlyphAdvance(' ')==4&&font.javaGlyphAdvance(0x200c)==0);
 const auto id=textures.loadTexture("java-font/base-0.png");const auto* data=textures.getTemporaryTextureData(id);assert(data&&data->w==128&&data->h==536);
 auto smallcap=render(u8"ᴀ");int opaque=0;for(int y=0;y<8;++y)for(int x=0;x<8;++x){const auto* source=data->data+((296+y)*128+56+x)*4;for(int c=0;c<3;++c){int expected=source[c]*source[3]/255;int pixel=smallcap[((95-(42+y*2))*160+40+x*2)*4+c];assert(std::abs(pixel-expected)<=1);}if(source[3])++opaque;}assert(opaque>10);
 font.setJavaTextEnabled(false);auto nativeFrame=[&](){glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);font.draw("Native TCPE",20,20,0xffffffff);glFinish();std::vector<unsigned char> p(160*96*4);glReadPixels(0,0,160,96,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;};auto original=nativeFrame();font.drawJava(u8"ᴀᴇᴛ",20,20,0xffffffff);auto retained=nativeFrame();assert(original==retained);
 std::cout<<"Java bitmap smallcaps provider pixel equivalence, exact advances/bold1/space4 and original TCPE font preservation PASS"<<std::endl;std::_Exit(0);
}
