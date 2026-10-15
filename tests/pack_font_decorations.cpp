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
 auto render=[&](const std::string& value){glViewport(0,0,160,96);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,80,48,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable2(GL_DEPTH_TEST);glEnable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);font.drawJava(value,20,20,0xffffffff);glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> pixels(160*96*4);glReadPixels(0,0,160,96,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());return pixels;};
 auto red=[](const std::vector<unsigned char>& p,int x,int y){return p[((95-y)*160+x)*4];};
 auto plain=render(u8"§ztest:font;§c\ue000");assert(red(plain,42,48)==0&&red(plain,42,56)==0);
 for(const char* cp:{u8"\ue000",u8"\ue001"}){auto underline=render(std::string(u8"§ztest:font;§c§n")+cp);assert(red(underline,42,56)==255);assert(red(underline,42,48)==0);auto strike=render(std::string(u8"§ztest:font;§c§m")+cp);assert(red(strike,42,48)==255);assert(red(strike,42,56)==0);}
 auto mixed=render(u8"§ztest:font;§c§n\ue000§r§c§zminecraft:default;A");auto baseline=render(u8"§cA");for(int y=0;y<16;++y)for(int x=0;x<16;++x)for(int c=0;c<3;++c)assert(mixed[((95-(40+y))*160+56+x)*4+c]==baseline[((95-(40+y))*160+40+x)*4+c]);
 std::cout<<"Packed bitmap and space glyph underline/strike pixels and following native atlas restoration PASS"<<std::endl;std::_Exit(0);
}
