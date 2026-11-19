#include "client/renderer/RenderList.h"
#include "client/renderer/RenderChunk.h"
#include "client/renderer/Tesselator.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <vector>
#include <cassert>
#include <iostream>
struct Vertex{float x,y,z,u,v;unsigned color;float nx,ny,nz;};
static std::vector<unsigned char> read(){glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> p(128*128*4);glReadPixels(0,0,128,128,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;}
static void clear(){glClearColor(.1f,.2f,.3f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);}
int main(){assert(sizeof(Vertex)==size_t(VertexSizeBytes));assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_CREATION_API,GLFW_EGL_CONTEXT_API);glfwWindowHint(GLFW_CLIENT_API,GLFW_OPENGL_ES_API);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,0);auto*w=glfwCreateWindow(128,128,"Render list reuse",nullptr,nullptr);assert(w);glfwMakeContextCurrent(w);assert(TCPEGL3::initialize());glViewport(0,0,128,128);glDisable(GL_LIGHTING);glDisable(GL_FOG);glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);glDisableClientState(GL_NORMAL_ARRAY);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(-2,2,-2,2,-10,10);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
 const Vertex data[]={{-.5f,-.5f,0,0,0,0xff0000ff},{.5f,-.5f,0,0,0,0xff00ff00},{0,.5f,0,0,0,0xffff0000}};GLuint vbo=0;glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,sizeof(data),data,GL_STATIC_DRAW);RenderChunk a(vbo,3),b(vbo,3);a.pos.x=-.65f;b.pos.x=.65f;
 RenderList reused;
 for(int count:{2,1,0,2,0,1}){
  clear();for(int i=0;i<count;++i){auto& c=i?b:a;glPushMatrix();glTranslatef(c.pos.x,0,0);glBindBuffer(GL_ARRAY_BUFFER,vbo);glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_COLOR_ARRAY);glEnableClientState(GL_TEXTURE_COORD_ARRAY);glVertexPointer(3,GL_FLOAT,sizeof(Vertex),nullptr);glTexCoordPointer(2,GL_FLOAT,sizeof(Vertex),(void*)12);glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(Vertex),(void*)20);glDrawArrays(GL_TRIANGLES,0,3);glPopMatrix();}auto expected=read();
  clear();reused.init(0,0,0);for(int i=0;i<count;++i)reused.addR(i?b:a);reused.render();assert(read()==expected);
 }
 glDeleteBuffers(1,&vbo);glfwDestroyWindow(w);glfwTerminate();std::cout<<"Actual GPU reused terrain list matches direct draws across 2/1/empty transitions PASS\n";
}
