#include "headless_egl.h"
#include "client/Minecraft.h"
#include "client/renderer/TileRenderer.h"
#include "client/renderer/Tesselator.h"
#include "client/renderer/RenderList.h"
#include "world/level/LevelSource.h"
#include "world/level/storage/LevelData.h"
#include "world/level/LevelSettings.h"
#include "world/level/material/Material.h"
#include "world/level/tile/Tile.h"
#include "world/item/Item.h"
#include <map>
#include <tuple>
#include <iostream>
#include <cassert>
#include <cstdlib>
#include <png.h>
class World:public LevelSource{public:LevelData data;std::map<std::tuple<int,int,int>,int> blocks;World(int gen):data(LevelSettings(1,GameType::Creative,gen),"mesh regression",gen){}int getTile(int x,int y,int z)override{return blocks[{x,y,z}];}bool isEmptyTile(int x,int y,int z)override{return !getTile(x,y,z);}float getBrightness(int,int,int)override{return 1;}int getData(int,int,int)override{return 0;}const Material* getMaterial(int x,int y,int z)override{return getTile(x,y,z)?Material::stone:Material::air;}bool isSolidRenderTile(int x,int y,int z)override{return getTile(x,y,z)!=0;}bool isSolidBlockingTile(int x,int y,int z)override{return isSolidRenderTile(x,y,z);}BiomeSource* getBiomeSource()override{return nullptr;}Biome* getBiome(int,int)override{return nullptr;}LevelData* getLevelData()override{return &data;}};
static RenderChunk mesh(World& world,int x,int y,int z){auto& t=Tesselator::instance;t.begin();t.offset(-x,-y,-z);TileRenderer renderer(&world);renderer.setAtlasFilter(0);renderer.tesselateInWorld(Tile::rock,x,y,z);GLuint buffer;glGenBuffers(1,&buffer);auto rc=t.end(true,buffer);t.offset(0,0,0);return rc;}
static unsigned picture(RenderChunk rc){glViewport(0,0,64,64);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(-1,1,-1,1,-10,10);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glTranslatef2(.5,-.5,0);glRotatef2(180,0,1,0);glEnable2(GL_CULL_FACE);glCullFace(GL_BACK);glEnable2(GL_DEPTH_TEST);glDisable2(GL_TEXTURE_2D);glDisable2(GL_LIGHTING);glDisable2(GL_FOG);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);RenderList list;list.init(0,0,0);list.addR(rc);list.render();glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> pixels(64*64*4);glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());unsigned count=0;for(size_t i=0;i<pixels.size();i+=4)count+=pixels[i]||pixels[i+1]||pixels[i+2];png_image image{};image.version=PNG_IMAGE_VERSION;image.width=64;image.height=64;image.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&image,"mesh-north.png",0,pixels.data(),0,nullptr));return count;}
int main(){std::cout.setf(std::ios::unitbuf);NativeHeadlessEGL egl(64,64);Material::initMaterials();Tile::initTiles();Item::initItems();Tesselator::instance.init();bool pass=true;for(bool ao:{false,true}){Minecraft::useAmbientOcclusion=ao;for(int gen:{LGV_ORIGINAL,LGV_INFINITE})for(auto p:{std::make_tuple(19,8,0,0,0,-1),std::make_tuple(19,8,255,0,0,1),std::make_tuple(0,8,19,-1,0,0),std::make_tuple(255,8,19,1,0,0),std::make_tuple(19,0,19,0,-1,0)}){int x,y,z,dx,dy,dz;std::tie(x,y,z,dx,dy,dz)=p;World w(gen);w.blocks[{x,y,z}]=1;w.blocks[{x+dx,y+dy,z+dz}]=1;auto covered=mesh(w,x,y,z);assert(covered.vertexCount==30);w.blocks.erase({x+dx,y+dy,z+dz});auto broken=mesh(w,x,y,z);int expected=gen==LGV_INFINITE?36:30;std::cout<<"ao="<<ao<<" gen="<<gen<<" block="<<x<<","<<y<<","<<z<<" before="<<covered.vertexCount<<" afterBreak="<<broken.vertexCount<<" expected="<<expected<<std::endl;pass&=broken.vertexCount==expected;if(ao&&gen==LGV_INFINITE&&z==0){unsigned pixels=picture(broken);std::cout<<"Actual north mesh visible pixels="<<pixels<<std::endl;pass&=pixels>0;}}}std::cout<<(pass?"PASS":"FAIL")<<" actual TileRenderer boundary neighbor-break mesh, fixedworld control +AO on/off +y0 modern floor"<<std::endl;std::_Exit(pass?0:1);}
