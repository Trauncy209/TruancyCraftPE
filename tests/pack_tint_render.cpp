// Isolate inline model state from older linked GUI objects while testing the
// production header; full hotbar integration is tested after a coherent build.
#define JavaPackItemModels TintFixtureModels
#include "network/java/JavaPackItemModels.h"
#undef JavaPackItemModels
#include "AppPlatform_glfw.h"
#include <cassert>
#include <iostream>
int main(){
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);auto* w=glfwCreateWindow(160,96,"Tint pixels",nullptr,nullptr);assert(w);glfwMakeContextCurrent(w);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();Tesselator::instance.init();AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);
 TextureData data;data.w=data.h=1;data.numBytes=4;data.data=new unsigned char[4]{255,255,255,255};const auto texture=textures.assignTexture("tint-test",data);
 JavaGameSession::PackItemModel model;model.id="test:tint";JavaGameSession::PackModelQuad quad;quad.texture=0;quad.tint=0;quad.vertices={{{{0,0,8,0,1}},{{16,0,8,1,1}},{{16,16,8,1,0}},{{0,16,8,0,0}}}};model.quads.push_back(quad);JavaGameSession::PackModelTint tint;tint.kind="constant";tint.color=0x123456;model.tints.push_back(tint);JavaGameSession::Item item;
 auto draw=[&](){TintFixtureModels::install(model,{texture});glViewport(0,0,160,96);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(0,80,48,0,-100,100);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glEnable2(GL_TEXTURE_2D);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);assert(TintFixtureModels::render(&textures,model.id,20,20,16,&item));glFinish();assert(glGetError()==GL_NO_ERROR);unsigned char p[4]={};glReadPixels(50,45,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p);return uint32_t(p[0])<<16|uint32_t(p[1])<<8|p[2];};
 assert(draw()==0x123456);model.tints[0].kind="custom_model_data";model.tints[0].index=1;model.tints[0].defaultColor=0x345678;item.customModelData.present=true;item.customModelData.colors={0x0000ff,0xabcdef};assert(draw()==0xabcdef);item.customModelData.colors.resize(1);assert(draw()==0x345678);item.customModelData.present=false;assert(draw()==0x345678);model.tints[0].kind="unsupported";model.tints[0].defaultColor=0x112233;assert(draw()==0x112233);quad.tint=-1;model.quads={quad};assert(draw()==0xffffff);
 std::cout<<"Native GUI tint exactRGB: constant, indexed custom color, missing/default/unsupported and untinted white PASS"<<std::endl;std::_Exit(0);
}
