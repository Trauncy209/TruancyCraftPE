#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "client/renderer/gles.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <array>
#ifndef TCPE_GLES3
static void* loader(const char*n){return (void*)glfwGetProcAddress(n);}
#endif
int main(int argc,char**argv){assert(argc==2);assert(glfwInit());glfwWindowHint(GLFW_FOCUSED,GLFW_FALSE);
#ifdef TCPE_GLES3
 glfwWindowHint(GLFW_CONTEXT_CREATION_API,GLFW_EGL_CONTEXT_API);glfwWindowHint(GLFW_CLIENT_API,GLFW_OPENGL_ES_API);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,0);
#else
 glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);
#endif
 auto*w=glfwCreateWindow(360,240,"Native renderer parity",nullptr,nullptr);assert(w);glfwMakeContextCurrent(w);
#ifdef TCPE_GLES3
 assert(TCPEGL3::initialize());
#else
 assert(gladLoadGLES1Loader(loader));
#endif
 glViewport(0,0,360,240);glDisable(GL_DEPTH_TEST);glClearColor(.1,.1,.1,1);glClear(GL_COLOR_BUFFER_BIT);glMatrixMode(GL_PROJECTION);glLoadIdentity();float pr[]{2/360.f,0,0,0,0,-2/240.f,0,0,0,0,-.01f,0,-1,1,0,1};glLoadMatrixf(pr);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glEnableClientState(GL_VERTEX_ARRAY);float v[]{0,0,-10,80,0,-10,80,80,-10,0,0,-10,80,80,-10,0,80,-10};float uv[]{0,0,1,0,1,1,0,0,1,1,0,1};
 // Matrix order and state-query restoration, CPU vertex/color arrays.
 glPushMatrix();glTranslatef(15,15,0);glRotatef(7,0,0,1);glScalef(.8,.8,1);GLfloat mv[16];glGetFloatv(GL_MODELVIEW_MATRIX,mv);assert(mv[12]==15&&mv[13]==15);glVertexPointer(3,GL_FLOAT,0,v);glColor4f(.9,.2,.1,1);glDisable(GL_TEXTURE_2D);glDrawArrays(GL_TRIANGLES,0,6);glPopMatrix();
 // VBO vertex+normalized byte color, smooth interpolation.
 struct V{float xyz[3];unsigned char color[4];};V b[3]{{{110,15,-10},{255,0,0,255}},{{190,15,-10},{0,255,0,255}},{{150,95,-10},{0,0,255,255}}};GLuint buffer;glGenBuffers(1,&buffer);glBindBuffer(GL_ARRAY_BUFFER,buffer);glBufferData(GL_ARRAY_BUFFER,sizeof(b),b,GL_STATIC_DRAW);glVertexPointer(3,GL_FLOAT,sizeof(V),0);glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(V),(void*)12);glEnableClientState(GL_COLOR_ARRAY);glShadeModel(GL_SMOOTH);glDrawArrays(GL_TRIANGLES,0,3);glDisableClientState(GL_COLOR_ARRAY);glBindBuffer(GL_ARRAY_BUFFER,0);glVertexPointer(3,GL_FLOAT,0,v);
 // Alpha-tested textured quad, fixed-function modulate and alpha thresholds.
 GLuint tex;glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);unsigned char image[]{255,255,255,255,0,255,0,0,0,0,255,255,255,0,0,255};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,image);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glEnable(GL_TEXTURE_2D);glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.5);glTexCoordPointer(2,GL_FLOAT,0,uv);glEnableClientState(GL_TEXTURE_COORD_ARRAY);glColor4f(.6,.8,1,1);glPushMatrix();glTranslatef(230,15,0);glDrawArrays(GL_TRIANGLES,0,6);glPopMatrix();glDisable(GL_ALPHA_TEST);glDisable(GL_TEXTURE_2D);glDisableClientState(GL_TEXTURE_COORD_ARRAY);
 // Exact TCPE two-light setup with transformed direction and nonuniform scale.
 glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glEnable(GL_LIGHT1);glEnable(GL_COLOR_MATERIAL);glEnable(GL_NORMALIZE);float ambient[]{.4,.4,.4,1},diff[]{.6,.6,.6,1},zero[]{0,0,0,1},pos0[]{.16169042,.80845209,-.56591645,0},pos1[]{-.16169042,.80845209,.56591645,0};glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambient);glLightfv(GL_LIGHT0,GL_DIFFUSE,diff);glLightfv(GL_LIGHT1,GL_DIFFUSE,diff);glLightfv(GL_LIGHT0,GL_AMBIENT,zero);glLightfv(GL_LIGHT1,GL_AMBIENT,zero);glLightfv(GL_LIGHT0,GL_POSITION,pos0);glLightfv(GL_LIGHT1,GL_POSITION,pos1);glNormal3f(0,1,0);glColor4f(.8,.6,.2,1);glPushMatrix();glTranslatef(15,140,0);glRotatef(20,1,0,0);glScalef(1,.75,.5);glDrawArrays(GL_TRIANGLES,0,6);glPopMatrix();glDisable(GL_LIGHTING);
 // Linear and exponential fog.
 glEnable(GL_FOG);float fc[]{.2,.4,.6,1};glFogfv(GL_FOG_COLOR,fc);glFogf(GL_FOG_MODE,GL_LINEAR);glFogf(GL_FOG_START,0);glFogf(GL_FOG_END,20);glColor4f(1,.2,.1,1);glPushMatrix();glTranslatef(125,140,0);glDrawArrays(GL_TRIANGLES,0,6);glPopMatrix();glFogf(GL_FOG_MODE,GL_EXP2);glFogf(GL_FOG_DENSITY,.1);glPushMatrix();glTranslatef(235,140,0);glRotatef(15,0,1,0);glDrawArrays(GL_TRIANGLES,0,6);glPopMatrix();glDisable(GL_FOG);
 glFinish();auto error=glGetError();if(error)std::cerr<<"GL error "<<error<<std::endl;assert(!error);std::array<unsigned char,360*240*4>pixels;glReadPixels(0,0,360,240,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());std::ofstream out(argv[1],std::ios::binary);out<<"P6\n360 240\n255\n";for(int y=239;y>=0;y--)for(int x=0;x<360;x++)out.write((char*)&pixels[(y*360+x)*4],3);glfwSwapBuffers(w);std::cout<<glGetString(GL_VERSION)<<" matrix/client-array/VBO/alpha/textures/lights/fog PASS"<<std::endl;glfwDestroyWindow(w);}
