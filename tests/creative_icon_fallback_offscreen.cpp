#include "headless_egl.h"
#include "client/Options.h"
#include "client/renderer/Textures.h"
#include "client/renderer/Tesselator.h"
#include "client/renderer/entity/ItemRenderer.h"
#include "world/level/material/Material.h"
#include "world/item/Item.h"
#include "world/item/ItemInstance.h"
#include "world/level/tile/Tile.h"
#include "AppPlatform_glfw.h"
#include <vector>
#include <iostream>
#include <cassert>
#include <cstdlib>
#include <png.h>
static std::vector<unsigned char> render(Textures& textures,ItemInstance& item,bool batch){glViewport(0,0,64,32);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,64,32,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable2(GL_DEPTH_TEST);glDisable2(GL_LIGHTING);glDisable2(GL_FOG);glEnable2(GL_TEXTURE_2D);glEnable2(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.1);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glColor4f2(1,1,1,1);glClearColor(.125,.25,.375,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);auto& t=Tesselator::instance;t.color(255,255,255);if(batch)t.beginOverride();ItemRenderer::renderGuiItem(nullptr,&textures,&item,4,4,16,16,false);assert(t.isOverridden()==batch);ItemInstance apple(Item::apple);t.color(255,255,255);ItemRenderer::renderGuiItem(nullptr,&textures,&apple,28,4,16,16,false);if(batch)t.endOverrideAndDraw();glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> p(64*32*4);glReadPixels(0,0,64,32,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;}
static void save(const std::vector<unsigned char>& p,const char* path){std::vector<unsigned char> flip(p.size());for(int y=0;y<32;++y)std::copy(p.begin()+y*256,p.begin()+(y+1)*256,flip.begin()+(31-y)*256);png_image image{};image.version=PNG_IMAGE_VERSION;image.width=64;image.height=32;image.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&image,path,0,flip.data(),0,nullptr));}
int main(){std::cout.setf(std::ios::unitbuf);NativeHeadlessEGL egl(64,32);Material::initMaterials();Tile::initTiles();Item::initItems();Tesselator::instance.init();Options options;AppPlatform_glfw platform;Textures textures(&options,&platform);bool pass=true;std::vector<Item*> icons;for(int id=256;id<512;++id)if(Item::items[id]){ItemInstance candidate(Item::items[id]);if(candidate.getIcon()>=0&&candidate.getIcon()<256&&ItemRenderer::getAtlasPos(&candidate)<0)icons.push_back(Item::items[id]);}icons.push_back(Item::apple);std::cout<<"native fallback sprites checked="<<icons.size()-1<<std::endl;for(auto* it:icons){ItemInstance item(it);auto plain=render(textures,item,false),batch=render(textures,item,true);unsigned diff=0,red=0;for(unsigned i=0;i<plain.size();i+=4){diff+=!std::equal(plain.begin()+i,plain.begin()+i+4,batch.begin()+i);red+=batch[i]>240&&batch[i+1]<10&&batch[i+2]<10;}std::cout<<"id="<<item.id<<" atlas="<<ItemRenderer::getAtlasPos(&item)<<" batch_vs_plain_different_pixels="<<diff<<" pure_red="<<red<<std::endl;if(it==Item::egg)save(batch,"../egg-batched.png");pass&=diff==0;}std::cout<<(pass?"PASS":"FAIL")<<" actual batched fallback matches direct icon; next mapped icon and override state preserved"<<std::endl;std::_Exit(pass?0:1);}
