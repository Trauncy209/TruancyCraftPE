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
 auto half=render(u8"§ztest:font;§c\ue000",int(0x80ffffff));assert(red(half,42,40)>=127&&red(half,42,40)<=129);
 auto shadow=render(u8"§ztest:font;§c\ue000",int(0xffffffff),true);assert(red(shadow,42,40)==63);
 auto full=render(u8"§ztest:font;§c\ue000",int(0xffffffff));assert(red(full,42,40)==255);
 auto rgb=render(u8"§ztest:font;§x§1§2§3§4§5§6\ue000",int(0xffffffff));assert(red(rgb,42,40)==18);
 auto rgbAlpha=render(u8"§ztest:font;§x§1§2§3§4§5§6\ue000",int(0x80ffffff));assert(red(rgbAlpha,42,40)>=8&&red(rgbAlpha,42,40)<=10);
 auto ordinary=render(u8"§cA",int(0xffffffff));auto ordinaryHalf=render(u8"§cA",int(0x80ffffff));int lit=0;for(int y=40;y<56;++y)for(int x=40;x<56;++x)if(red(ordinary,x,y)>0){++lit;assert(std::abs(int(red(ordinaryHalf,x,y))-int(red(ordinary,x,y)*128/255))<=1);}assert(lit>10);
 std::cout<<"Native font opacity and RGB inheritance: packed/base glyphs preserve alpha; shadow quarter-RGB PASS"<<std::endl;std::_Exit(0);
}
