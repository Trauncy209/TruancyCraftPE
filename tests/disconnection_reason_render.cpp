#include "client/Minecraft.h"
#include "client/gui/Screen.h"
#include "client/gui/Font.h"
#include "client/gui/components/Button.h"
#include "network/java/JavaGameSession.h"
#include "network/java/JavaRichText.h"
#define private public
#include "client/gui/screens/DisconnectionScreen.h"
#undef private
#include "client/renderer/Tesselator.h"
#include "client/renderer/Textures.h"
#include "AppPlatform_glfw.h"
#include <cassert>
#include <png.h>
#include <iostream>
class GameProbe:public Minecraft{public:GameProbe():Minecraft(){}};
class DisconnectProbe:public DisconnectionScreen{public:explicit DisconnectProbe(const std::string& reason):DisconnectionScreen(reason){}using Screen::buttons;};
static void snapshot(const char* path,int w,int h){std::vector<unsigned char> pixels(w*h*4),flipped(pixels.size());glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());for(int row=0;row<h;++row)std::copy(pixels.begin()+row*w*4,pixels.begin()+(row+1)*w*4,flipped.begin()+(h-1-row)*w*4);png_image im{};im.version=PNG_IMAGE_VERSION;im.width=w;im.height=h;im.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&im,path,0,flipped.data(),0,nullptr));}
int main(){assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_TRUE);glfwWindowHint(GLFW_FOCUSED,GLFW_FALSE);glfwWindowHint(GLFW_FOCUS_ON_SHOW,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_CREATION_API,GLFW_EGL_CONTEXT_API);glfwWindowHint(GLFW_CLIENT_API,GLFW_OPENGL_ES_API);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,0);auto* window=glfwCreateWindow(854,480,"TCPE native disconnect readability fixture",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(TCPEGL3::initialize());Tesselator::instance.init();AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);Font font(&options,"font/default8.png",&textures);font.setJavaTextEnabled(true);auto* game=new GameProbe();game->textures=&textures;game->font=&font;game->javaSession=new JavaGameSession();
 const std::string reason=u8"§6§lSonar\n\n§cThere are too many players online with your IP address.\n§7If you believe that this is an error, contact an administrator.";
 auto draw=[&](DisconnectProbe& screen,int w,int h,const char* output){screen.Screen::init(game,w,h);glViewport(0,0,854,480);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,w,h,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable2(GL_DEPTH_TEST);glEnable2(GL_TEXTURE_2D);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);screen.render(-1,-1,0);glFinish();assert(glGetError()==GL_NO_ERROR);for(const auto& line:screen._lines)assert(font.widthJava(line)<=w-24);for(auto* b:screen.buttons)if(b->visible){assert(b->x>=0&&b->x+b->width<=w&&b->y>=0&&b->y+b->height<=h);}if(output)snapshot(output,854,480);};
 auto* normal=new DisconnectProbe(reason);draw(*normal,427,240,"../gles3-disconnection-reason-wrapped.png");assert(normal->_lines.size()>3);assert(!normal->_next->visible);bool gold=false;for(const auto& glyph:JavaRichText::parse(normal->_lines[0]))gold|=(glyph.color&0xffffff)==0xffaa00&&glyph.bold;assert(gold);
 std::string longReason=reason;for(int i=0;i<35;++i)longReason+=u8"\n§bDiagnostic section "+std::to_string(i)+u8" §x§1§2§3§4§5§6雪 — long-server-message-without-spaces-needs-safe-wrapping.";
 for(auto size:{std::pair<int,int>{427,240},{285,160},{200,160}}){auto* paged=new DisconnectProbe(longReason);draw(*paged,size.first,size.second,size.first==200?"../gles3-disconnection-reason-narrow-page.png":"../gles3-disconnection-reason-overflow-first.png");assert(paged->_next->visible&&paged->_next->active&&!paged->_previous->active);size_t pages=0;while(paged->_next->active){paged->buttonClicked(paged->_next);++pages;assert(pages<1000);}assert(pages>2&&paged->_previous->active&&paged->_page*paged->_pageLines<paged->_lines.size());paged->keyPressed(33);assert(paged->_next->active);paged->_page=pages;paged->updatePageButtons();glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);paged->render(-1,-1,0);glFinish();if(size.first==427)snapshot("../gles3-disconnection-reason-overflow-last.png",854,480);std::vector<uint32_t> rebuilt,expected;for(const auto& line:paged->_lines)for(const auto& g:JavaRichText::parse(line))rebuilt.push_back(g.codepoint);for(const auto& g:JavaRichText::parse(longReason))if(g.codepoint!='\n')expected.push_back(g.codepoint);assert(rebuilt==expected);}
 std::cout<<"GLES3 native disconnect: styled reason width427/285/200 bounds, complete multipage text preservation, native buttons, first/last page and RGB/Unicode PASS"<<std::endl;std::_Exit(0);
}
