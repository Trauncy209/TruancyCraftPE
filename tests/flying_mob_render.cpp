#include "network/java/JavaFlyingMobModel.h"
#include "network/java/JavaIronGolemModel.h"
#include "client/renderer/entity/EntityRenderDispatcher.h"
#include "network/java/JavaEntityAdapter.h"
#include "network/java/JavaGameSession.h"
#include "network/java/JavaLevel.h"
#include "world/entity/player/Player.h"
#include "world/level/storage/MemoryLevelStorage.h"
#include "client/renderer/Tesselator.h"
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
int main(int argc,char**argv){
 assert(glfwInit());glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,2);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,1);auto* window=glfwCreateWindow(400,400,"Java flying creatures",nullptr,nullptr);assert(window);glfwMakeContextCurrent(window);assert(gladLoadGLES1Loader((GLADloadproc)winGLLoader));glPatchDesktopCompat();
 Mth::initMth();Material::initMaterials();MobCategory::initMobCategories();Tile::initTiles();Item::initItems();Biome::initBiomes();Tesselator::instance.init();
 AppPlatform_glfw platform;Options options;Textures textures(&options,&platform);
 assert(JavaFlyingMobModel::supports("Bee")&&JavaFlyingMobModel::supports("Allay")&&!JavaFlyingMobModel::supports("Player")&&!JavaFlyingMobModel::supports("Zombie"));
 JavaGameSession session;std::vector<uint16_t> air(4096,0);std::vector<uint8_t> sunlight(2048,255);uint8_t lightFlags=1;
 for(int x:{-1,0})for(int z:{-1,0}){TcpeJavaEvent chunk{};chunk.kind=TCPE_JAVA_CHUNK;chunk.x=x;chunk.z=z;chunk.min_y=0;chunk.height=16;chunk.states=air.data();chunk.states_len=air.size();chunk.sky_light=sunlight.data();chunk.sky_light_len=sunlight.size();chunk.light_sections=&lightFlags;chunk.light_sections_len=1;assert(session.chunks.apply(chunk));}
 JavaLevel level(new MemoryLevelStorage(),session.chunks);
 for(auto pair:{std::make_pair(1001,"Bee"),std::make_pair(1002,"Allay")}){JavaGameSession::Entity e{};e.id=pair.first;e.kind=pair.second;e.position={{0,10,0}};session.entities[e.id]=e;}
 JavaGameSession::Entity npc{};npc.id=1003;npc.kind="Player";npc.position={{2,10,0}};npc.profileName="NPC fixture";npc.profileSkinUrl="fixture-skin";npc.profileSkinSlim=true;npc.skinLayers=127;session.entities[npc.id]=npc;session.skinTextures["fixture-skin"]="mob/char.png";session.skinHeights["fixture-skin"]=64;
 JavaGameSession::Entity golem{};golem.id=1004;golem.kind="IronGolem";golem.position={{4,10,0}};session.entities[golem.id]=golem;
 synchronizeJavaEntities(&level,session);auto* golemEntity=static_cast<Level&>(level).getEntity(1004);assert(golemEntity&&golemEntity->getEntityTypeId()==125&&golemEntity->isPickable()&&golemEntity->isShootable());assert(std::abs(golemEntity->bbWidth-1.4f)<.0001f&&std::abs(golemEntity->bbHeight-2.7f)<.0001f);assert(std::string(JavaIronGolemModel::texture())=="mob/java-iron-golem.png");auto* player=static_cast<Level&>(level).getEntity(1003);assert(player&&player->isPlayer());bool slim=false;assert(javaPlayerSkinModel(player,slim)&&slim);assert(javaPlayerSkinLayers(player)==127);assert(static_cast<Player*>(player)->name=="NPC fixture");
 for(int id:{1001,1002}){auto* entity=static_cast<Level&>(level).getEntity(id);assert(entity&&entity->getEntityTypeId()==126);assert(entity->isPickable()&&entity->isShootable());assert(std::string(JavaFlyingMobModel::texture(session.entities[id].kind))==(id==1001?"mob/java-bee.png":"mob/java-allay.png"));assert(std::abs(entity->bbHeight-.6f)<.0001f);assert(std::abs(entity->bbWidth-(id==1001?.7f:.35f))<.0001f);}
 auto frame=[&](bool bee,float ticks){
  glViewport(0,0,400,400);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(-.85f,.85f,1.6f,-.1f,-4,4);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable2(GL_CULL_FACE);glDisable2(GL_DEPTH_TEST);glEnable2(GL_TEXTURE_2D);glEnable2(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.1f);glEnable2(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);textures.loadAndBindTexture(JavaFlyingMobModel::texture(bee?"Bee":"Allay"));glColor4f2(1,1,1,1);glRotatef2(25,0,1,0);JavaFlyingMobModel model(bee);model.renderFlying(ticks,0,0);glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> pixels(400*400*4);glReadPixels(0,0,400,400,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());return pixels;
 };
 auto bee=frame(true,0),allay=frame(false,0),beeMoving=frame(true,2),allayMoving=frame(false,8);assert(bee!=allay&&bee!=beeMoving&&allay!=allayMoving);
 auto golemFrame=[&](float phase,float speed,float attack,float health=100,int flower=0){
  glViewport(0,0,400,400);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(-1.5f,1.5f,1.65f,-1.5f,-4,4);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glEnable2(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glClearDepthf(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);textures.loadAndBindTexture(JavaIronGolemModel::texture());glColor4f2(1,1,1,1);glRotatef2(155,0,1,0);JavaIronGolemModel model;model.renderPose(phase,speed,15,5,attack,flower);if(const auto* cracks=JavaIronGolemModel::crackTexture(health,100)){glDepthFunc(GL_EQUAL);textures.loadAndBindTexture(cracks);model.renderPose(phase,speed,15,5,attack,flower);glDepthFunc(GL_LEQUAL);}if(flower)assert(model.renderFlower(&textures,1));glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> pixels(400*400*4);glReadPixels(0,0,400,400,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());return pixels;
 };
 auto golemPixels=golemFrame(0,0,0),walkingGolem=golemFrame(5,.8f,0),attackGolem=golemFrame(0,0,9);assert(golemPixels!=walkingGolem&&golemPixels!=attackGolem);size_t ironPixels=0,plantPixels=0;for(size_t i=0;i<golemPixels.size();i+=4){if(golemPixels[i]>120&&golemPixels[i+1]>120&&golemPixels[i+2]>120)++ironPixels;if(golemPixels[i+1]>golemPixels[i]*1.3&&golemPixels[i+1]>golemPixels[i+2]*1.3&&golemPixels[i+1]>50)++plantPixels;}assert(ironPixels>100&&plantPixels>10);
 assert(JavaIronGolemModel::crackTexture(75,100)==nullptr);assert(std::string(JavaIronGolemModel::crackTexture(74.99f,100)).find("low")!=std::string::npos);assert(std::string(JavaIronGolemModel::crackTexture(50,100)).find("low")!=std::string::npos);assert(std::string(JavaIronGolemModel::crackTexture(49.99f,100)).find("medium")!=std::string::npos);assert(std::string(JavaIronGolemModel::crackTexture(25,100)).find("medium")!=std::string::npos);assert(std::string(JavaIronGolemModel::crackTexture(24.99f,100)).find("high")!=std::string::npos);assert(JavaIronGolemModel::crackTexture(100,200)!=nullptr);assert(JavaIronGolemModel::crackTexture(10,0)==nullptr);
 auto lowCracks=golemFrame(0,0,0,70),mediumCracks=golemFrame(0,0,0,40),highCracks=golemFrame(0,0,0,20),flowerGolem=golemFrame(0,0,0,100,400);
 assert(golemPixels!=lowCracks&&lowCracks!=mediumCracks&&mediumCracks!=highCracks&&flowerGolem!=golemPixels);
 auto changed=[&](const std::vector<unsigned char>& pixels){size_t count=0;for(size_t i=0;i<pixels.size();i+=4)if(!std::equal(pixels.begin()+i,pixels.begin()+i+3,golemPixels.begin()+i))++count;return count;};assert(changed(lowCracks)>0&&changed(mediumCracks)>changed(lowCracks)&&changed(highCracks)>changed(mediumCracks));
 size_t flowerRed=0;for(size_t i=0;i<flowerGolem.size();i+=4)if(flowerGolem[i]>100&&flowerGolem[i]>flowerGolem[i+1]*1.8&&flowerGolem[i]>flowerGolem[i+2]*1.8)++flowerRed;assert(flowerRed>0);
 EntityRenderDispatcher dispatcher;dispatcher.textures=&textures;
 auto adapterFrame=[&](unsigned attack,unsigned flower,unsigned hurt,bool health,float maximum){
  auto& snapshot=session.entities[1004];snapshot.mobAttackTicks=attack;snapshot.mobFlowerTicks=flower;snapshot.hurtTicks=hurt;snapshot.hasLivingHealth=health;snapshot.livingHealth=80;snapshot.hasLivingMaxHealth=health;snapshot.livingMaxHealth=maximum;snapshot.yaw=0;snapshot.headYaw=0;snapshot.pitch=0;synchronizeJavaEntities(&level,session);
  glViewport(0,0,400,400);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrthof(-1.5f,1.5f,-.1f,3.05f,-4,4);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glRotatef2(-25,0,1,0);glEnable2(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);assert(renderJavaPresentation(golemEntity,dispatcher,0,0,0,0));glFinish();assert(glGetError()==GL_NO_ERROR);std::vector<unsigned char> pixels(400*400*4);glReadPixels(0,0,400,400,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());return pixels;
 };
 auto authoritativeIdle=adapterFrame(0,0,0,false,100);assert(authoritativeIdle==adapterFrame(0,0,10,false,100));assert(authoritativeIdle!=adapterFrame(10,0,0,false,100));assert(authoritativeIdle!=adapterFrame(0,400,0,false,100));assert(authoritativeIdle==adapterFrame(0,0,0,true,100));assert(authoritativeIdle!=adapterFrame(0,0,0,true,200));assert(authoritativeIdle==adapterFrame(0,0,0,false,100));
 if(argc==6){for(int i=0;i<5;++i){const auto& pixels=i==4?flowerGolem:i==3?highCracks:i==2?golemPixels:i?allay:bee;std::vector<unsigned char> flipped(pixels.size());for(int row=0;row<400;++row)std::copy(pixels.begin()+row*400*4,pixels.begin()+(row+1)*400*4,flipped.begin()+(399-row)*400*4);png_image image{};image.version=PNG_IMAGE_VERSION;image.width=image.height=400;image.format=PNG_FORMAT_RGBA;assert(png_image_write_to_file(&image,argv[i+1],0,flipped.data(),0,nullptr));}}
 size_t yellow=0,cyan=0;for(size_t i=0;i<bee.size();i+=4){if(bee[i]>100&&bee[i+1]>60&&bee[i+2]<bee[i+1]/2)++yellow;if(allay[i+1]>80&&allay[i+2]>100&&allay[i]<allay[i+2]/2)++cyan;}assert(yellow>100&&cyan>100);
 std::cout<<"Bee/Allay/IronGolem native models: exact kind mapping, canonical textures/hitboxes, Player NPC preservation, pickability, textured GPU pixels and flying/walk/attack/flower pose equations and progressive crack overlays PASS"<<std::endl;std::_Exit(0);
}
