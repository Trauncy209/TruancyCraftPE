#include "network/java/JavaDisplayRenderer.h"
#include "network/java/JavaGameSession.h"
#include "network/java/JavaLevel.h"
#include "client/renderer/entity/EntityRenderDispatcher.h"
#include "client/player/RemotePlayer.h"
#include "client/gui/Font.h"
#include "client/renderer/Textures.h"
#include "client/renderer/Tesselator.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "world/level/tile/Tile.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include "util/Mth.h"
#include "AppPlatform_glfw.h"
#include <png.h>
#include <cassert>
#include <iostream>
#include "network/java/JavaPlayerModel.h"
#include "client/renderer/TextureData.h"
int main(int argc,char**argv){
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);auto* window=glfwCreateWindow(854,480,"Modern player skin geometry",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();Mth::initMth();Tesselator::instance.init();
 AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);TextureData skin;skin.w=skin.h=64;skin.numBytes=64*64*4;skin.data=new unsigned char[skin.numBytes]();skin.transparent=true;
 auto paint=[&](int x0,int y0,int x1,int y1,unsigned color){for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x){int p=(y*64+x)*4;skin.data[p]=color>>16;skin.data[p+1]=color>>8;skin.data[p+2]=color;skin.data[p+3]=255;}};
 paint(0,0,32,16,0xffffff);paint(16,16,40,32,0xffff00);paint(40,16,56,32,0xff0000);paint(0,16,16,32,0x00ff00);paint(32,48,48,64,0x0000ff);paint(16,48,32,64,0xff00ff);
 assert(textures.loadTexture("fixture-skin")==Textures::InvalidId);textures.removeTexture("fixture-skin");textures.assignTexture("fixture-skin",skin);assert(textures.hasTexture("fixture-skin"));glViewport(0,0,854,480);glClearColor(.04,.06,.08,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(-3,3,-1.8,1.8,-10,10);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glEnable2(GL_TEXTURE_2D);glEnable2(GL_DEPTH_TEST);glEnable2(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.1);glDisable2(GL_CULL_FACE);glColor4f2(1,1,1,1);
 JavaPlayerModel classic(false),slim(true);assert(classic.texHeight==64&&slim.texHeight==64);assert(!classic.arm1.mirror&&!classic.leg1.mirror);assert(classic.cubes.size()==12&&slim.cubes.size()==12);
 for(int i=0;i<2;++i){textures.loadAndBindTexture("fixture-skin");glPushMatrix2();glTranslatef2(i?1.2f:-1.2f,1.f,0);glScalef2(1,-1,1);glRotatef2(-15,0,1,0);(i?slim:classic).render(nullptr,0,0,0,0,0,1.f/16);glPopMatrix2();}
 glFinish();std::vector<unsigned char> pixels(854*480*4),flipped(pixels.size());glReadPixels(0,0,854,480,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());for(int row=0;row<480;++row)std::copy(pixels.begin()+row*854*4,pixels.begin()+(row+1)*854*4,flipped.begin()+(479-row)*854*4);
 for(int x:{250,595}){int p=(70*854+x)*4;assert(flipped[p]>240&&flipped[p+1]>240&&flipped[p+2]>240);}
 int reds[2]={},blues[2]={};
 for(int half=0;half<2;++half){int red=0,blue=0,green=0,purple=0;double redX=0,blueX=0;for(int y=0;y<480;++y)for(int x=half*427;x<(half+1)*427;++x){int p=(y*854+x)*4;int r=flipped[p],g=flipped[p+1],b=flipped[p+2];if(r>200&&g<20&&b<20){red++;redX+=x;}if(b>200&&r<20&&g<20){blue++;blueX+=x;}if(g>200&&r<20&&b<20)green++;if(r>200&&b>200&&g<20)purple++;}assert(red>100&&blue>100&&green>100&&purple>100);assert(redX/red<blueX/blue);reds[half]=red;blues[half]=blue;}
 assert(reds[1]<reds[0]&&blues[1]<blues[0]);
 png_image image{};image.version=PNG_IMAGE_VERSION;image.width=854;image.height=480;image.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&image,argv[1],0,flipped.data(),0,nullptr));assert(glGetError()==GL_NO_ERROR);textures.removeTexture("fixture-skin");assert(!textures.hasTexture("fixture-skin"));textures.removeTexture("fixture-skin");assert(glGetError()==GL_NO_ERROR);std::cout<<"Modern skin distinct limbs and texture lifecycle render PASS\n";return 0;
}
