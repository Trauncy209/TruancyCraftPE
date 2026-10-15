#include <fstream>
#include "network/java/JavaBlockModels.h"
#include "client/renderer/Chunk.h"
#include "client/renderer/RenderList.h"
#include "client/particle/JavaGustParticle.h"
#include "network/java/JavaIronGolemModel.h"
#include "client/renderer/entity/EntityRenderDispatcher.h"
#include "network/java/JavaEntityAdapter.h"
#include "network/java/JavaGameSession.h"
#include "network/java/JavaLevel.h"
#include "world/entity/player/Player.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "client/renderer/Tesselator.h"
#include "client/renderer/Textures.h"
#include "world/level/tile/Tile.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "util/Mth.h"
#include "AppPlatform_glfw.h"
#include <cassert>
#include <png.h>
#include <iostream>
extern "C" TcpeJavaSession* tcpe_java_connect(const char*,size_t,const char*,size_t,bool){return nullptr;}
extern "C" void tcpe_java_close(TcpeJavaSession*){}
extern "C" bool tcpe_java_command(TcpeJavaSession*,const char*,size_t){return true;}
int main(){assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_TRUE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);auto*w=glfwCreateWindow(854,480,"Live GPU grass replay",nullptr,nullptr);assert(w);glfwMakeContextCurrent(w);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);glViewport(0,0,854,480);glEnable2(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_TRUE);glDisable2(GL_BLEND);glShadeModel2(GL_FLAT);glEnable2(GL_TEXTURE_2D);glEnable2(GL_CULL_FACE);glClearColor(.4,.6,.9,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);textures.loadAndBindTexture("java-terrain.png");{GLfloat mv[]={0.680722713f,-0.643769741f,0.349538416f,0.0f,0.0f,0.477158755f,0.878817141f,0.0f,-0.732541144f,-0.598230779f,0.3248128f,0.0f,5.31278992f,9.40878296f,-11.6035156f,1.0f},pr[]={0.439130068f,0.0f,0.0f,0.0f,0.0f,0.781285584f,0.0f,0.0f,0.0f,0.0f,-1.00039065f,-1.0f,0.0f,0.0f,-0.100019537f,0.0f};glMatrixMode(GL_PROJECTION);glLoadMatrixf(pr);glMatrixMode(GL_MODELVIEW);glLoadMatrixf(mv);std::ifstream f("/home/truancy/Documents/Codex/2026-10-13/referenced-chatgpt-conversation-this-is-an/work/visible-gles3-211-render-audit3/tcpe-gles3-qa-vbo-77023-alpha0.bin",std::ios::binary|std::ios::ate);assert(f);size_t bytes=f.tellg();f.seekg(0);std::vector<char>b(bytes);f.read(b.data(),bytes);GLuint buffer;glGenBuffers2(1,&buffer);glBindBuffer2(GL_ARRAY_BUFFER,buffer);glBufferData2(GL_ARRAY_BUFFER,bytes,b.data(),GL_STATIC_DRAW);glEnableClientState2(GL_VERTEX_ARRAY);glEnableClientState2(GL_TEXTURE_COORD_ARRAY);glEnableClientState2(GL_COLOR_ARRAY);glVertexPointer2(3,GL_FLOAT,36,0);glTexCoordPointer2(2,GL_FLOAT,36,(void*)12);glColorPointer2(4,GL_UNSIGNED_BYTE,36,(void*)20);glDisable2(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.01f);glDrawArrays2(GL_TRIANGLES,0,bytes/36);}{GLfloat mv[]={0.680722713f,-0.643769741f,0.349538416f,0.0f,0.0f,0.477158755f,0.878817141f,0.0f,-0.732541144f,-0.598230779f,0.3248128f,0.0f,5.31278992f,9.40878296f,-11.6035156f,1.0f},pr[]={0.439130068f,0.0f,0.0f,0.0f,0.0f,0.781285584f,0.0f,0.0f,0.0f,0.0f,-1.00039065f,-1.0f,0.0f,0.0f,-0.100019537f,0.0f};glMatrixMode(GL_PROJECTION);glLoadMatrixf(pr);glMatrixMode(GL_MODELVIEW);glLoadMatrixf(mv);std::ifstream f("/home/truancy/Documents/Codex/2026-10-13/referenced-chatgpt-conversation-this-is-an/work/visible-gles3-211-render-audit3/tcpe-gles3-qa-vbo-77024-alpha1.bin",std::ios::binary|std::ios::ate);assert(f);size_t bytes=f.tellg();f.seekg(0);std::vector<char>b(bytes);f.read(b.data(),bytes);GLuint buffer;glGenBuffers2(1,&buffer);glBindBuffer2(GL_ARRAY_BUFFER,buffer);glBufferData2(GL_ARRAY_BUFFER,bytes,b.data(),GL_STATIC_DRAW);glEnableClientState2(GL_VERTEX_ARRAY);glEnableClientState2(GL_TEXTURE_COORD_ARRAY);glEnableClientState2(GL_COLOR_ARRAY);glVertexPointer2(3,GL_FLOAT,36,0);glTexCoordPointer2(2,GL_FLOAT,36,(void*)12);glColorPointer2(4,GL_UNSIGNED_BYTE,36,(void*)20);glEnable2(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.01f);glDrawArrays2(GL_TRIANGLES,0,bytes/36);}glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char>p(854*480*4),f(p.size());glReadPixels(0,0,854,480,GL_RGBA,GL_UNSIGNED_BYTE,p.data());for(int y=0;y<480;++y)std::copy(p.begin()+y*854*4,p.begin()+(y+1)*854*4,f.begin()+(479-y)*854*4);png_image image{};image.version=PNG_IMAGE_VERSION;image.width=854;image.height=480;image.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&image,"../grass-live-gpu-replay.png",0,f.data(),0,nullptr));glfwSwapBuffers(w);glfwPollEvents();std::cout<<"Live GPU VBO + actual uniform projection/modelview replay GLerror0"<<std::endl;std::_Exit(0);}
