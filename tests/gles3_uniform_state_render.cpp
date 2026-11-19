#include "client/renderer/GLES3Compat.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <vector>
#include <fstream>
#include <cassert>
#include <cstdio>
int main(int argc,char**argv){assert(argc>=1&&argc<=3);assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_CREATION_API,GLFW_EGL_CONTEXT_API);glfwWindowHint(GLFW_CLIENT_API,GLFW_OPENGL_ES_API);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,0);auto*w=glfwCreateWindow(160,160,"Shader equivalence",NULL,NULL);assert(w);glfwMakeContextCurrent(w);assert(TCPEGL3::initialize());
 const float vertices[]={-1,-1,0,1,-1,0,1,1,0,-1,-1,0,1,1,0,-1,1,0};const float uv[]={0,0,1,0,1,1,0,0,1,1,0,1};
 const float normals[]={0,0,1,0,0,1,0,0,1,0,0,1,0,0,1,0,0,1};GLuint tex=0;glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);const unsigned char rgba[]={255,100,40,255,30,200,255,80,60,255,10,200,220,40,255,255};glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 std::ofstream out;if(argc>1)out.open(argv[1],std::ios::binary);glViewport(0,0,160,160);glBindBuffer(GL_ARRAY_BUFFER,0);glVertexPointer(3,GL_FLOAT,0,vertices);glTexCoordPointer(2,GL_FLOAT,0,uv);glNormalPointer(GL_FLOAT,0,normals);glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_TEXTURE_COORD_ARRAY);glDisableClientState(GL_COLOR_ARRAY);
 for(int frame=0;frame<64;++frame){
  if(frame==32){TCPEGL3::resetContext();assert(TCPEGL3::initialize());glBindBuffer(GL_ARRAY_BUFFER,0);glVertexPointer(3,GL_FLOAT,0,vertices);glTexCoordPointer(2,GL_FLOAT,0,uv);glNormalPointer(GL_FLOAT,0,normals);glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_TEXTURE_COORD_ARRAY);glDisableClientState(GL_COLOR_ARRAY);}
  glClearColor(.07f,.11f,.13f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(-2,2,-2,2,-10,10);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glRotatef(frame*7.f,1,.3f,.2f);glScalef(.6f+frame%4*.3f,1.3f,.7f);glColor4f(.8f,.7f,.6f,.9f);
  float pos[]={float(frame%5)-2,2,3,float(frame%2)},diffuse[]={.6f,.4f,.2f,1},ambient[]={.13f,.18f,.21f,1};glLightfv(GL_LIGHT0,GL_POSITION,pos);glLightfv(GL_LIGHT0,GL_DIFFUSE,diffuse);glLightfv(GL_LIGHT1,GL_POSITION,pos);glLightfv(GL_LIGHT1,GL_AMBIENT,ambient);glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambient);
  if(frame%5==0)glDisable(GL_LIGHTING);else glEnable(GL_LIGHTING);if(frame%3)glEnable(GL_LIGHT0);else glDisable(GL_LIGHT0);if(frame%2)glEnable(GL_LIGHT1);else glDisable(GL_LIGHT1);if(frame%2)glEnable(GL_NORMALIZE);else glDisable(GL_NORMALIZE);if(frame%3)glEnableClientState(GL_NORMAL_ARRAY);else{glDisableClientState(GL_NORMAL_ARRAY);glNormal3f(.1f,.2f,1);}
  if(frame%2)glEnable(GL_TEXTURE_2D);else glDisable(GL_TEXTURE_2D);if(frame%3){glEnable(GL_FOG);glFogf(GL_FOG_MODE,GL_LINEAR);glFogf(GL_FOG_START,0);glFogf(GL_FOG_END,3);glFogfv(GL_FOG_COLOR,ambient);}else glDisable(GL_FOG);if(frame%4){glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.4f);}else glDisable(GL_ALPHA_TEST);
  glShadeModel(frame%7==0?GL_FLAT:GL_SMOOTH);glMatrixMode(GL_TEXTURE);glLoadIdentity();if(frame%3==0)glScalef(2,2,1);glMatrixMode(GL_MODELVIEW);glDrawArrays(GL_TRIANGLES,0,6);if(argc==3&&frame==5){GLint program=0;glGetIntegerv(GL_CURRENT_PROGRAM,&program);assert(glGetUniformLocation(program,"uLighting")==-1);assert(glGetUniformLocation(program,"uFlat")==-1);assert(glGetUniformLocation(program,"uTX")==-1);assert(glGetUniformLocation(program,"uTexture")>=0);}glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> pixels(160*160*4);glReadPixels(0,0,160,160,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());if(out.is_open())out.write((char*)pixels.data(),pixels.size());
 }
 if(out.is_open())assert(out);puts("64 lighting/normal/texture/fog/alpha state-transition frames rendered without GL errors");glfwDestroyWindow(w);glfwTerminate();
}
