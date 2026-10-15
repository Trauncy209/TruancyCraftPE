#include "network/java/JavaItemIcons.h"
#include "client/Minecraft.h"
#include "client/renderer/Tesselator.h"
#include "client/renderer/Textures.h"
#include "world/level/tile/Tile.h"
#include "world/level/material/Material.h"
#include "world/level/biome/Biome.h"
#include "world/entity/MobCategory.h"
#include "world/item/Item.h"
#include "util/Mth.h"
#include "AppPlatform_glfw.h"
#include <png.h>
#include <cassert>
#include <fstream>
#include <iostream>
extern "C" TcpeJavaSession* tcpe_java_connect(const char*,size_t,const char*,size_t,bool){return nullptr;}
extern "C" void tcpe_java_close(TcpeJavaSession*){}
extern "C" bool tcpe_java_command(TcpeJavaSession*,const char*,size_t){return true;}
extern "C" bool tcpe_java_account_status(TcpeJavaAccountStatus*){return false;}
extern "C" bool tcpe_java_account_action(uint32_t){return false;}
extern "C" bool tcpe_java_uses_account(const TcpeJavaSession*){return false;}
int main(){glfwInitHint(GLFW_PLATFORM,GLFW_PLATFORM_X11);assert(glfwInit());auto*w=glfwCreateWindow(800,480,"Actual Java1.21.11 native icons",nullptr,nullptr);assert(w);glfwMakeContextCurrent(w);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);
 auto projection=[&](){glViewport(0,0,800,480);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,800,480,0,2000,3000);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glTranslatef2(0,0,-2000);glDisable2(GL_LIGHTING);glDisable2(GL_FOG);glDisable2(GL_CULL_FACE);glDisable2(GL_DEPTH_TEST);glEnable2(GL_TEXTURE_2D);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);};
 std::vector<std::string> selected;for(const char*c:{"white","orange","magenta","light_blue","yellow","lime","pink","gray","light_gray","cyan","purple","blue","brown","green","red","black"}){selected.push_back("minecraft:"+std::string(c)+"_bed");selected.push_back("minecraft:"+std::string(c)+"_harness");}
 for(const char*t:{"wooden","stone","copper","iron","golden","diamond","netherite"})selected.push_back("minecraft:"+std::string(t)+"_spear");selected.push_back("minecraft:mace");
 projection();JavaGameSession::Item item;item.count=1;for(size_t i=0;i<selected.size();i++){item.resourceId=selected[i];if(!JavaItemIcons::has(item.resourceId))std::cout<<"Missing selected "<<item.resourceId<<std::endl;assert(JavaItemIcons::has(item.resourceId));assert(JavaItemIcons::render(&textures,item.resourceId,20+(i%8)*95,20+(i/8)*85,64,&item));}glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> p(800*480*4),flip(p.size());glReadPixels(0,0,800,480,GL_RGBA,GL_UNSIGNED_BYTE,p.data());for(int y=0;y<480;y++)std::copy(p.begin()+y*3200,p.begin()+(y+1)*3200,flip.begin()+(479-y)*3200);png_image image{};image.version=PNG_IMAGE_VERSION;image.width=800;image.height=480;image.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&image,"../native-new-items-proof.png",0,flip.data(),0,nullptr));glfwSwapBuffers(w);glfwPollEvents();
 projection();int blockIndex=0;for(const char*id:{"minecraft:oak_planks","minecraft:crafting_table","minecraft:lime_wool","minecraft:stone","minecraft:bricks","minecraft:glass","minecraft:oak_stairs","minecraft:oak_slab"}){item.resourceId=id;GLfloat before[16],after[16];glGetFloatv(GL_PROJECTION_MATRIX,before);assert(JavaItemIcons::render(&textures,id,30+(blockIndex%4)*190,30+(blockIndex/4)*200,150,&item));glGetFloatv(GL_PROJECTION_MATRIX,after);assert(std::equal(before,before+16,after));blockIndex++;}glFinish();glReadPixels(0,0,800,480,GL_RGBA,GL_UNSIGNED_BYTE,p.data());for(int y=0;y<480;y++)std::copy(p.begin()+y*3200,p.begin()+(y+1)*3200,flip.begin()+(479-y)*3200);assert(png_image_write_to_file(&image,"../native-sharp-block-icons-proof.png",0,flip.data(),0,nullptr));
 std::ifstream input("../java-official-item-definition-ids.txt");assert(input);std::ofstream missing("../native-item-gui-coverage-missing.txt");std::string id;size_t total=0,rendered=0;while(std::getline(input,id)){if(id.empty())continue;total++;projection();item.resourceId=id;const bool result=JavaItemIcons::render(&textures,id,0,0,32,&item);glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> pixels(32*32*4);glReadPixels(0,448,32,32,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());size_t visible=0;for(size_t i=3;i<pixels.size();i+=4)if(pixels[i])visible++;if(result&&visible)rendered++;else missing<<id<<" result="<<result<<" occupied="<<visible<<"\n";}
 missing.flush();assert(total==1506&&rendered==1505);std::cout<<"All16 authored beds,16harnesses,7spears and mace GUI render PASS; base GUI coverage "<<rendered<<"/"<<total<<" (remaining missing list explicit)"<<std::endl;std::_Exit(0);
}
