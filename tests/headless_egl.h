#ifndef TCPE_TEST_HEADLESS_EGL_H
#define TCPE_TEST_HEADLESS_EGL_H
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <cstdio>
#include <cstdlib>
#include "client/renderer/GLES3Compat.h"
// Offscreen only: no GLFW initialization, native window, display socket or input.
class NativeHeadlessEGL {
 EGLDisplay display=EGL_NO_DISPLAY;EGLSurface surface=EGL_NO_SURFACE;EGLContext context=EGL_NO_CONTEXT;
 static void require(bool ok,const char* stage){if(!ok){std::fprintf(stderr,"Headless EGL failure: %s (0x%x)\n",stage,eglGetError());std::abort();}}
public:
 NativeHeadlessEGL(int width,int height){
  auto platform=reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));require(platform!=nullptr,"platform function");display=platform(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,nullptr);require(display!=EGL_NO_DISPLAY,"surfaceless display");EGLint major,minor;require(eglInitialize(display,&major,&minor),"initialize");require(eglBindAPI(EGL_OPENGL_ES_API),"ES API");
  const EGLint attributes[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT_KHR,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_DEPTH_SIZE,24,EGL_NONE};EGLConfig config;EGLint count;require(eglChooseConfig(display,attributes,&config,1,&count)&&count>0,"config");const EGLint size[]={EGL_WIDTH,width,EGL_HEIGHT,height,EGL_NONE};surface=eglCreatePbufferSurface(display,config,size);require(surface!=EGL_NO_SURFACE,"pbuffer");const EGLint version[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};context=eglCreateContext(display,config,EGL_NO_CONTEXT,version);require(context!=EGL_NO_CONTEXT,"context");require(eglMakeCurrent(display,surface,surface,context),"make current");require(TCPEGL3::initialize(),"native ES3 renderer");
 }
 ~NativeHeadlessEGL(){TCPEGL3::resetContext();eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroyContext(display,context);eglDestroySurface(display,surface);eglTerminate(display);}
 NativeHeadlessEGL(const NativeHeadlessEGL&)=delete;NativeHeadlessEGL& operator=(const NativeHeadlessEGL&)=delete;
};
#endif
